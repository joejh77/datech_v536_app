
#if !defined(__SYSTEM_LOG_H)
#define __SYSTEM_LOG_H

#include <string>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"
#include "daversion.h"

//#define SYSTEMLOG CSystemlog::save

#define DEF_SYSTEM_LOG_DIR 			"/mnt/extsd"
#define DEF_SYSTEM_LOG_PATH_SD 		"/mnt/extsd/systemlog.dat"

#define DEF_SYSTEM_LOG_DIR_SPI		"/data"
#define DEF_SYSTEM_LOG_PATH_SPI		"/data/systemlog.dat"

#define DEF_SYSTEM_LOG_PATH		DEF_SYSTEM_LOG_PATH_SPI


#define SYSTEMLOG_MAX_COUNT		6400

 /*************************************************
  * log definitions
  *************************************************/
#define 	LOG_INIT					0

#define		LOG_SYSBOOT						1001
#define		LOG_SYSEND						1002
	 
#define		LOG_EVENT_DATETIMECHANGE		2001
		 /* sub type */
	#define		RTC_SRC_GPS							0
	#define		RTC_SRC_SETUP_SD				1
	#define 		RTC_SRC_SETUP_APP				2
	#define 		RTC_SRC_SETUP_SERVER		3
 
#define		LOG_ERRDBOVERTIME				2002
 
#define		LOG_EVENT_REC_BEGIN				2101
#define		LOG_EVENT_REC_END				2102
	 
#define		LOG_EVENT_BACKUP				2150
		 /* sub type */
	#define		_LOG_BACKUP_ST_BEGIN	0
	#define		_LOG_BACKUP_ST_END		1
		 /* data type */
	#define		__LOG_BACKUP_DT_SUCCEED	0
	#define		__LOG_BACKUP_DT_USB		0
	#define		__LOG_BACKUP_DT_JPEG	0
	#define		__LOG_BACKUP_DT_DB		1
		 /* data value */
				 /* union data : byte[0] -> error code	( __LOG_BACKUP_DT_SUCCEED or Error codes ) */
				 /* union data : byte[1] -> media type	( __LOG_BACKUP_DT_USB ) */
				 /* union data : byte[2] -> backup type ( __LOG_BACKUP_DT_JPEG or __LOG_BACKUP_DT_DB ) */
 
#define		LOG_EVENT_LOGINOUT				2151
		 /* sub type */
	#define		_LOG_LOGIN				0
	#define		_LOG_LOGOUT				1
		 /* data type */
		 /* data value */
				 /* union data : byte -> account name */
 
#define		LOG_EVENT_MEMORY_FORMAT			2152
				 /* union data : byte -> target name(ex:nand, sd) */
	 
#define		LOG_EVENT_BEGIN_SLEEP			2200
 
#define		LOG_EVENT_ACC_STATE				2201
		 /* data value */
				 /* union data : byte -> xx.xV(battery votage string) */
#define		LOG_EVENT_MP4_FILE_RECOVERY		2300
		 
#define 	LOG_DISCHARGE_SLEEP				2400
 
#define 	LOG_EVENT_MEMORY_INFO				2500
#define 	LOG_EVENT_SD_CD					2501
#define 	LOG_EVENT_USB_CD				2502

#define		LOG_EVENT_BATTCHANGE			2600
		 /* sub type = BLACKBOX_BATT_STATE */
	 enum{
	 	eBB_BATT_STATE_NORMAL = 0,
		eBB_BATT_STATE_PARKING,
		eBB_BATT_STATE_LOW,
		eBB_BATT_STATE_EMER_LOW,
		eBB_BATT_STATE_FALL,
	};
		 /* data value */
				 /* union data : byte -> xx.xV(battery votage string) */

 #define		LOG_EVENT_TEMPCHANGE			2610 
		 /* sub type = BLACKBOX_BATT_STATE */
	 enum{
	 	eBB_TEMP_STATE_NORMAL = 0,
		eBB_TEMP_STATE_HIGH,
		eBB_TEMP_STATE_LOW,
	};
		 /* data value */
				 /* union data : byte -> xx.x Celsius (temperatures in Celsius string) */
		 
#define		LOG_EVENT_EVENT_RECORDING		2700
		 /* sub type */
		 //BLACKBOX_EVENT_TYPE
		 
#define		LOG_EVENT_GSENSOR_ERROR			2702
#define 	LOG_EVENT_BUTTON_POWER			2703	// power off
 
#define		LOG_EVENT_UPDATE				2800
		 /* sub type */
	#define _LOG_FIRMWARE_FILE				0
	#define _LOG_FIRMWARE_USB				1
	#define _LOG_USERDATA_FILE				10
	#define _LOG_USERDATA_USB				11

 /*************************************************
	 //// error message
  *************************************************/	 
#define 	LOG_ERROR_BASE_NO				5000
#define		LOG_ERROR_RTC_READ_FAIL			(LOG_ERROR_BASE_NO + 0)
#define 	LOG_ERROR_SD_DISK_INIT_FAIL		(LOG_ERROR_BASE_NO + 1)
#define 	LOG_ERROR_USB_DISK_INIT_FAIL	(LOG_ERROR_BASE_NO + 2)
#define 	LOG_ERROR_MEDIA_ERROR			(LOG_ERROR_BASE_NO + 3)
#define 	LOG_ERROR_MEDIA_WARNING			(LOG_ERROR_BASE_NO + 4)
#define		LOG_ERROR_INTERNAL_ERR			(LOG_ERROR_BASE_NO + 5)

 /*************************************************
	 //// comm message
  *************************************************/
 enum {
 	LOG_COMM_WIFI = 100,
	LOG_COMM_SERVER,
	LOG_COMM_APP,

	LOG_COMM_END,
};

typedef enum {
	LOG_TYPE_SYSTEM = 1,
	LOG_TYPE_ERROR,
	LOG_TYPE_COMM,
	LOG_TYPE_ETC,
	LOG_TYPE_BATTERY,
	LOG_TYPE_TEMPERATURE,

	LOG_TYPE_END
}eLOG_TYPE;
 /*************************************************
  * log structures
  *************************************************/
 #define LOG_ITEM_DATA_SIZE		20 //ViewerSystemLogWnd.cpp check
 #define LOG_ITEM_DATA_ELLIPSIS_FLAG	0x80
 
 typedef struct tagTr1LogItem {
 	u32 		 log_no;
	 time_t 	 create_time;		 /* log time */
	 
	 u8		 type;				//eLOG_TYPE
	 u8		 log_sub;	 /* sub log log_type */
	 u16		 log_type;		 /* log log_type */
	 
	 union
	 {
		 unsigned char		 byte[LOG_ITEM_DATA_SIZE];
		 u16	 word[LOG_ITEM_DATA_SIZE/2];
		 u32	 dword[LOG_ITEM_DATA_SIZE/4];
	 } data;
 } ST_LOG_ITEM, * LPST_LOG_ITEM;
 
class CSystemlog 
{
public:
	CSystemlog();
	virtual ~CSystemlog();

public:
	static int add_log(ST_LOG_ITEM *log);
	static int get_log(u32 no, ST_LOG_ITEM *log);
	static int save(LPST_LOG_ITEM pLog);
	static int save(eLOG_TYPE type, u16 log_type, u8 log_sub, char *fmt, ...);
	static bool get_item_type_string(char *szbuff, LPST_LOG_ITEM p_log);

	static int init(void);
	static int deinit(void);
	
protected:
	static bool get_item_type_string_sys(char *szbuff, u16 log_type, u8 log_sub);
	static bool get_item_type_string_error(char *szbuff, u16 log_type, u8 log_sub);
	static bool get_item_type_string_comm(char *szbuff, u16 log_type, u8 log_sub);
	static bool get_item_type_string_etc(char *szbuff, u16 log_type, u8 log_sub);
	static bool get_item_type_string_batt(char *szbuff, u16 log_type, u8 log_sub);
};

bool SYSTEMLOG_IS_SYSBOOT(void);
int SYSTEMLOG_START(void);
int SYSTEMLOG_STOP(void);
int SYSTEMLOG(eLOG_TYPE type, u16 log_type, u8 log_sub, const char *fmt, ...);
#endif // !defined(__SYSTEM_LOG_H)

