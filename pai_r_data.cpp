// pai_r_data.cpp: implementation of the CUserData class.
//
//////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include <fcntl.h>
#include "daappconfigs.h"
#include "datools.h"
#include "sysfs.h"
//#include "tinyxml.h"
#include "pai_r_data.h"

#define DBG_PAIR_INIT		1 // DBG_MSG
#define DBG_PAIR_FUNC 	1 // DBG_MSG
#define DBG_PAIR_INFO		1 
#define DBG_PAIR_ERR  		DBG_ERR
#define DBG_PAIR_WRN		DBG_WRN

static std::mutex data_queue_lock_;

CUserData::CUserData()
{
	m_location_queue_init = false;

	m_operation_id = 0;
	memset((void *)&m_str_uuid, 0, sizeof(m_str_uuid));
	
	for(int ud_type = 0; ud_type < eUserDataType_End; ud_type++){
		if(ud_type == eUserDataType_Normal){
			hd[ud_type].max_count = LOCATION_QUEUE_MAX_COUNT;
			
			strcpy(hd[ud_type].data_path, DEF_LOCATION_DATA_PATH);
		}
		else if(ud_type == eUserDataType_Event) {
			hd[ud_type].max_count = EVENT_INFO_QUEUE_MAX_COUNT;
			
			strcpy(hd[ud_type].data_path, DEF_EVENT_FILE_INFO_PATH);
		}
		else{
		}

		hd[ud_type].in = 1;
		hd[ud_type].out = 1;
	
		hd[ud_type].offs_file_size = 0;
		hd[ud_type].init_size = 0;
	
		queue_backup_id.index[eQueueInOutType_In].id[ud_type] = 0;
		queue_backup_id.index[eQueueInOutType_Out].id[ud_type] = 0;
		queue_index_init = false;

		m_unique_local_autoid = 0;
		m_local_autoid_offset = 0;
	}
}

CUserData::~CUserData()
{
	Location_queue_deinit();
}

int CUserData::write_slot_data(FILE *fp, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size, u32 offset_pos)
{
	int ret = 0;
	u32 in_id = (pos % (max_pos));
	u32 write_pos = (in_id * size) + offset_pos;
	
	fseek(fp, write_pos , SEEK_SET);
	
	if(data_size)
		size = data_size;
	
	ret = fwrite(data, 1, size, fp);				

	if (ret != (int)size) {
		dbg_printf(DBG_PAIR_ERR, "write failed: %d(%s) , at(%d) ret = %d : %d\n", errno, strerror(errno), write_pos, ret, size);
		return 0;
	}
	
	//dbg_printf(DBG_PAIR_FUNC, "push %d (size : %d) \n", pos, size);

	return ret;
}

int CUserData::write_slot_data(const char * path, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size, u32 offset_pos)
{
	int ret = 0;
	FILE *fp = fopen(path, "rb+");

	if(fp){
		ret  = write_slot_data(fp, pos, max_pos, size, data, data_size, offset_pos);
		
		fclose(fp);
		
	} else {
		dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
	}

	return ret;
}


int CUserData::read_slot_data(FILE *fp, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size, u32 offset_pos)
{
	int ret = 0;
	u32 in_id = (pos % (max_pos));
	u32 read_pos = (in_id * size) + offset_pos;

	fseek(fp, read_pos,SEEK_SET);

	if(data_size)
		size = data_size;
	
	ret = fread(data, 1, size, fp);				


	if (ret != (int)size) {
		dbg_printf(DBG_PAIR_ERR, "read failed: %d(%s) ,at(%d) ret = %d : %d\n", errno, strerror(errno), read_pos, ret, size);
		return 0;
	}
	
	//dbg_printf(DBG_PAIR_FUNC, "pop %d \n", pos);	
	return ret;
}

int CUserData::read_slot_data(const char * path, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size, u32 offset_pos)
{
	int ret = 0;
	FILE *fp = fopen(path, "rb");

	if(fp){
		ret  = read_slot_data(fp, pos, max_pos, size, data, data_size, offset_pos);
		
		fclose(fp);
	} else {
		dbg_printf(DBG_PAIR_ERR, "%s open failed: %d(%s)\n", path, errno, strerror(errno));
	}
	
	return ret;
}



int CUserData::addLocation(PAI_R_LOCATION loc, eUserDataType ud_type, int pre_recording_time)
{
	std::lock_guard<std::mutex> _l(data_list_lock_);
	
	int i;
	PAI_R_DATA data;
	int speed_count = m_pre_speed_list.size();
	
	data.location = loc;
	data.speed_list.clear();
	
	for(i=0; i < DEF_MAX_CAMERA_COUNT; i++){
		data.thumbnail_data[i] = NULL;
		data.thumbnail_size[i] = 0;
	}
	
	if((ud_type && speed_count) || pre_recording_time){
		ITER_SPEED iSpeed = m_pre_speed_list.end();
		//ITER_SPEED iSpeedstart = m_pre_speed_list.begin();

		iSpeed--;

		if(pre_recording_time < 10) // //pre recording 10 + 20 sec
			pre_recording_time = 30;
		
		for( i = 0; i < speed_count; i++, iSpeed-- )
		{
			if(iSpeed->create_time < loc.create_time - pre_recording_time)
				break;

			data.speed_list.push_front(*iSpeed);
		}
	}

	if(m_data_list[ud_type].size() > PAI_R_DATA_MAX_COUNT){
		ITER_PAI_R_DATA iDataend = m_data_list[ud_type].end();
		iDataend--;

		for(int i = 0; i < DEF_MAX_CAMERA_COUNT; i++){
			if(iDataend->thumbnail_size[i] && iDataend->thumbnail_data[i])
				delete [] iDataend->thumbnail_data[i];
		}
		
		m_data_list[ud_type].pop_front();
		dbg_printf(DBG_PAIR_WRN, "%s(): data [%d] buffer full !!\r\n", __func__,  (int)ud_type);
	}

	m_data_list[ud_type].push_back(data);

	dbg_printf(DBG_PAIR_FUNC, "%s() : (lat:%.06f, Lng:%.06f) %s data list %d, speed list %d\r\n", __func__, data.location.latitude, data.location.longitude, ud_type ? "EVENT":"", m_data_list[ud_type].size(), data.speed_list.size());
	return 0;
}

int CUserData::addSpeed(PAI_R_SPEED_CSV spd, eUserDataType ud_type)
{	
	std::lock_guard<std::mutex> _l(data_list_lock_);
	
	for(int i = 0; i < (ud_type + 1); i++){
		if(m_data_list[i].size()){
			if(m_data_list[i].back().speed_list.size() > 240) // 0.5 * 120
				m_data_list[i].back().speed_list.pop_front();
			
			m_data_list[i].back().speed_list.push_back(spd);
		}
	}

	if(m_pre_speed_list.size() > 240) // 0.5 * 120
		m_pre_speed_list.pop_front();

	m_pre_speed_list.push_back(spd);

//	if(m_data_list[0].size())
//		dbg_printf(DBG_PAIR_FUNC, "%s() : %d\r\n", __func__, m_data_list[0].front().speed_list.size());


//	dbg_printf(DBG_PAIR_FUNC, "%s() : [e:%d] %d:%d\r\n", __func__, (int)is_event, m_data_list[0].size(), m_pre_speed_list.size());
	
	return 0;
}

int CUserData::addThumbnail(u8 * data, size_t size, eUserDataType ud_type)
{	
	int i = 0;

	std::lock_guard<std::mutex> _l(data_list_lock_);
	
	if(m_data_list[ud_type].size()){
		ITER_PAI_R_DATA iDataend = m_data_list[ud_type].end();
		iDataend--;

		for( i = 0; i < DEF_MAX_CAMERA_COUNT; i++){
			if(iDataend->thumbnail_size[i] ==0)
			{
				iDataend->thumbnail_data[i] = new u8[ size ];
				iDataend->thumbnail_size[i] = size;
				memcpy(iDataend->thumbnail_data[i], data, size);
				
				dbg_printf(DBG_PAIR_FUNC, "%s() : [e:%d] %d new %d\r\n", __func__, (int)ud_type, i, size);
				return 0;
			}
		}
	}

	dbg_printf(DBG_PAIR_FUNC, "%s() : [e:%d] %d list, count %d, skip %d\r\n", __func__, (int)ud_type, m_data_list[ud_type].size(), i, size);
	
	return 0;
}

bool CUserData::Location_queue_index_init(void)
{
	if(queue_index_init)
		return queue_index_init;
	
	const char * data_path = DEF_USER_DATA_IN_PATH;
	queue_backup_id.max_count = 0;

	for(int ud_type= 0; ud_type < eUserDataType_End; ud_type++){
		if(queue_backup_id.max_count < hd[ud_type].max_count)
			queue_backup_id.max_count = hd[ud_type].max_count;
	}

	if(!queue_backup_id.max_count){
		return 0;
	}

	int ret;
	
	u32 size = queue_backup_id.max_count *  sizeof(QUEUE_INDEX);
	char* dummy = new char[ size ];
		
	for(int in_out = 0; in_out < eQueueInOutType_End; in_out++){
		
		if(in_out == eQueueInOutType_Out)
			data_path = DEF_USER_DATA_OUT_PATH;
		
		if ( access(data_path, R_OK ) == 0) {
			int i;
			u32 last_idx = 0;
			u32 pos = 0;
			QUEUE_INDEX * q_idx = (QUEUE_INDEX *)dummy;
			
			ret = read_slot_data(data_path, 0, 1, size, (void *)q_idx);

			queue_backup_id.index[in_out].no = 0;
			for( i = 0; i < queue_backup_id.max_count; i++){
				//dbg_printf(DBG_PAIR_FUNC, "(%d) %u : %u, %u, %u \r\n", i, last_idx, q_idx[i].no, q_idx[i].id[0], q_idx[i].id[1]);
				if(last_idx < q_idx[i].no){					
					last_idx = q_idx[i].no;
					pos = i;
				}
				else if(last_idx || q_idx[i+1].no == 0)
					break;
			}
			dbg_printf(DBG_PAIR_FUNC, "(%d) %u : %u, %u, %u \r\n", i, last_idx, q_idx[i].no, q_idx[i].id[0], q_idx[i].id[1]);				

			for(int ud_type= 0; ud_type < eUserDataType_End; ud_type++){
				if(in_out == eQueueInOutType_In)
					hd[ud_type].in = queue_backup_id.index[in_out].id[ud_type] = q_idx[pos].id[ud_type]; 
				else
					hd[ud_type].out = queue_backup_id.index[in_out].id[ud_type] = q_idx[pos].id[ud_type]; 
			}

			queue_backup_id.index[in_out].no = last_idx + 1;
			dbg_printf(DBG_PAIR_FUNC, "%s() :  %s no %d [normal : %u, event : %u]\n", __func__, in_out ? "out":"in", queue_backup_id.index[in_out].no, queue_backup_id.index[in_out].id[0], queue_backup_id.index[in_out].id[1]);
			
			queue_index_init = true;
		}
		else {

				FILE *fp = fopen(data_path, "wb");
				dbg_printf(DBG_PAIR_FUNC, "%s fopen 0x%x\n", data_path, fp);

 				if(fp==NULL) {
					dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", data_path, errno, strerror(errno));
					queue_index_init = false;
					break;
				}
				
				memset((void *)dummy, 0x00, size);
				ret = write_slot_data(fp, 0, 1, size, (void *)dummy);
				fclose(fp);
				
				if(ret == size) {
					dbg_printf(DBG_PAIR_FUNC, "%s saved %d bytes.\n", data_path, size);

					queue_index_init = true;

					for(int ud_type= 0; ud_type < eUserDataType_End; ud_type++){
							hd[ud_type].in = 1;
							hd[ud_type].out = 1;
					}
							
					Location_queue_index_backup((eQueueInOutType)in_out);
					
				} else {
					queue_index_init = false;
					break;
				}
		}
	}

	delete [] dummy;
	
	return queue_index_init;
}

int CUserData::Location_queue_index_backup(eQueueInOutType InOut)
{
	if(!queue_index_init){
		if(!Location_queue_index_init())
			return 0;
	}

	bool index_backup = false;

	for(int ud_type = 0; ud_type < eUserDataType_End; ud_type++){
		u32 id;
		if(InOut == eQueueInOutType_In){
			 id = hd[ud_type].in;
		}
		else {
			 id = hd[ud_type].out;
			 
			 if(queue_backup_id.index[eQueueInOutType_In].id[ud_type]  < id){
			 	dbg_printf(DBG_PAIR_FUNC, "%s() : id error! %d (in %u : out %u)\r\n", __func__, queue_backup_id.index[InOut].no, queue_backup_id.index[eQueueInOutType_In].id[ud_type], id);

				id = queue_backup_id.index[InOut].id[ud_type];
			 }
		}
		
		if(queue_backup_id.index[InOut].id[ud_type] != id){
			queue_backup_id.index[InOut].id[ud_type] = id;
			index_backup = true;
		}
	}

	if(index_backup){
		int ret = 0;
		const char * data_path;
		if(InOut == eQueueInOutType_In)
			data_path = DEF_USER_DATA_IN_PATH;
		else
			data_path = DEF_USER_DATA_OUT_PATH;
		
		ret = write_slot_data(data_path, queue_backup_id.index[InOut].no, queue_backup_id.max_count, sizeof(QUEUE_INDEX), (void *)&queue_backup_id.index[InOut]);

		if(ret)
			queue_backup_id.index[InOut].no++;

#if 1
		if(InOut == eQueueInOutType_In){
			//dbg_printf(DBG_PAIR_FUNC, "%s() : unique local autoid %u\n", __func__, m_unique_local_autoid);
		}
		dbg_printf(DBG_PAIR_FUNC, "%s() : %s no %d [normal : %u, event : %u] [autoid %u]\n", __func__, InOut ? "out":"in", queue_backup_id.index[InOut].no, queue_backup_id.index[InOut].id[0], queue_backup_id.index[InOut].id[1], m_unique_local_autoid);
#else
		dbg_printf(DBG_PAIR_FUNC, "%s() :  %s no %d [normal : %u, event : %u]\n", __func__, InOut ? "out":"in", queue_backup_id.index[InOut].no, queue_backup_id.index[InOut].id[0], queue_backup_id.index[InOut].id[1]);
#endif
	}

	
	return 1;
}


int CUserData::Location_queue_init(void)
{
	std::lock_guard<std::mutex> _l(data_queue_lock_);
	//Folder check
	int retry = 0;
	u32 i;
	
	const char * system_path = DEF_SYSTEM_DIR;
	
	//const char * location_queue_in_fmt = "location queue in : %u\r\n";
	//const char * location_queue_out_fmt = "location queue out : %u\r\n";
	
	char szCmd[256];
	u32 file_size;
	if(m_location_queue_init)
		return 1;
	
SYSTEM_INIT:

	dbg_printf(DBG_PAIR_FUNC, "%s() : %d \n", __func__, retry);

	while(access(DA_FORMAT_FILE_NAME, R_OK ) == 0 || access(DA_FORMAT_FILE_NAME2, R_OK ) == 0){
		sleep(5);
	}
	
	if ( access(system_path, R_OK ) != 0) {
		sprintf(szCmd, "mkdir %s", system_path);
		system(szCmd);

		dbg_printf(DBG_PAIR_FUNC, "%s() : %s \n", __func__, szCmd);
	}

	if(retry++ >= 3){
		dbg_printf(DBG_PAIR_FUNC, "%s() : error !\n", __func__);
		return 0;
	}
	
	if ( access(system_path, R_OK ) != 0) {
		dbg_printf(DBG_PAIR_ERR, "%s() : error! (mkdir %s)\r\n", __func__, system_path);
		return 0;
	}

#if 0
	if(access("/tmp/offs.info", R_OK ) == 0){
		char buf[512] = { 0,};
		int info_size = sysfs_read("/tmp/offs.info", buf, sizeof(buf));
		
		if(info_size > 0){
			char info_str[128];
			int j = 0;

			dbg_printf(DBG_PAIR_INIT, " %s\n", buf);
			
			for( i = 0; i < info_size; i++){
				info_str[j] = buf[i];
					if ( info_str[j] == '\r' || info_str[j] == '\n') {
						u32 files = 0;
						info_str[j] = 0;

						if(sscanf(info_str, "folder1_files=%u", &files)){
							hd[eUserDataType_Normal].max_count = ((files + 63) / 32) * 32;
						}
						else if(sscanf(info_str, "folder2_files=%u", &files)){
							hd[eUserDataType_Event].max_count = ((files + 63) / 32) * 32;
							break;
						}
						
						j = 0;
					}
					else if(j < sizeof(info_str))
						j++;					
			}

			dbg_printf(DBG_PAIR_INIT, " SD CARD (max queue : %u, %u)\n", hd[eUserDataType_Normal].max_count, hd[eUserDataType_Event].max_count);
		}
	}

#else
	// sd space check
	//cat /sys/block/mmcblk0/device/name
	//SA64G
	u32 sd_size = 0;
	if(sysfs_scanf("/sys/block/mmcblk0/size", "%u", &sd_size)){
		if(sd_size > 134217728){ // 128G
			hd[eUserDataType_Normal].max_count = LOCATION_QUEUE_MAX_COUNT * 4;
			hd[eUserDataType_Event].max_count = EVENT_INFO_QUEUE_MAX_COUNT * 2;
			dbg_printf(DBG_PAIR_FUNC, " 128G");
		}
		else if(sd_size > 67108864) { //64G
			hd[eUserDataType_Normal].max_count = LOCATION_QUEUE_MAX_COUNT * 2;
			dbg_printf(DBG_PAIR_FUNC, " 64G");
		}
		else if(sd_size > 33554432){ // 32G
			dbg_printf(DBG_PAIR_FUNC, " 32G");
		}
		else if(sd_size <= 16777216) { //8G
			hd[eUserDataType_Normal].max_count = LOCATION_QUEUE_MAX_COUNT / 2;
			hd[eUserDataType_Event].max_count = EVENT_INFO_QUEUE_MAX_COUNT / 2;
			dbg_printf(DBG_PAIR_FUNC, " 8G or less than 8G");
		}
		dbg_printf(DBG_PAIR_INIT, " SD CARD (max queue : %u, %u)\n", hd[eUserDataType_Normal].max_count, hd[eUserDataType_Event].max_count);
	}
#endif

	Location_queue_index_init();
	
	for(int ud_type = 0; ud_type < eUserDataType_End; ud_type++){
		const char *data_path = (const char *)hd[ud_type].data_path;
		
		if ( access(data_path, R_OK ) != 0) {

#if 0
			char cmd[512];
			int length;
			u32 start_time = get_tick_count() / 1000;
			
			//dd if=/dev/zero of=filename bs=1 count=0 seek=200K
			sprintf(cmd, "dd if=/dev/zero of=%s bs=%u count=%u", data_path, hd[ud_type].max_count, sizeof(PAI_R_BACKUP_DATA));
			//sprintf(cmd, "fallocate -l %u %s", hd[ud_type].max_count * sizeof(PAI_R_BACKUP_DATA), data_path);
			
			system( cmd );

			length = sysfs_getsize(data_path);
			dbg_printf(DBG_PAIR_FUNC, "%s saved %d bytes. (save time %d)\n", data_path, length, (get_tick_count() / 1000) - start_time);
#else
			FILE *fp = fopen(data_path, "wb");
			dbg_printf(DBG_PAIR_FUNC, "%s fopen 0x%x\n", data_path, fp);

			queue_index_init = false;
			
			if(fp) {
				int i = 0, length = 0;
				u32 size = sizeof(PAI_R_BACKUP_DATA) * 4;
				char* dummy = new char[  size ];

				memset((void *)dummy, 0xff, size);
#if 1				
				fseek(fp, sizeof(PAI_R_BACKUP_DATA) * (hd[ud_type].max_count-4), SEEK_SET);
				fwrite((void *)dummy, 1, size, fp);
#else				
//				for( i = 0 ; i < hd[ud_type].max_count; i++) {
					fwrite((void *)dummy, 1, size, fp);
					length += size;
//				}
#endif

				delete [] dummy;

				fclose(fp);
				dbg_printf(DBG_PAIR_FUNC, "%s saved %d : %d bytes.\n", data_path, sizeof(PAI_R_BACKUP_DATA) * hd[ud_type].max_count, sysfs_getsize(data_path));
				
			} else {
				sprintf(szCmd, "rm -rf %s", system_path);
				system(szCmd);

				dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", data_path, errno, strerror(errno));
			}
#endif

			hd[ud_type].out = hd[ud_type].in = 1;

			for(int in_out = 0; in_out < eQueueInOutType_End ;in_out++){
				Location_queue_index_backup((eQueueInOutType)in_out);
			}
			
			goto SYSTEM_INIT;
		}
		else {
			u32 seek_pos = (hd[ud_type].in % hd[ud_type].max_count) * sizeof(PAI_R_BACKUP_DATA);
			
			file_size = sysfs_getsize(data_path);

			if(file_size < hd[ud_type].max_count * sizeof(PAI_R_BACKUP_DATA))
				hd[ud_type].init_size = file_size;
			else
				hd[ud_type].init_size = 0;
			

			//dbg_printf(DBG_PAIR_ERR, "#### \n %s file size %u:%u (retry:%d) \n### \n", data_path, file_size, seek_pos, retry);

			if(file_size < hd[ud_type].max_count * sizeof(PAI_R_BACKUP_DATA) &&  file_size < seek_pos){
				if(file_size + sizeof(PAI_R_BACKUP_DATA) == seek_pos){
					hd[ud_type].in -= 1;
					Location_queue_index_backup(eQueueInOutType_In);
					dbg_printf(DBG_PAIR_ERR, "%s file size error ? %u:%u bytes.(%d) \n", data_path, file_size, seek_pos, retry);
				}
				else {
					//file error			
					dbg_printf(DBG_PAIR_ERR, "%s file size %u:%u bytes. error! %d \n", data_path, file_size, seek_pos, retry);
					
					//sprintf(szCmd, "rm %s", data_path);
					//system(szCmd);

					sprintf(szCmd, "rm -rf %s", system_path);
					system(szCmd);

					queue_index_init = false;
					
					goto SYSTEM_INIT;
				}
			}

			dbg_printf(DBG_PAIR_FUNC, "%s file size %u:%u bytes.\n", data_path, file_size, seek_pos);
		}

		Location_queue_init_file((eUserDataType)ud_type);
		
		dbg_printf(DBG_PAIR_INIT, "%s() : queue[%d] in : %u, out : %u, count : %u\r\n", __func__, ud_type, hd[ud_type].in, hd[ud_type].out, hd[ud_type].in - hd[ud_type].out);
	}

#if 1
	if( m_unique_local_autoid < hd[eUserDataType_Normal].in) {
		m_unique_local_autoid =hd[eUserDataType_Normal].in;
	}
	
	m_local_autoid_offset = m_unique_local_autoid - hd[eUserDataType_Normal].in;

	dbg_printf(DBG_PAIR_FUNC, "%s() : unique local autoid %u (offset : %d)\n", __func__, m_unique_local_autoid, m_local_autoid_offset);
#endif

	m_location_queue_init = true;
	return 1;
}

void CUserData::Location_queue_init_file(eUserDataType ud_type)
{
	if(hd[ud_type].init_size){
		u32 size = sizeof(PAI_R_BACKUP_DATA) * 4;
		char* dummy = new char[  size ];
		
		memset((void *)dummy, 0x00, size);
		

		if(hd[ud_type].init_size && hd[ud_type].init_size < hd[ud_type].max_count * sizeof(PAI_R_BACKUP_DATA)){
			write_slot_data(hd[ud_type].data_path, hd[ud_type].init_size / size, hd[ud_type].max_count, size, dummy);
			
			hd[ud_type].init_size += size;
		}
		else {
			hd[ud_type].init_size = 0;
		}

		delete [] dummy;
	}
}

int CUserData::Location_queue_deinit(bool backup_need)
{
	std::lock_guard<std::mutex> _l(data_queue_lock_);
//	const char * system_path = DEF_SYSTEM_DIR;
	
	if(!m_location_queue_init){
		return 0;
	}

	if(backup_need){
		for(int in_out = 0; in_out < eQueueInOutType_End; in_out++)
			Location_queue_index_backup((eQueueInOutType)in_out);
	}
	
	for(int ud_type = 0; ud_type < eUserDataType_End; ud_type++){
		u32 count = Location_queue_count((eUserDataType)ud_type);
		
		dbg_printf(DBG_PAIR_INIT, "%s() : queue[%d] in : %u, out : %u, count : %u\r\n", __func__, ud_type, hd[ud_type].in, hd[ud_type].out, count);
	}
	m_location_queue_init = false;

	return 1;
}

int CUserData::Location_queue_count(eUserDataType ud_type){ 
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}
	
	if(hd[ud_type].in < hd[ud_type].out){
		m_location_queue_init = 0;
		queue_index_init = 0;
		dbg_printf(DBG_PAIR_ERR, "%s() : error! in %u < out %u\n", __func__, hd[ud_type].in, hd[ud_type].out);
		return 0;
	}
		
	return hd[ud_type].in - hd[ud_type].out; 
}

int CUserData::Location_queue_clear(u32 count, eUserDataType ud_type)
{ 
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}

	std::lock_guard<std::mutex> _l(data_queue_lock_);
	
	if(Location_queue_count(ud_type) < (int)count)
		count = (u32)Location_queue_count(ud_type);

	hd[ud_type].out += count;

	dbg_printf(DBG_PAIR_FUNC, "%s() :  queue[%d] in : %u, out : %u, count : %u => %u\r\n", __func__, ud_type, hd[ud_type].in, hd[ud_type].out, Location_queue_count(ud_type) + count, Location_queue_count(ud_type));
	return 1; 
}

int CUserData::Location_queue_clear_idx(u32 out_idx, eUserDataType ud_type)
{ 
	
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}

	std::lock_guard<std::mutex> _l(data_queue_lock_);
	
	out_idx += 1;
	
	if(hd[ud_type].in >= out_idx && hd[ud_type].out < out_idx){
		u32 count = out_idx - hd[ud_type].out;
		hd[ud_type].out = out_idx;
		dbg_printf(DBG_PAIR_FUNC, "%s() :  queue[%d] in : %u, out : %u, count : %u => %u\r\n", __func__, ud_type, hd[ud_type].in, hd[ud_type].out, Location_queue_count(ud_type) + count, Location_queue_count(ud_type));
	}
	else {
		dbg_printf(DBG_PAIR_FUNC, "%s() :  %u is out idx out of range!( queue[%d] in : %u, out : %u, count : %u)\r\n", __func__, out_idx, ud_type, hd[ud_type].in, hd[ud_type].out, Location_queue_count(ud_type));
	}
	return 1; 
}

int CUserData::Location_push_back(PAI_R_BACKUP_DATA *pLoc, eUserDataType ud_type)
{
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}

	std::lock_guard<std::mutex> _l(data_queue_lock_);
	
	u32 in_id = (hd[ud_type].in % (hd[ud_type].max_count));

	FILE *fp = fopen(hd[ud_type].data_path, "rb+");

	if(fp){
		int ret;
		
		hd[ud_type].in++;
		if(Location_queue_count(ud_type) >= (int)hd[ud_type].max_count){
			hd[ud_type].out = hd[ud_type].in - (hd[ud_type].max_count - 1);
			dbg_printf(DBG_PAIR_FUNC, "%s() : queue full! : deleted data. \r\n", __func__);
		}
		
		fseek(fp, in_id * sizeof(PAI_R_BACKUP_DATA),SEEK_SET);
		ret = fwrite((void *)pLoc, 1, sizeof(PAI_R_BACKUP_DATA), fp);

		dbg_printf(DBG_PAIR_FUNC, "%s push %d (size : %d:%d)\n", hd[ud_type].data_path, in_id, ret, sizeof(PAI_R_BACKUP_DATA));

		//fflush(fp);
		fclose(fp);
		
	} else {
		m_location_queue_init = false;
		dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", hd[ud_type].data_path, errno, strerror(errno));
	}
	
	return hd[ud_type].in;
}

u32 CUserData::Location_data_list_size(eUserDataType ud_type)
{
	std::lock_guard<std::mutex> _l(data_list_lock_);
	
	return m_data_list[ud_type].size();
}

int CUserData::BackupLocation(eUserDataType ud_type)
{
	PAI_R_BACKUP_DATA data;
	
	int data_count = m_data_list[ud_type].size();
	
	if(data_count)
	{
		std::lock_guard<std::mutex> _l(data_list_lock_);
		
		
		ITER_PAI_R_DATA iData = m_data_list[ud_type].begin();

		memset((void *)&data, 0, sizeof(data));

		data.operation_id = m_operation_id;
		strcpy(data.operation_uuid, m_str_uuid);
		
		if(ud_type)
			data.event_autoid = hd[eUserDataType_Event].in;	// 이벤트 동영상 자동 ID
		else
			data.event_autoid = 0;		
		
		data.location.autoid = hd[eUserDataType_Normal].in;	//로컬 동영상 자동 ID

		data.location.unique_local_autoid = m_unique_local_autoid++;

		strcpy(data.location.file_path, iData->location.file_path); //" /mnt/extsd/NORMAR/20001130_152107_I2.avi"

		if(iData->location.move_filesize) {
			data.location.move_filesize= iData->location.move_filesize; // / 1024; //KByte => Byte
		}
		else{
			if(hd[ud_type].offs_file_size == 0)
				hd[ud_type].offs_file_size = sysfs_getsize(data.location.file_path); //  / 1024; //KByte => Byte

			data.location.move_filesize= hd[ud_type].offs_file_size;
		}

		for( int j=0 ; j < DEF_MAX_CAMERA_COUNT; j++){
			if(iData->thumbnail_size[j] && iData->thumbnail_data[j]) {
				if(iData->thumbnail_size[j] < DEF_MAX_THUMBNAIL_DATA_SIZE)
					memcpy((void *)data.location.thumbnail.data[j], (void *)iData->thumbnail_data[j], iData->thumbnail_size[j]); // 360 x 180 jpeg
				else
					dbg_printf(DBG_PAIR_ERR, "%s() : Error!! thumbnail buffer size.\r\n", __func__);

				dbg_printf(DBG_PAIR_FUNC, "%s() : [e:%d] delete %d ( 0x%08x ...)\r\n", __func__, (int)ud_type, iData->thumbnail_size[j], *((int *)&iData->thumbnail_data[j][0]));
				delete [] iData->thumbnail_data[j];
			}
			else if(ud_type && j == 0){
				FILE* file=  fopen( "/tmp/snapshot-event.jpg", "r");
				
				if(file){
					int ret =  fread( (void *)data.location.thumbnail.data[j], 1, DEF_MAX_THUMBNAIL_DATA_SIZE, file );
					if (ret) {
						iData->thumbnail_size[j] = ret;
						dbg_printf(DBG_PAIR_FUNC, "%s() : [e:%d] snapshot-event %d ( 0x%08x ...)\r\n", __func__, (int)ud_type, iData->thumbnail_size[j], *((int *)&data.location.thumbnail.data[j][0]));
					}
					fclose(file);	
				}
			}
						
			data.location.thumbnail.size[j]= iData->thumbnail_size[j];
		}
	
		data.location.latitude = iData->location.latitude;
		data.location.longitude = iData->location.longitude;
		data.location.accurate = iData->location.accurate;
		data.location.direction = iData->location.direction; //0~359
		data.location.altitude = iData->location.altitude;

		data.location.create_time_ms = iData->location.create_time_ms;


		memset((void *)data.speed_list, 0, sizeof(data.speed_list));
		int speed_count = iData->speed_list.size();
			
		if(speed_count){
			ITER_SPEED iSpeed = iData->speed_list.begin();
			//ITER_SPEED iSpeedend = iData.speed_list.end();

			for( int j=0 ; j < speed_count; j++, iSpeed++ )
			{
				if(j < DEF_BACKUP_SPEED_LIST_MAX_COUNT)	{	
					data.speed_list[j].create_time = iSpeed->create_time;
					data.speed_list[j].create_time_ms = iSpeed->create_time_ms;
				
					data.speed_list[j].speed = iSpeed->speed;
					data.speed_list[j].distance = iSpeed->distance;
				}				
			}

			iData->speed_list.clear();
		}

		//m_data_list[ud_type].pop_back();
		//m_data_list[ud_type].pop_front();
		m_data_list[ud_type].erase(iData);
	}

	if(data_count){
		Location_push_back(&data, eUserDataType_Normal);

		if(ud_type){
			Location_push_back(&data, eUserDataType_Event);
			dbg_printf(DBG_PAIR_FUNC , "%s() : %u [event in %u]\r\n", __func__, hd[eUserDataType_Normal].in, hd[eUserDataType_Event].in);
		}
//		else
//			dbg_printf(DBG_PAIR_FUNC , "%s() : %u \r\n", __func__, hd[eUserDataType_Normal].in);


		Location_queue_index_backup(); //슈퍼캡 사용시 제거
		return 1;
	}
	
	return 0;
}

u32 CUserData::Location_queue_fiarst_autid(eUserDataType ud_type)
{
	u32 count = Location_queue_count(ud_type);
	
	if(count > (hd[ud_type].max_count - 5)){ //hd[ud_type].max_count - 5
		count = count - (hd[ud_type].max_count - 5);
		
		dbg_printf(DBG_PAIR_WRN, "%s() : queue skip! : %d \r\n", __func__, count);
		Location_queue_clear(count, ud_type);
	}
	
	return hd[ud_type].out;
}

int CUserData::update_upload_time(u32 auto_id, time_t upload_time, eUserDataType ud_type)
{
	u32 id = 0;
	
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}

	if(auto_id >= hd[ud_type].in || \
		((hd[ud_type].in > hd[ud_type].max_count) && (auto_id <= (hd[ud_type].in - hd[ud_type].max_count)))){
		dbg_printf(DBG_PAIR_ERR, "%s() : id error!(%d:%d)\n", __func__, hd[ud_type].in, auto_id);
		return 0;
	}
	
	
	const char * data_path = DEF_LOCATION_DATA_PATH;
	u32		event_autoid = 0;
	int 		ret = 0;
	
	if(ud_type == eUserDataType_Event)
		data_path = DEF_EVENT_FILE_INFO_PATH;

	FILE *fp = fopen(data_path, "rb+");

	id = (auto_id % hd[ud_type].max_count);
	
	if(fp){
		std::lock_guard<std::mutex> _l(data_queue_lock_);
		
		fseek(fp, id  * sizeof(PAI_R_BACKUP_DATA),SEEK_SET);

		if(upload_time == 0)
			upload_time = time(0);
		
		fwrite((void *)&upload_time, 1, sizeof(upload_time), fp);

		ret = fread( (void *)&event_autoid, 1, sizeof(event_autoid), fp );
		
		fclose(fp);
			
		dbg_printf(DBG_PAIR_FUNC, "%s() :  %s %d (%s) \n", __func__, ud_type ? "EVENT":"",  id, make_time_string(upload_time).c_str());
	} else {
		m_location_queue_init = false;
		dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", data_path, errno, strerror(errno));
	}
	
	//EVENT
	if(ud_type == eUserDataType_Normal && ret  == sizeof(event_autoid) && event_autoid){
		update_upload_time(event_autoid, time(0), eUserDataType_Event);				
	}
	
	return auto_id;
}

int CUserData::update_file_hash(u32 auto_id, const char * hash, eUserDataType ud_type)
{
	u32 id = 0;
	//int ret;
	
	if(!m_location_queue_init){
		if(Location_queue_init() == 0)
			return 0;
	}

	if(auto_id >= hd[ud_type].in || \
		((hd[ud_type].in > hd[ud_type].max_count) && (auto_id <= (hd[ud_type].in - hd[ud_type].max_count)))){
		dbg_printf(DBG_PAIR_ERR, "%s() : id error!(%d:%d)\n", __func__, hd[ud_type].in, auto_id);
		return 0;
	}
	
	const char * data_path = DEF_LOCATION_DATA_PATH;
	u32		event_autoid = 0;
	int 		ret= 0;
	
	if(ud_type == eUserDataType_Event)
		data_path = DEF_EVENT_FILE_INFO_PATH;
	
	FILE *fp = fopen(data_path, "rb+");

	id = (auto_id % hd[ud_type].max_count);
	
	if(fp){
		std::lock_guard<std::mutex> _l(data_queue_lock_);
		fseek(fp, (id  * sizeof(PAI_R_BACKUP_DATA)) + sizeof(PAI_R_BACKUP_DATA::upload_time), SEEK_SET);

		ret = fread( (void *)&event_autoid, 1, sizeof(event_autoid), fp );

		fwrite((void *)hash, 1, strlen(hash)+1, fp);				

		fclose(fp);
		
		dbg_printf(DBG_PAIR_FUNC, "%s() :  %s %d (%s) \n", __func__, ud_type ? "EVENT":"",  id, hash);
	} else {
		m_location_queue_init = false;
		dbg_printf(DBG_PAIR_ERR, "%s creation failed: %d(%s)\n", data_path, errno, strerror(errno));
	}

	//EVENT
	if(ud_type == eUserDataType_Normal && ret  == sizeof(event_autoid) && event_autoid){
		update_file_hash(event_autoid, hash, eUserDataType_Event);
	}
	
	return auto_id;
}

int CUserData::update_operation_id(u32 operation_id, const char * uuid)
{
	m_operation_id = operation_id;
	strcpy(m_str_uuid, uuid);

	return 0;
}

//========================================================

CPai_r_data::CPai_r_data()
{
	m_queue_index_init = false;
	m_tmp_pop_count = 0;
	m_local_auto_id_offset = 0;
	
	m_read_fp = NULL;

	memset((void *)&hd, 0, sizeof(hd));
	strcpy(hd[eUserDataType_Normal].data_path, DEF_LOCATION_DATA_PATH);
	strcpy(hd[eUserDataType_Event].data_path, DEF_EVENT_FILE_INFO_PATH);
}

CPai_r_data::~CPai_r_data()
{
}

int CPai_r_data::Location_queue_count(eUserDataType ud_type)
{ 
	if(!hd[ud_type].max_count){
		return 0;
	}
	
	if(hd[ud_type].in < hd[ud_type].out){
		dbg_printf(DBG_PAIR_ERR, "%s() : error! in %u < out %u\n", __func__, hd[ud_type].in, hd[ud_type].out);
		return 0;
	}

	if(hd[ud_type].in - hd[ud_type].out > hd[ud_type].max_count)
		hd[ud_type].out = hd[ud_type].in -hd[ud_type].max_count;
		
	return hd[ud_type].in - hd[ud_type].out; 
}

u32 CPai_r_data::Location_queue_fiarst_autid(eUserDataType ud_type)
{
	u32 count = Location_queue_count(ud_type);
	
	if(count > (hd[ud_type].max_count - 5)){ //hd[ud_type].max_count - 5
		count = count - (hd[ud_type].max_count - 5);
		
		dbg_printf(DBG_PAIR_WRN, "%s() : queue skip! : %d \r\n", __func__, count);
		Location_queue_clear(count, ud_type);
	}
	
	return hd[ud_type].out;
}

int CPai_r_data::Location_queue_clear(u32 count, eUserDataType ud_type)
{ 
	if(!hd[ud_type].max_count){
		return 0;
	}

	if(Location_queue_count(ud_type) < (int)count)
		count = (u32)Location_queue_count(ud_type);

	hd[ud_type].out += count;

	dbg_printf(DBG_PAIR_FUNC, "%s() :  queue[%d] in : %u, out : %u, count : %u => %u\r\n", __func__, ud_type, hd[ud_type].in, hd[ud_type].out, Location_queue_count(ud_type) + count, Location_queue_count(ud_type));
	return 1; 
}

int CPai_r_data::Location_pop_front(PAI_R_BACKUP_DATA *pLoc, eUserDataType ud_type, bool continue_read)
{
	return Location_pop(pLoc, 0, ud_type);
}

int CPai_r_data::Location_pop(PAI_R_BACKUP_DATA *pLoc, u32 auto_id,eUserDataType ud_type, bool continue_read)
{
	if(!hd[ud_type].max_count){
		return 0;
	}

	std::lock_guard<std::mutex> _l(data_queue_lock_);
	
	int data_count = Location_queue_count(ud_type);
	u32 out_id = 0;
	int ret;

	if(data_count >= hd[ud_type].max_count){
		hd[ud_type].out = hd[ud_type].in - (hd[ud_type].max_count - 1);
		data_count = Location_queue_count(ud_type);
	}
		
	if(auto_id && auto_id < hd[ud_type].in){		
		out_id = (auto_id % (hd[ud_type].max_count));
	}
	else if(data_count){
		out_id = (hd[ud_type].out % (hd[ud_type].max_count));
		hd[ud_type].out++;
	}
	else {
		return 0;
	}

	if(continue_read){
		if(m_read_fp == NULL){
			const char * data_path = hd[ud_type].data_path;
			m_read_fp = fopen(data_path, "rb");

			if(m_read_fp == NULL){
				dbg_printf(DBG_PAIR_ERR, "%s open failed: %d(%s)\n", data_path, errno, strerror(errno));
				return 0;
			}
		}
		ret = CUserData::read_slot_data(m_read_fp, out_id, hd[ud_type].max_count, sizeof(PAI_R_BACKUP_DATA), (void *)pLoc);
	}
	else {
		ret = CUserData::read_slot_data(hd[ud_type].data_path, out_id, hd[ud_type].max_count, sizeof(PAI_R_BACKUP_DATA), (void *)pLoc);
	}
	
	if(ret == 0)
		return 0;
	
	return 1;
}

//file close
void CPai_r_data::Location_pop_continue_read_close(void)
{
	if(m_read_fp){
		fclose(m_read_fp);
		m_read_fp = NULL;
	}
}

int CPai_r_data::BackupLocationGetJson(LOCATION_INFO &loc_info, eUserDataType ud_type)
{	
	int data_count = Location_queue_count(ud_type);
	
	if(data_count)
	{
		int i = 0;
		u32 start_queue_no = Location_queue_fiarst_autid(ud_type);
		
		PAI_R_BACKUP_DATA * ptrPai_r_data = (PAI_R_BACKUP_DATA *)new char[  sizeof(PAI_R_BACKUP_DATA) ];

		if(ptrPai_r_data == NULL){
			dprintf(DBG_PAIR_ERR, "%s() : memory allocation error!(size:%d)", __func__, sizeof(PAI_R_BACKUP_DATA));
			return 0;
		}

		if(data_count > LOCATION_ONETIME_MAX_SEND_COUNT)
			data_count = LOCATION_ONETIME_MAX_SEND_COUNT;
		
		m_tmp_pop_count = 0;
		
		loc_info.speed = std::string("\"create_datetime\",\"loc_info.speed\",\"distance\"\r\n");
		loc_info.loc = std::string("[");

		for(i=0 ; i < data_count; i++)
		{			
			u32 cur_queue_no = start_queue_no + i;
			
			if(!Location_pop(ptrPai_r_data, cur_queue_no, ud_type)){
				if(i == 0)
					Location_queue_clear(1, ud_type);
				
				break;
			}

			if(cur_queue_no != ptrPai_r_data->location.autoid){
				dprintf(DBG_PAIR_ERR, "%s() : error!! autoid is different.(%d:%d)\r\n", __func__,  ptrPai_r_data->location.autoid, cur_queue_no);
				
				if(i == 0){
					Location_queue_clear(1, ud_type);
					continue;
				}

				break;
			}
			
			const char * file = strrchr(ptrPai_r_data->location.file_path, '/');
			
			if(file == NULL) {
				if(i == 0)
					Location_queue_clear(1, ud_type);
				
				dbg_printf(DBG_PAIR_ERR , "%s() : file path error! \r\n", __func__);
				break;
			}
			
	  		file++;

			time_t create_time = recording_time_string_to_time(file);
			int is_event = 0;

			if(strchr(file, REC_FILE_TYPE_EVENT))
				is_event = 1;				
			
			if(m_tmp_pop_count == 0) {
				loc_info.create_time = create_time;
				loc_info.file_name = std::string(file);
				loc_info.file_auto_id = ptrPai_r_data->location.unique_local_autoid;
				loc_info.queue_no = cur_queue_no;
				loc_info.is_event = is_event ? true : false;

				loc_info.operation_id = ptrPai_r_data->operation_id;
				strcpy(loc_info.operation_uuid, ptrPai_r_data->operation_uuid);
			}
			else{
				if(is_event) //event first
					break;

				if(loc_info.operation_id != ptrPai_r_data->operation_id || strcmp(loc_info.operation_uuid, ptrPai_r_data->operation_uuid) != 0 )
					break;
				
				loc_info.loc.append(",");
			}

			//dprintf(DBG_PAIR_FUNC, "%s() : queue_no %d, cur_queue_no %d (location data queue no %d,  autoid %d)\r\n", __func__,  cur_queue_no, cur_queue_no + m_local_auto_id_offset, ptrPai_r_data->location.autoid, ptrPai_r_data->location.unique_local_autoid);
			
			loc_info.loc.append("{");
			//loc_info.loc.append(format_string("\"driverecorder_autoid\":%u,", ptrPai_r_data->location.autoid/*hd[ud_type].in.c_str()*/));
			loc_info.loc.append(format_string("\"driverecorder_autoid\":%u,", ptrPai_r_data->location.unique_local_autoid));
        	loc_info.loc.append(format_string("\"create_datetime\":\"%s.%03u\",", make_time_string(create_time).c_str(),  (u32)ptrPai_r_data->location.create_time_ms));
        	loc_info.loc.append(format_string("\"movie_filesize\":%u,", ptrPai_r_data->location.move_filesize));
        	loc_info.loc.append(format_string("\"g_sensortype_id\":%d,", is_event));
			loc_info.loc.append(format_string("\"latitude\":%0.6f,", ptrPai_r_data->location.latitude));
			loc_info.loc.append(format_string("\"longitude\":%0.6f,", ptrPai_r_data->location.longitude));
			loc_info.loc.append(format_string("\"accurate\":%u,", ptrPai_r_data->location.accurate));
			loc_info.loc.append(format_string("\"direction\":%u,", ptrPai_r_data->location.direction));
			loc_info.loc.append(format_string("\"altitude\":%u", ptrPai_r_data->location.altitude));
			loc_info.loc.append("}");

			for( int j=0 ; j < DEF_BACKUP_SPEED_LIST_MAX_COUNT; j++){
				if(ptrPai_r_data->speed_list[j].create_time == 0)
					break;

				loc_info.speed.append(format_string("\"%s.%02d\",\"%d\",\"%0.1f\"\r\n", \
					make_time_string(ptrPai_r_data->speed_list[j].create_time).c_str(), ptrPai_r_data->speed_list[j].create_time_ms/10, \
					ptrPai_r_data->speed_list[j].speed, \
					ptrPai_r_data->speed_list[j].distance));		
			}
			m_tmp_pop_count++;

			if((loc_info.loc.length() + loc_info.speed.length()) > (HTTP_MULTIPART_BUFFER_SIZE - (3 * 1024)))
				break;
		}

		//Location_pop_continue_read_close();
		
		loc_info.loc.append("]");
		//dbg_printf(DBG_PAIR_FUNC , "%s \r\n", loc_info.loc.c_str());
		//loc_info.loc = std::string("[{\"driverecorder_autoid\":\"58\",\"create_datetime\":\"2019-09-08 00:08:00.598\",\"movie_filesize\":6907955,\"g_sensortype_id\":0,\"latitude\":34.668555,\"longitude\":135.518744,\"accurate\":10,\"direction\":90,\"altitude\":6}]");
		dbg_printf(DBG_PAIR_INFO , "%s(file autoid : %d, queue_no : %d) \r\n", loc_info.file_name.c_str(), loc_info.file_auto_id, start_queue_no);
		dbg_printf(DBG_PAIR_INFO , "%s \r\n", loc_info.loc.c_str());
		//dbg_printf(DBG_PAIR_FUNC , "%s \r\n", loc_info.speed.c_str());

		delete [] ptrPai_r_data;
		
		return m_tmp_pop_count;
	}

	return 0;
}


