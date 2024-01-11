
#if !defined(__DATA_LOG_H)
#define __DATA_LOG_H

#include <string>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"
#include "daversion.h"

//#define SYSTEMLOG CSystemlog::save

#define DEF_DATA_LOG_DIR 			"/mnt/extsd/DATA"
#define DEF_DATA_LOG_FILE_FMT 		"log_data_%04d%02d%02d_%d.log"
#define DEF_DATA_LOG_PATH_FMT 		DEF_DATA_LOG_DIR "/" DEF_DATA_LOG_FILE_FMT
#define DEF_DATA_LOG_STRING_MAX_SIZE		128

#if 0
//#define DATALOG_FILE_SIZE					(22000 * DEF_DATA_LOG_STRING_MAX_SIZE) // 6시간 분량
//#define DATALOG_FILE_MAX_COUNT		(100)				  // 약 1개월 분량
#elif 0
#define DATALOG_FILE_SIZE					(3600 * DEF_DATA_LOG_STRING_MAX_SIZE) // 1시간 분량
#define DATALOG_FILE_MAX_COUNT		(100)				  // 약 3일
#else
#define DATALOG_FILE_SIZE					(12 * 1024 * 1024) //(86400 * DEF_DATA_LOG_STRING_MAX_SIZE) // 24시간 분량
#define DATALOG_FILE_MAX_COUNT		(30)				  // 약 1개월 분량
#endif

 /*************************************************
  * log definitions
  *************************************************/

/*
FWVersion=VRHD/0.0.22
SerialNumber=2020121234
Date=2020-12-09
Time,OperatingTime,GpsConnectionState,GpsSignalState,Latitude,Longitude,Speed(Km/h),Distance(Km),RPM,GS_Event,GS_X,GS_Y,GS_Z,Brake,Winker_L,Winker_R,InputTrigger1,InputTrigger2,Volt
15:04:15,20,1,1,37.576016,126.676922,100,0.03,1000,0,0.01,-0.02,1.02,0,0,0,0,0,14.4
*/



 /*************************************************
  * log structures
  *************************************************/

#define DEF_DRIVER_CODE_FILE_PATH					"/mnt/extsd/driver_code.txt"

#define EVENT_CAUSE_CATEGORY_NO_EVENT								1
#define EVENT_CAUSE_CATEGORY_RAPID_ACCELERATION		2
#define EVENT_CAUSE_CATEGORY_SUDDEN_DECELERATION	3
#define EVENT_CAUSE_CATEGORY_SUDDEN_HANDLE					4
#define EVENT_CAUSE_CATEGORY_OTHERS									5
#define EVENT_CAUSE_CATEGORY_OVERSPEED							6
#define EVENT_CAUSE_CATEGORY_FASTSPEED  							7	
 
 typedef struct tagDataLogItem {
	 time_t 	 Time;		 /* log time */
	 u32 		OperatingTime;
	 
	 u16			GpsConnectionState;
	 u16			GpsSignalState;
	 double 	Latitude;
	 double	Longitude;

	 u32			Speed;	// Km/h
	 double	Distance; // Km
	 u16			RPM;
	 
	 u16			GS_Event;
	 double	GS_X;
	 double	GS_Y;
	 double	GS_Z;

	 u8			Brake;
	 u8 			Winker_L;
	 u8			Winker_R;
	 u8			InputTrigger1;
	 u8			InputTrigger2;

	 double	Volt;

	 u8			Cause;
	 int 			Difference;
	 int			Highway;
 } ST_DATA_LOG_ITEM, * LPST_DATA_LOG_ITEM;

typedef std::list<ST_DATA_LOG_ITEM>						DATALOG_POOL;
typedef DATALOG_POOL::iterator								ITER_DATALOG;

class CDatalog 
{
public:
	CDatalog();
	virtual ~CDatalog();

//public:
	int add_log(LPST_DATA_LOG_ITEM pLog);
	int save_log(bool shutdown = false);
	void set_serial_number(u32 no) { m_serial_number = no; }
	void set_driver_code(char * code) { strcpy(m_driver_code, code); }

	int init(time_t time);
	int deinit(void);

	DATALOG_POOL	m_data_log_list;
	time_t	m_last_add_time;
	bool		m_event_recording;
	u8		m_event_cause_category;
	int		m_event_cause_difference;
	
protected:

	int make_log_string(char *log_string, int size, LPST_DATA_LOG_ITEM pLog, struct tm &tm_t);
	int write_log(void * log_string, int log_length);
	
	bool m_is_init;
	u32	m_log_position;
	u32	m_file_serial_no;
	u32	m_file_count;
	u32  m_log_file_max_count;

	u32 m_serial_number;
	time_t	m_pre_log_time;
	u32 m_pre_operating_time;

	char m_driver_code[64];

	FILE *m_fp;
};
#endif // !defined(__DATA_LOG_H)

