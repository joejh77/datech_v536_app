
#if !defined(PAI_R_DATA_H)
#define PAI_R_DATA_H

#include <string>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include "datypes.h"
#include "daversion.h"

//#define LOCATION_QUEUE_MAX_COUNT		4096
#define LOCATION_QUEUE_MAX_COUNT		2048	//32G base auto dectect

#define EVENT_INFO_QUEUE_MAX_COUNT	1024


#define DEF_SYSTEM_DIR 							"/mnt/extsd/System"
#define DEF_LOCATION_DATA_PATH 		"/mnt/extsd/System/Location_Data.dat"

#define DEF_SERVER_INFO_JSON_PATH		"/mnt/extsd/System/Server_Info.json"

#define DEF_EVENT_FILE_INFO_PATH 		"/mnt/extsd/System/Event_File_Info.dat"

#define DEF_USER_DATA_IN_PATH 			"/mnt/extsd/System/User_Data_In.dat"
#define DEF_USER_DATA_OUT_PATH 			"/mnt/extsd/System/User_Data_Out.dat"

#define DEF_LOCATION_QUEUE_CHECK_FILE	DEF_USER_DATA_OUT_PATH

#define KBYTE(k)		(k*1024)

#define LOCATION_MAX_SIZE		KBYTE(35)	//60 interval 약 24시간
#define SPEED_CSV_MAX_SIZE		KBYTE(2000) //0.5 interval 약 24시간 

#define LOCATION_ONETIME_MAX_SEND_COUNT 		5 //

#define MOVIE_STATUS_FILE_EXIST 4
/*
[
    {
        "driverecorder_autoid": 12345,
        "create_datetime": "2020-02-20 20:20:20",
        "movie_filesize": 10994320,
        "g_sensortype_id": 0,
        "latitude": 36.5630188,
        "logitude": 136.6640123,
        "accurate": 10,
        "direction": 0,
        "altitude": 5
    },
    {
        "driverecorder_autoid": 22345,
        "create_datetime": "2020-02-20 20:20:20",
        "movie_filesize": 4702863,
        "g_sensortype_id": 1,
        "latitude": 36.5630188,
        "logitude": 136.6640123,
        "accurate": 10,
        "direction": 0,
        "altitude": 5
    }
]
```
*/
typedef enum{
	eUserDataType_Normal = 0,
	eUserDataType_Event,
	
	eUserDataType_End
}eUserDataType;

typedef enum{
	eQueueInOutType_In = 0,
	eQueueInOutType_Out,
	
	eQueueInOutType_End
}eQueueInOutType;

typedef enum{
	eGsensorTypeID_Normal = 0,
	eGsensorTypeID_GEvent,
	eGsensorTypeID_End
}eGsensorTypeID;

typedef struct {
	char 	file_path[64];
	u16		create_time_ms;
	time_t 	create_time;
	size_t 	move_filesize; //Byte
	double	latitude;
	double	longitude;
	u16		accurate;
	u16		direction; //0~359
	u16		altitude;
}PAI_R_LOCATION;

#define DEF_MAX_CAMERA_COUNT 2
//#define DEF_MAX_THUMBNAIL_DATA_SIZE (20 * 1024 + 50164) //jpg 480x270, quality 100
//#define DEF_MAX_THUMBNAIL_DATA_SIZE (50164)
#define DEF_MAX_THUMBNAIL_DATA_SIZE (39884)  //jpg 480x270, quality 80

typedef struct {
	u8		data[DEF_MAX_CAMERA_COUNT][DEF_MAX_THUMBNAIL_DATA_SIZE]; // 320 x 180 jpeg
	size_t 	size[DEF_MAX_CAMERA_COUNT]; //byte
}THUMBNAIL;

/*############################
##############################
사이즈 수정 시 반드시 
81920 Byte가 되도록 
DEF_MAX_THUMBNAIL_DATA_SIZE 를 
수정 한다.
##############################
#############################*/
typedef struct {
	u32		autoid;	//로컬 동영상 자동 ID
	u32 		unique_local_autoid; // 공장초기화 후 계속 누적되는 로컬 동영상 자동 ID
	char 	file_path[64]; //" /mnt/extsd/NORMAR/20001130_152107_I2.avi"
	size_t 	move_filesize; //Byte
	THUMBNAIL thumbnail;
	
	double	latitude;
	double	longitude;
	u16		accurate;
	u16		direction; //0~359
	u16		altitude;

	u16		create_time_ms;
}PAI_R_LOCATION_DATA;

/*
CSV의 파일명은 speed(동영상:로컬 동영상 자동ID).csv로 한다. 예:speed1.csv => zip으로 압축
"create_datetime","speed","distance"
"2019-01-01 11:22:33.12","50","0.00694"
"2019-01-01 11:22:33.62","50","0.00694"
"2019-01-01 11:22:34.12","50","0.00694"
*/
typedef struct {
	time_t 	create_time;
	u16		create_time_ms;
	
	u16 		speed;
	double	distance;
}PAI_R_SPEED_CSV;

#define DEF_BACKUP_SPEED_LIST_MAX_COUNT		120
//struct size 102400
typedef struct {
/*fix*/	time_t 	upload_time;
/*fix*/	u32		event_autoid;	// 이벤트 동영상 자동 ID
/*fix*/	char 	file_hash[36]; //md5

	u32	operation_id;
	char operation_uuid[68];
	
	PAI_R_LOCATION_DATA		location;
	PAI_R_SPEED_CSV			speed_list[DEF_BACKUP_SPEED_LIST_MAX_COUNT];
}PAI_R_BACKUP_DATA;

typedef std::list<PAI_R_LOCATION>								LOCATION_POOL;
typedef LOCATION_POOL::iterator								ITER_LOCATION;

typedef std::list<PAI_R_SPEED_CSV>							SPEED_POOL;
typedef SPEED_POOL::iterator									ITER_SPEED;


typedef struct {
	u8		*thumbnail_data[DEF_MAX_CAMERA_COUNT]; // 320 x 180 jpeg
	size_t 	thumbnail_size[DEF_MAX_CAMERA_COUNT]; //KByte
	
	PAI_R_LOCATION	location;
	SPEED_POOL		speed_list;
}PAI_R_DATA;

typedef std::list<PAI_R_DATA>									PAI_R_DATA_POOL;
typedef PAI_R_DATA_POOL::iterator								ITER_PAI_R_DATA;

#define PAI_R_DATA_MAX_COUNT		10	//((LOCATION_MAX_SIZE + SPEED_CSV_MAX_SIZE) / (sizeof(PAI_R_LOCATION) + sizeof(PAI_R_SPEED_CSV)))

typedef struct {
	bool 	is_event;
	time_t 	create_time;
	u32 		file_auto_id;
	u32		queue_no;

	u32	operation_id;
	char operation_uuid[64];
	
	std::string loc;
	std::string speed;
	std::string file_name;
}LOCATION_INFO;

typedef struct{
	u32 	no;
	u32 	id[eUserDataType_End];
}QUEUE_INDEX;

typedef struct{
	QUEUE_INDEX index[eQueueInOutType_End];
	u32 max_count;
}QUEUE_ID;

typedef struct{
	u32 	max_count;
	u32 	in;		// 로컬 동영상 자동 ID, queue in no
	u32 	out;		// 로컬 동영상 자동 ID, queue out no
	char	data_path[64];

	u32	offs_file_size;	

	u32 	init_size;
}USER_DATA_HEADER;

class CUserData 
{
public:
	CUserData();
	virtual ~CUserData();

	std::mutex data_list_lock_;

	PAI_R_DATA_POOL m_data_list[eUserDataType_End]; // normal, event
	SPEED_POOL		m_pre_speed_list;

	bool		m_location_queue_init;

	u32		m_operation_id;
	char  	m_str_uuid[64];

	u32 	m_unique_local_autoid; // 공장초기화 후 계속 누적되는 로컬 동영상 자동 ID
	int	m_local_autoid_offset; // m_unique_local_autoid - queue_no
	
public:
	static int write_slot_data(FILE *fp, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size = 0, u32 offset_pos = 0);
	static int write_slot_data(const char * path, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size = 0, u32 offset_pos = 0);
	
	static int read_slot_data(FILE *fp, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size = 0, u32 offset_pos = 0);
	static int read_slot_data(const char * path, u32 pos, u32 max_pos, u32 size, void *data, u32 data_size = 0, u32 offset_pos = 0);
	
	
	int addLocation(PAI_R_LOCATION loc, eUserDataType ud_type = eUserDataType_Normal, int pre_recording_time = 0);
	int addSpeed(PAI_R_SPEED_CSV spd, eUserDataType ud_type = eUserDataType_Normal);
	int addThumbnail(u8 * data, size_t size, eUserDataType ud_type = eUserDataType_Normal);

	u32 Location_queue_max_count_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].max_count; }
	u32 Location_queue_auto_id_in_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].in; }
	void Location_queue_auto_id_in_set(u32 id_in, eUserDataType ud_type = eUserDataType_Normal)	{	hd[ud_type].in = id_in; }
	u32 Location_queue_auto_id_out_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].out; }
	void Location_queue_auto_id_out_set(u32 id_out, eUserDataType ud_type = eUserDataType_Normal)	{	hd[ud_type].out = id_out; }
	bool Location_queue_index_init(void);
	void Location_queue_init_file(eUserDataType ud_type);
	int Location_queue_index_backup(eQueueInOutType InOut = eQueueInOutType_In);
	int Location_queue_init(void);
	int Location_queue_deinit(bool backup_need = true);
	
	int Location_queue_count(eUserDataType ud_type = eUserDataType_Normal);
	int Location_queue_clear(u32 count, eUserDataType ud_type = eUserDataType_Normal);
	int Location_queue_clear_idx(u32 out_idx, eUserDataType ud_type = eUserDataType_Normal);
	int Location_push_back(PAI_R_BACKUP_DATA *pLoc, eUserDataType ud_type = eUserDataType_Normal);

	u32 Location_data_list_size(eUserDataType ud_type = eUserDataType_Normal);

	int BackupLocation(eUserDataType ud_type = eUserDataType_Normal);

	u32 Location_queue_fiarst_autid(eUserDataType ud_type = eUserDataType_Normal);
	int update_upload_time(u32 auto_id, time_t upload_time = 0, eUserDataType ud_type = eUserDataType_Normal);
	int update_file_hash(u32 auto_id, const char * hash, eUserDataType ud_type = eUserDataType_Normal);	
	int update_operation_id(u32 operation_id, const char * uuid);
	
protected:

	USER_DATA_HEADER hd[eUserDataType_End];
	u32			location_file_init_size[eUserDataType_End];

	bool			queue_index_init;
	QUEUE_ID 	queue_backup_id;
};

class CPai_r_data
{
public:
	bool		m_queue_index_init;
	u32		m_tmp_pop_count;
	int 		m_local_auto_id_offset;
	
	CPai_r_data();
	virtual ~CPai_r_data();


	int Location_queue_count(eUserDataType ud_type = eUserDataType_Normal);
	u32 Location_queue_fiarst_autid(eUserDataType ud_type = eUserDataType_Normal);
	int Location_queue_clear(u32 count, eUserDataType ud_type = eUserDataType_Normal);
		
	u32 Location_queue_max_count_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].max_count; }
	void Location_queue_max_count_set(u32 id_max, eUserDataType ud_type = eUserDataType_Normal)	{	hd[ud_type].max_count = id_max; }
	u32 Location_queue_auto_id_in_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].in; }
	void Location_queue_auto_id_in_set(u32 id_in, eUserDataType ud_type = eUserDataType_Normal)	{	hd[ud_type].in = id_in; }
	u32 Location_queue_auto_id_out_get(eUserDataType ud_type = eUserDataType_Normal)	{	return hd[ud_type].out; }
	void Location_queue_auto_id_out_set(u32 id_out, eUserDataType ud_type = eUserDataType_Normal)	{	hd[ud_type].out = id_out; }

	int Location_pop_front(PAI_R_BACKUP_DATA *pLoc, eUserDataType ud_type = eUserDataType_Normal, bool continue_read = false);
	int Location_pop(PAI_R_BACKUP_DATA *pLoc, u32 auto_id = 0, eUserDataType ud_type = eUserDataType_Normal, bool continue_read = false);
	void Location_pop_continue_read_close(void);
	
	int BackupLocationGetJson(LOCATION_INFO &loc_info, eUserDataType ud_type = eUserDataType_Normal);
protected:
	FILE *	m_read_fp;
	USER_DATA_HEADER hd[eUserDataType_End];
	
};

/*
	iterator(반복자)
	begin() : beginning iterator를 반환
	end() : end iterator를 반환
	추가 및 삭제
	push_front(element) : 리스트 제일 앞에 원소 추가
	pop_front() : 리스트 제일 앞에 원소 삭제
	push_back(element) : 리스트 제일 뒤에 원소 추가
	pop_back() : 리스트 제일 뒤에 원소 삭제
	insert(iterator, element) : iterator가 가리키는 부분 “앞”에 원소를 추가
	erase(iterator) : iterator가 가리키는 부분에 원소를 삭제
	조회
	*iterator : iterator가 가리키는 원소에 접근
	front() : 첫번째 원소를 반환
	back() : 마지막 원소를 반환
	기타
	empty() : 리스트가 비어있으면 true 아니면 false를 반환
	size() : 리스트 원소들의 수를 반환

	m_GSensorList.push_back(data);
	
	CTextData::ITER_GSENSOR iGSensor;
	iGSensor = m_GSensorList.begin();

	CTextData::ITER_GSENSOR iGSensor = this->m_pSensorData->m_GSensorList.begin();
	CTextData::ITER_GSENSOR iGSensorend = this->m_pSensorData->m_GSensorList.end();

	m_GSensorList.size()
	
	for( ; iGSensor != iGSensorend; iGSensor++ )
	{
		if( dwCurrentTime <= iGSensor->m_cts)
			break;
	}
	
	m_GSensorList.clear();
*/
#endif // !defined(PAI_R_DATA_H)

