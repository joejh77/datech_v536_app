// pai_r_data.cpp: implementation of the CSystemlog class.
//
//////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include <fcntl.h>
#include <thread>
#include "daappconfigs.h"
#include "datools.h"
#include "sysfs.h"
//#include "tinyxml.h"
#include "system_log.h"

#define DBG_SYS_LOG_FUNC 	1 // DBG_MSG
#define DBG_SYS_LOG_ERR  		DBG_ERR
#define DBG_SYS_LOG_WRN		DBG_WRN

static bool m_is_init = false;
static u32	m_log_no = 0;

CSystemlog::CSystemlog()
{

}

CSystemlog::~CSystemlog()
{
	deinit();
}


int CSystemlog::add_log(ST_LOG_ITEM *log)
{
	u32 id = 0;
	FILE *fp;
	const char * data_path = DEF_SYSTEM_LOG_PATH;

	if(!m_is_init){
		if(init() == 0)
			return 0;
	}
	
	fp = fopen(data_path, "rb+");

	id = (m_log_no % (SYSTEMLOG_MAX_COUNT));
	
	if(fp){
		fseek(fp, id  * sizeof(ST_LOG_ITEM), SEEK_SET);

		if(log->create_time == 0)
			log->create_time = time(0);
		
		fwrite((void *)log, 1, sizeof(ST_LOG_ITEM), fp);				
	
		fclose(fp);
		
		//dbg_printf(DBG_SYS_LOG_FUNC, "%s() :  %d (%s) \n", __func__, m_log_no, make_time_string(log->create_time).c_str());
		m_log_no++;
	} else {
		dbg_printf(DBG_SYS_LOG_ERR, "%s creation failed: %d(%s)\n", data_path, errno, strerror(errno));
	}

	
	if(log->type == LOG_TYPE_SYSTEM && log->log_type == LOG_SYSEND){
		if(access("/dev/mmcblk0p1", R_OK ) == 0 ){
			char szCmd[64];
			sprintf(szCmd, "cp %s %s", data_path, DEF_SYSTEM_LOG_DIR);
			system(szCmd);
			dbg_printf(DBG_SYS_LOG_FUNC, "%s() :  %s\n", __func__, szCmd);
		}
	}
	
	return m_log_no;
}

int CSystemlog::get_log(u32 no, ST_LOG_ITEM *log)
{
	u32 id = 0;
	
	if(!m_is_init){
		if(init() == 0)
			return 0;
	}

	if(no >= m_log_no){
		dbg_printf(DBG_SYS_LOG_ERR, "%s() : id error!(%d:%d)\n", __func__, no, m_log_no);
		return 0;
	}
	
	
	const char * data_path = DEF_SYSTEM_LOG_PATH;
	FILE *fp = fopen(data_path, "rb");

	id = (no % (SYSTEMLOG_MAX_COUNT));
	
	if(fp){
		fseek(fp, id * sizeof(ST_LOG_ITEM),SEEK_SET);	
		int ret =  fread( (void *)log, 1, sizeof(ST_LOG_ITEM), fp );
		if (ret != sizeof(ST_LOG_ITEM) ) {
			dbg_printf(DBG_SYS_LOG_ERR, "%s read failed: %d(%s) , ret = %d : %d\n", data_path, errno, strerror(errno), ret, sizeof(ST_LOG_ITEM));
		}

		fclose(fp);
		dbg_printf(DBG_SYS_LOG_FUNC, "%s get %d \n", data_path, no);
	} else {
		dbg_printf(DBG_SYS_LOG_ERR, "%s open failed: %d(%s)\n", data_path, errno, strerror(errno));
		return 0;
	}
	return 1;
}

int CSystemlog::init(void)
{
	if(m_is_init)
		return m_log_no;
	
	const char * file_path = DEF_SYSTEM_LOG_PATH;

	if ( access(file_path, R_OK ) != 0){
		const char * sd_log_file_path = DEF_SYSTEM_LOG_PATH_SD;

		if ( access(sd_log_file_path, R_OK ) == 0){
			//fflush(fp);
			char szCmd[64];
			sprintf(szCmd, "cp -rf %s %s", sd_log_file_path, DEF_SYSTEM_LOG_DIR_SPI);
			system(szCmd);
			dbg_printf(DBG_SYS_LOG_FUNC, "%s() :  %s\n", __func__, szCmd);
		}
		else {
			FILE *fp = fopen(file_path, "wb");
			dbg_printf(DBG_SYS_LOG_FUNC, "%s fopen %d\n", file_path, fp);
			
			if(fp) {
				ST_LOG_ITEM *p_log;
				int buf_size = SYSTEMLOG_MAX_COUNT * sizeof(ST_LOG_ITEM);
				u8* buf = new u8[ buf_size ];
				memset((void *)buf, 0, buf_size);

				p_log = (ST_LOG_ITEM *)buf;
				
				p_log->log_no = 1;
				p_log->type = LOG_TYPE_SYSTEM;
				p_log->log_type = LOG_INIT;
				p_log->create_time = time(0);
				sprintf((char *)p_log->data.byte, "Initialize");
				
				fwrite((void *)buf, 1, buf_size, fp);

				fclose(fp);
				delete [] buf;
				dbg_printf(DBG_SYS_LOG_FUNC, "%s saved %d bytes.\n", file_path, buf_size);
			} else {
				dbg_printf(DBG_SYS_LOG_ERR, "%s creation failed: %d(%s)\n", file_path, errno, strerror(errno));
			}
		}
	}

	
	FILE* file=  fopen( file_path, "r");
	int length = 0;
	
	if(!file){
		dbg_printf(DBG_SYS_LOG_ERR," [%s] file open error!\n", file_path);
		return 0;
	}
	
	fseek( file, 0, SEEK_END );
	length = ftell( file );
	fseek( file, 0, SEEK_SET );
	
// Strange case, but good to handle up front.
	if ( length <= 0 ) {
		m_log_no = 0;
		dbg_printf(DBG_SYS_LOG_ERR, "%s file error!\r\n", file_path);
	}
	else if(length < (int)(SYSTEMLOG_MAX_COUNT * sizeof(ST_LOG_ITEM))) {
		m_log_no = length / sizeof(ST_LOG_ITEM);
	}
	else {
		ST_LOG_ITEM *p_log;
		int ret;
		int buf_size = SYSTEMLOG_MAX_COUNT * sizeof(ST_LOG_ITEM);
		u8* buf = new u8[ buf_size ];

		memset((void *)buf, 0, buf_size);
		
		ret =  fread( (void *)buf, 1, buf_size, file );
		if (ret != buf_size ) {
			dbg_printf(DBG_SYS_LOG_ERR, "%s read failed: %d(%s) , ret = %d : %d\n", file_path, errno, strerror(errno), ret, sizeof(ST_LOG_ITEM));
		}
		
		m_log_no = 0;

		for(int i = 0; i < SYSTEMLOG_MAX_COUNT; i++){
			p_log = (ST_LOG_ITEM *)&buf[i*sizeof(ST_LOG_ITEM)];
			
			if(p_log->log_no > m_log_no)
				m_log_no = p_log->log_no;
			else
				break;
		}

		delete [] buf;
	}
	fclose(file);
	
	m_is_init = true;
	
	dbg_printf(DBG_SYS_LOG_FUNC, "%s() : %d\n", __func__, m_log_no);
	return m_log_no;
}

int CSystemlog::deinit(void)
{
	m_is_init = false;
	return 0;
}

int CSystemlog::save(LPST_LOG_ITEM pLog)
{
	if(!m_is_init){
		if(init() == 0)
			return 0;
	}

	pLog->log_no = m_log_no + 1;
	add_log(pLog );
//// debug 
#if DBG_SYS_LOG_FUNC
	{
		char str_type[64];
		get_item_type_string(str_type, pLog );
		dbg_printf(DBG_SYS_LOG_FUNC, "%s() : %d : %s	%s\n", __func__, pLog->log_no, str_type, (char *) pLog->data.byte);
	}
#endif
	return m_log_no;
}

int CSystemlog::save(eLOG_TYPE type, u16 log_type, u8 log_sub, char *fmt, ...)
{
	if(!m_is_init){
		if(init() == 0)
			return 0;
	}

	va_list argP;
	char string[255] = {0,};
	ST_LOG_ITEM log = { 0,};
	
	va_start(argP, fmt);
	vsprintf(string, fmt, argP);
	va_end(argP);

	log.log_no = m_log_no + 1;
	
	log.create_time = time(0);

	log.type = (u8)type;
	log.log_type = log_type;
	log.log_sub = log_sub;

	strncpy((char *)log.data.byte, string, LOG_ITEM_DATA_SIZE-1);

	add_log(&log);
	
	
//// debug 
#if DBG_SYS_LOG_FUNC
	{
		char str_type[64];
		get_item_type_string(str_type, &log);
		dbg_printf(DBG_SYS_LOG_FUNC, "%s() : %d : %s	%s\n", __func__, log.log_no, str_type, string);
	}
#endif	
	return m_log_no;
}


bool CSystemlog::get_item_type_string_sys(char *szbuff, u16 log_type, u8 log_sub)
{
	switch(log_type)
	{
	case LOG_INIT:
		sprintf(szbuff,"SYSTEM LOG");
		break;
	case LOG_SYSBOOT:
		sprintf(szbuff,"SYSTEM START");
		break;
	case LOG_SYSEND:
		sprintf(szbuff,"SYSTEM END");
		break;
	case LOG_EVENT_DATETIMECHANGE:
		/* sub type */
		if( log_sub == RTC_SRC_GPS )
			sprintf((char*)szbuff, "TIME SETUP(GPS)");
		else if( log_sub == RTC_SRC_SETUP_SD )
			sprintf((char*)szbuff, "TIME SETUP(SD)");
		else if( log_sub == RTC_SRC_SETUP_APP )
			sprintf((char*)szbuff, "TIME SETUP(APP)");
		else if( log_sub == RTC_SRC_SETUP_SERVER )
			sprintf((char*)szbuff, "TIME SETUP(SVR)");
		else
			sprintf((char*)szbuff, "TIME SETUP");
		
		break;
	case LOG_ERRDBOVERTIME:
		sprintf((char*)szbuff, "WRITE ERROR - OVER TIME");
		break;
	case LOG_EVENT_REC_BEGIN:
		sprintf((char*)szbuff, "RECORD(BEGIN)");
		break;
	case LOG_EVENT_REC_END:
		sprintf((char*)szbuff, "RECORD(END)");
		break;
	case LOG_EVENT_LOGINOUT:
		/* sub type */
		if( log_sub == _LOG_LOGIN )
			sprintf((char*)szbuff, "LOG IN  ");
		else if( log_sub == _LOG_LOGOUT )
			sprintf((char*)szbuff, "LOG OUT ");

		break;
	case LOG_EVENT_MEMORY_FORMAT:
		sprintf((char*)szbuff, "FORMAT");
		break;
	case LOG_EVENT_BEGIN_SLEEP:
		sprintf((char*)szbuff, "SLEEP MODE");
		break;
	case LOG_EVENT_ACC_STATE:
		sprintf((char*)szbuff, "ACC");
		break;
	case LOG_EVENT_MP4_FILE_RECOVERY:
		sprintf((char*)szbuff, "RECOVERY");
		break;
	case LOG_DISCHARGE_SLEEP :
		sprintf((char*)szbuff, "DISCHARGE");
		break;
	case LOG_EVENT_MEMORY_INFO:
		sprintf((char*)szbuff, "MEMORY INFO");
		break;
	case LOG_EVENT_SD_CD:
		sprintf((char*)szbuff, "SD CARD");
		break;
	case LOG_EVENT_USB_CD:
		sprintf((char*)szbuff, "USB MEMORY");
		break;
	case LOG_EVENT_BATTCHANGE:
		if( log_sub == eBB_BATT_STATE_NORMAL )
			sprintf((char*)szbuff, "BATT NORMAL");
		else if( log_sub == eBB_BATT_STATE_PARKING)
			sprintf((char*)szbuff, "BATT PARKING");
		else if( log_sub == eBB_BATT_STATE_LOW)
			sprintf((char*)szbuff, "BATT LOW");
		else if( log_sub == eBB_BATT_STATE_EMER_LOW)
			sprintf((char*)szbuff, "BATT EMER LOW");
		else if( log_sub == eBB_BATT_STATE_FALL)
			sprintf((char*)szbuff, "BATT FALL");
		else
			sprintf((char*)szbuff, "BATT");
		
		break;

	case LOG_EVENT_TEMPCHANGE:
		if( log_sub == eBB_TEMP_STATE_NORMAL )
			sprintf((char*)szbuff, "TEMP NORMAL");
		else if( log_sub == eBB_TEMP_STATE_HIGH)
			sprintf((char*)szbuff, "TEMP HIGH");
		else if( log_sub == eBB_TEMP_STATE_LOW)
			sprintf((char*)szbuff, "TEMP LOW");
		else
			sprintf((char*)szbuff, "TEMP");
		
		break;
		
	case LOG_EVENT_EVENT_RECORDING:
		sprintf((char*)szbuff, "EVENT");
		break;

	case LOG_EVENT_GSENSOR_ERROR:
		sprintf((char*)szbuff, "G_SENSOR");
		break;
	case LOG_EVENT_BUTTON_POWER:
		sprintf((char*)szbuff, "KEY");
		break;
	case LOG_EVENT_UPDATE:
		sprintf((char*)szbuff, "S/W UPDATE");
		break;
	
	default:
		sprintf(szbuff,"undefined(%d)", log_type);
		break;
	}

	return TRUE;
}

bool CSystemlog::get_item_type_string_error(char *szbuff, u16 log_type, u8 log_sub)
{
	switch(log_type)
	{
		//// error message
		case LOG_ERROR_RTC_READ_FAIL :
			sprintf((char*)szbuff, "[ERROR] RTC");
			break;
		case LOG_ERROR_SD_DISK_INIT_FAIL :
			sprintf((char*)szbuff, "[ERROR] SD");
			break;
		case LOG_ERROR_USB_DISK_INIT_FAIL:
			sprintf((char*)szbuff, "[ERROR] USB");
			break;
		case LOG_ERROR_MEDIA_ERROR :
			sprintf((char*)szbuff, "[ERROR] MEDIA");
			break;
		case LOG_ERROR_MEDIA_WARNING :
			sprintf((char*)szbuff, "[ERROR] MEDIA WARNING");
			break;
		case LOG_ERROR_INTERNAL_ERR	:
			sprintf((char*)szbuff, "[ERROR] INTERNAL");
			break;
		
		default:
			sprintf(szbuff,"undefined(%d)", log_type);
			break;
	}
	return TRUE;
}

bool CSystemlog::get_item_type_string_comm(char *szbuff, u16 log_type, u8 log_sub)
{
	switch(log_type)
	{
		case LOG_COMM_WIFI :
			sprintf((char*)szbuff, "WIFI");
			break;
			
		case LOG_COMM_SERVER :
			sprintf((char*)szbuff, "SERVER");
			break;

		case LOG_COMM_APP :
			sprintf((char*)szbuff, "APP");
			break;
			
		default:
			sprintf(szbuff,"undefined(%d)", log_type);
			break;
	}
	return TRUE;
}

bool CSystemlog::get_item_type_string_etc(char *szbuff, u16 log_type, u8 log_sub)
{
	switch(log_type)
	{

		default:
			sprintf(szbuff,"undefined(%d)", log_type);
			break;
	}
	return TRUE;
}

bool CSystemlog::get_item_type_string_batt(char *szbuff, u16 log_type, u8 log_sub)
{
	switch(log_type)
	{

		default:
			sprintf(szbuff,"undefined(%d)", log_type);
			break;
	}
	return TRUE;
}



bool CSystemlog::get_item_type_string(char *szbuff, LPST_LOG_ITEM p_log)
{
	u8 log_sub = p_log->log_sub & ~LOG_ITEM_DATA_ELLIPSIS_FLAG;
	
	switch(p_log->type)
	{		
		case LOG_TYPE_ERROR : 		get_item_type_string_error(szbuff, p_log->log_type, log_sub);	break;
		case LOG_TYPE_COMM : 		get_item_type_string_comm(szbuff, p_log->log_type, log_sub);	break;
		case LOG_TYPE_ETC : 			get_item_type_string_etc(szbuff, p_log->log_type, log_sub);	break;
		
		case LOG_TYPE_BATTERY : 	//get_item_type_string_batt(szbuff, p_log->log_type, log_sub);	break;
		case LOG_TYPE_TEMPERATURE :
		case LOG_TYPE_SYSTEM : 	get_item_type_string_sys(szbuff, p_log->log_type, log_sub);		break;

		default: sprintf(szbuff,"%d", p_log->type); break;
	}
	
	return TRUE;
}

//=============================================================
static std::thread syslog_thread;
static bool syslog_exit = false;
static int syslog_msg_q_id = -1;
static bool m_is_sysboot = false;

static void system_log_task(void)
{
		while(!syslog_exit && get_tick_count() < SAFE_POWER_ON_TIME){
			sleep(1);
		}
		
		do {
			ST_LOG_ITEM log;
			
			while(datool_ipc_msgrcv(syslog_msg_q_id, (void *)&log, sizeof(log)) != -1){

				if(log.type == LOG_TYPE_SYSTEM && log.log_type == LOG_SYSBOOT){
					m_is_sysboot = true;
				}
				else if(!m_is_sysboot){
					char str_type[64];
					CSystemlog::get_item_type_string(str_type, &log );
					dbg_printf(DBG_SYS_LOG_FUNC, "%s() : [%d : %s	%s] save skip!. sysboot do not started.\n", __func__, log.log_no, str_type, (char *) log.data.byte);
					continue;
				}
					
				//CSystemlog::deinit();
				CSystemlog::save(&log);			

				if(log.type == LOG_TYPE_SYSTEM && log.log_type == LOG_SYSEND){
					msleep(50);
					m_is_sysboot = false;
				}
			}

			msleep(250);
		} while(!syslog_exit);

}

bool SYSTEMLOG_IS_SYSBOOT(void)
{
	return m_is_sysboot;
}

int SYSTEMLOG_START(void)
{
	syslog_exit = false;
	
   	syslog_thread = std::thread(system_log_task);
	dbg_printf(DBG_SYS_LOG_FUNC, "%s() : %d\r\n", __func__, syslog_msg_q_id);
	return 0;
}

int SYSTEMLOG_STOP(void)
{
	syslog_exit = true;
	
	 if (syslog_thread.joinable()) {
		syslog_thread.join();
 	}
	 
	dbg_printf(DBG_SYS_LOG_FUNC, "%s() : \r\n", __func__);
	 return 0;
}

int SYSTEMLOG(eLOG_TYPE type, u16 log_type, u8 log_sub, const char *fmt, ...)
{
	va_list argP;
	char string[255] = {0,};
	ST_LOG_ITEM log = { 0,};

	if(syslog_msg_q_id == -1)
		syslog_msg_q_id = datool_ipc_msgget(MSGQ_SYSTEM_LOG_IDKEY);
	
	va_start(argP, fmt);
	vsprintf(string, fmt, argP);
	va_end(argP);

	log.log_no = 1; //message type
	log.create_time = time(0);

	if(strlen(string) > LOG_ITEM_DATA_SIZE-1)
		log_sub |= LOG_ITEM_DATA_ELLIPSIS_FLAG;
	
	log.type = (u8)type;
	log.log_type = log_type;
	log.log_sub = log_sub;

	strncpy((char *)log.data.byte, string, LOG_ITEM_DATA_SIZE-1);

	return datool_ipc_msgsnd(syslog_msg_q_id, (void *)&log, sizeof(log));
}

