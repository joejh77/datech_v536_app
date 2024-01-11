#ifndef _datech_app_configs_h
#define _datech_app_configs_h

#include "daversion.h"

#define INI_3CH_FILE_PATH "/data/recorder3-datech-3ch.ini"
#define INI_REAR_2CH_FILE_PATH "/data/recorder2-datech-rear_2ch.ini"

//#define VIDEO_NORMAL_DIR	"/tmp/mnt/normal"
#define VIDEO_NORMAL_DIR	"/mnt/extsd/NORMAL"
#define VIDEO_EVENT_DIR	"/mnt/extsd/EVENT"
#define VIDEO_MOTION_DIR	"/mnt/extsd/NORMAL"
#define MD_THRESHOLD	500

#define ENABLE_MD 1
#define ENABLE_ADAS ENABLE_BOARD_CHECK

#define FRONT_720P 0

#define ENABLE_NETWORKING	1

#define CAMERA_FPS 	30
#define AVI_FPS		30 //time lapse
#define TIMELAPSE_FPS	1
//#define H264_FPS 15


#define MBYTES(x)	((x)*1024*1024)

#define MIC_16KHZ	0
#define MIC_24KHZ	0		//wav file is Signed 16 bit Little Endian, Rate 24000 Hz, Mono
//undefined 22050 use	//16K나 24K 사용시 aplay시행 후 recording audio 시간이 짧아짐(60 => 54sec)

#if 1
#if 0
	//high
#define BITRATE_1080P MBYTES(2)	// 3, 6, 9
#define BITRATE_720P (800*1024)  // 1, 2, 3
#define BITRATE_480I (500*1024)
#define OFFS_CFG_FILE_NAME		"offs_high.cfg"
#elif 1
	//medium
 #ifdef DEF_REAR_CAMERA_ONLY
 #define BITRATE_1080P (2800*1024) //
 #define BITRATE_720P  (1200*1024)  // L:16M M:33M H:48M
 #else
  #ifdef TARGET_CPU_V536		
	#define BITRATE_1080P (2000000)  // 4, 6 , 8M
   #define BITRATE_720P  (1000000) // 1.5, 2, 3M
   #define BITRATE_REAR_LOW  (1500000)
  #else
  #define BITRATE_1080P (MBYTES(2) + (200*1024)) // 100M(2M => 87M create, 2.2M => 101M create)
  //#define BITRATE_720P  (MBYTES(1) + (300*1024))  // 60M( 1M => 43M create, 1.3M => 54M create)
  #define BITRATE_720P  MBYTES(1)  // 43M( 1M => 43M create, 1.3M => 54M create)
  #endif
 #endif
#if DEF_30FIX_FRAME
#define BITRATE_480I (500*1024)  // 16M(500K => 14M create)
#else
#define BITRATE_480I (900*1024)  // F10/R3 (13.7M create)
#endif

#define OFFS_CFG_FILE_NAME		"offs.cfg"
#define OFFS_REAR_CAM_ONLY_FILE_NAME		"offs_rear_only.cfg"
#else
	//low
#define BITRATE_1080P  (MBYTES(1) + (512*1024))
#define BITRATE_720P 	  (512*1024)
#endif
#else
#define DEF_30FIX_FRAME		0
#define BITRATE_1080P 	MBYTES(6)
#define BITRATE_720P	(2400 * 1024)
#define OFFS_CFG_FILE_NAME		"offs.cfg"
#endif


#define DISABLE_OFFS_RECORDING	0
#define HEADER_WRITE_INTERVAL 	1	

#if 0 // test
#define RECORDING_DURATION_SECS	(10)

#else
//#define RECORDING_DURATION_SECS	10
//#define RECORDING_DURATION_SECS	(3*60)
//#define RECORDING_DURATION_SECS	(60)
#define RECORDING_DURATION_SECS	(1*60)
#define PRE_RECORDING_SECONDS 10
#endif

#define SAFE_POWER_ON_TIME			(10 * 1000) // sub mcu use

#define SAFETY_START_RECORDING_SECONDS	(20 * 1000)

#define 	DA_TIME_FILE_NAME				"/mnt/extsd/Time.cfg"
#define 	DA_FORMAT_FILE_NAME			"/mnt/extsd/format.txt"
#define 	DA_FORMAT_FILE_NAME2			"/mnt/extsd/VideoQualit.txt"

#define 	DA_SETUP_FILE_PATH_SD 		"/mnt/extsd/setup.XML"
#define 	DA_SETUP_FILE_PATH_BACKUP 	"/data/setup.XML"
#define 	DA_FACTORY_DEFAULT_FILE_NAME	"/mnt/extsd/factorydefault.txt"


#define 	DA_FIRMWARE_UPDATE_FILE_DIR			"/mnt/extsd/"
#define 	DA_FIRMWARE_UPDATE_FILE_NAME_FIND	"firstview_da300_fw*.img"
#define 	DA_FIRMWARE_UPDATE_FILE_NAME		"firstview_da300_fw.img"

#define 	DA_FIRMWARE_UPDATE_COMPLETE_FILE_PATH			"/data/reboot.txt"
#define 	DA_FIRMWARE_UPDATE_COMPLETE_DIR					"/data/reboot"


#define 	DA_HTTPD_R3_PATH_SYSTEM					"/datech/app/httpd_r3"
#define 	DA_HTTPD_R3_PATH_USER						"/data/bin/httpd_r3"
#define 	DA_HTTPD_R3_PATH_SD					"/mnt/extsd/httpd_r3"

#define REC_FILE_TYPE_NORMAL  'I'
#define REC_FILE_TYPE_EVENT  'E'
#define REC_FILE_TYPE_MOTION	'M'
#define REC_FILE_TYPE_BUTTON  'B'
#define REC_FILE_TYPE_TIMELAPSE  'T'
#define REC_FILE_TYPE_INPUT1  'G'
#define REC_FILE_TYPE_INPUT2  'H'

#define REC_FILE_TYPE_EVENT_INPUT1	'A'
#define REC_FILE_TYPE_EVENT_INPUT2  'B'

#define REC_FILE_TYPE_EVENT_WARNING 'W'

#define HTTP_MULTIPART_BUFFER_SIZE ( 1024 * 30 )

#endif //_datech_app_configs_h
