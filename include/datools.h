#ifndef _datech_tools_h
#define _datech_tools_h

#include <functional>
 #include <stdarg.h> 
#include "datypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define	DBG_ALL		(DBG_ERR | DBG_WRN | DBG_MSG)

#define	DBG_ERR		0X04
#define	DBG_WRN		0X02
#define	DBG_MSG 		0X01


/**
 * Position data in fractional degrees or radians
 */
typedef struct _nmeaPOS
{
    double lat;         /**< Latitude */
    double lon;         /**< Longitude */
	double alt;			 /**< Altitude */

} nmeaPOS;

int syslog_printf(const char *user, int view, const char *fmt, ...);
int dbg_printf(int status, const char *fmt, ...);

std::string datool_generateUUID();

/**
 * This is to suspend execution for microsecond intervals.
 * @param t 			Time in microsecond unit.
 */
void msleep(unsigned int t);

/**
 * This is to get microsecond.
 * @return 				microsecond.
 */
uint64_t systemTimeUs(void);

void parseUsec(int64_t usec, int &h, int &m, int &s, int &u);

void bytesToString(size_t size, char *size_string);

u32 get_tick_count(void);
u32 get_tick_count_sync(void);
void tick_count_sync(time_t time_sec);

u32 get_sec_count(void);

/**
 * This is to determine whether is it time out from the last start-time.
 * @param StartSec 		The start second.
 * @param TimeOutSec 		The second span in second.
 * @return 				1 on success; 0 on error.
 */
int is_time_out_sec(u32 *StartSec, u32 TimeOutSec);

/**
 * This is to determine whether it is time out for a command.
 * @param StartTm 		The command start time.
 * @param TimeOutMs 		The time span in millisecond.
 * @return 				1 on success; 0 on error.
 */
int is_time_out_ms(u32 *StartTm, u32 TimeOutMs);

/** 
 * @return 2019-08-05 10:22:00
 */
std::string make_time_string(time_t t);
time_t pai_r_time_string_to_time(std::string time);

std::string make_recording_time_string(time_t t);
time_t recording_time_string_to_time(std::string time);
std::string format_string(const std::string fmt, ...) ;

void bin2str(char *to, const unsigned char *p, size_t len);

/**
 * This is to get speed.
 * @param count 		The pulse count.
 * @param parameter 	The pulse parameter.
 * @return 			speed (km/h)
 */
double pulse_count_to_speed(u32 count, double parameter);

/**
 * This is to get pulse parameter.
 * @param count 			The pulse count.
 * @param speed 		 	The speed(km/h).
 * @return 			 	pulse parameter
 */
double pulse_speed_to_parameter(u32 count, double speed);

/**
 * \brief Calculate distance between two points
 * \return Distance in meters
 */
double datool_nmea_distance(
        const nmeaPOS *from_pos,    /**< From position in radians */
        const nmeaPOS *to_pos       /**< To position in radians */
        );

double datool_nmea_distance_with_altitude(
	     const nmeaPOS *from_pos,    /**< From position in radians */
        const nmeaPOS *to_pos       /**< To position in radians */
        );

/**
 * This is to set blue led.
 * @param set 		 	true led on, false led off.
 * @return 			 	0 on success.
 */
int datool_led_blue_set(bool set);

/**
 * This is to set red led.
 * @param speed 		 	true led on, false led off.
 * @return 			 	0 on success.
 */
int datool_led_red_set(bool set);

/**
 * This is to set green led. for WiFi
 * @param speed 		 	true led on, false led off.
 * @return 			 	0 on success.
 */
int datool_led_green_set(bool set);
	
/**
 * This is to set ext trigger out.
 * @param speed 		 	true high level, false low level.
 * @return 			 	0 on success.
 */
int datool_ext_trigger_out_set(bool set);

/**
 * This is to set ext speed limit out.
 * @param speed 		 	true high level, false low level.
 * @return 			 	0 on success.
 */
int datool_ext_speed_limit_out_set(bool set);


int datool_spk_shdn_set(bool set);

int datool_getIPAddress(char *ip_addr);

///////////////////////////////////////
enum {
	LED_WORK_NORMAL = 0,
	LED_WORK_EVENT,
	LED_WORK_FORMAT,
	LED_WORK_BOOTING,
	LED_WORK_SD_ERROR,

	LED_WORK_POWER_OFF,
	LED_WORK_LED_OFF,
	
	LED_WORK_END
};

enum {
	LED_BLUE = 0,
	LED_RED,
	LED_WIFI,

	LED_END,
	LED_WORK_MODE,
	LED_TIME_100MS,
	LED_EXIT
};
#define LED_TIME_INFINITY 		0x7fffffff

typedef struct {
	int type; // blud, red
	int onTimeMs;
	int offTimeMs;
}LED_MSG;

#define	MSGQ_LED_IDKEY		7370
#define	MSGQ_PAIR_LOC_IDKEY		7071
#define	MSGQ_PAIR_SPD_IDKEY		7172

#define IPC_MSG_QUEUE_USE 		1
#define	MSGQ_PAIR_LOC_Q_OUT_IDKEY		8010
#define	MSGQ_PAIR_LOC_Q_IN_IDKEY			8020
#define	MSGQ_SYSTEM_LOG_IDKEY			8030

typedef enum {
	WIFI_LED_NOT_CON = 0,		// off
	WIFI_LED_CONNECTED,	// 1Sec On/Off
	WIFI_LED_SERVER_OK, 	// ON

	WIFI_LED_AP_MODE,  // 50ms On, 950ms Off
	WIFI_LED_END
}ENUM_WIFI_LED;

typedef enum {
	QMSG_TYPE_START = 0,
		
	QMSG_IN,
	QMSG_LOC_MAX,
	QMSG_EXIT,		//use power off
	
	QMSG_OUT,
	QMSG_UP_TIME,		//type, auto_id, time_t
	QMSG_UP_FILE_HASH,	//type, auto_id, md5 string
	
	QMSG_CHANNEL,
	QMSG_SOUND,
	QMSG_LED,
	QMSG_SPEAKER,

	QMSG_HTTPD_START,
	QMSG_HTTPD_OPERID,

	
	QMSG_HTTPD_ALIVE,

	QMSG_HTTPD_SNAPSHOT_START, //camera, width, height
	QMSG_HTTPD_SNAPSHOT_DONE,		// type (0 is timeout or error, 1 is ok), size
	
	QMSG_END
}ENUM_QMSG_TYPE;

typedef struct {
	long type; // ENUM_QMSG_TYPE
	u32 data;
	u32 data2;
	time_t time;
	char string[64]; //md5, uuid ...
}ST_QMSG;


int datool_ipc_msgget(key_t key);
int datool_ipc_msgsnd(int msgq_id, void * data, size_t size);
int datool_ipc_msgsnd2(int msgq_id, long type, u32 data);
int datool_ipc_msgsnd3(int msgq_id, long type, u32 data, u32 data2);

int datool_ipc_msgrcv(int msgq_id, void * data, size_t size);


int datool_led_ipc_msgsnd_ex(int msgq_id, int led_type, int onTimeMs, int offTimeMs);
int datool_is_appexe_stop(const char *cmd, int flg, int *num);
int datool_test(void);
#ifdef __cplusplus
}
#endif

#endif //_datech_tools_h