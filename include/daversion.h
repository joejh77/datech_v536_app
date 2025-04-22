#ifndef _DA_VERSION_H_
#define _DA_VERSION_H_


#define	BUILD_MODEL_VRHD_PAI_R								1		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_SECURITY							2		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_SECURITY_REAR_1CH 					3
#define	BUILD_MODEL_VRHD_V1V2								4		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_220S								5		//AC200 ????, Pulse, digital IO ????
#define BUILD_MODEL_VRHD_SECURITY_3CH						6		//3ch

//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_PAI_R
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY_REAR_1CH
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_V1V2
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_220S
#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY_3CH


#define USE_DA_PULSE  				1

#define ENABLE_BOARD_CHECK			0
#define DEF_PULSE_DEBUG				1

#define DEF_BOARD_WITHOUT_DCDC		1 	// new board, without dcdc
#define DEF_SUB_MICOM_USE			1

#define DEF_PAI_R_SPD_INTERVAL		500

#if ENABLE_BOARD_CHECK
#define 	DA_MODEL_NAME		"DA_DEMO CHK"
#else
#ifdef TARGET_CPU_V536
#define 	DA_MODEL_NAME		"DA330 S"
#else
#define 	DA_MODEL_NAME		"DA200 S"
#endif
#endif

#ifdef HTTPD_EMBEDDED
#define DEF_DEBUG_SERIAL_LOG_FILE_SAVE 1
#else
#define DEF_DEBUG_SERIAL_LOG_FILE_SAVE 0
#endif



#if (BUILD_MODEL == BUILD_MODEL_VRHD_PAI_R)
#define __FW_VERSION__			"0.0.30"
#define	DA_FIRMWARE_VERSION	"VRHD/" __FW_VERSION__

#define ENABLE_AVI_SCRAMBLING 0

#define DEF_PAI_R_DATA_SAVE

#define DEF_LOW_FRONT_720P
#define DEF_30FIX_FRAME				0
#define REAR_1080P 					0
#define ENABLE_DATA_LOG	 			0

#define DEF_SAFE_DRIVING_MONITORING
#define DEF_OSAKAGAS_DATALOG
#define DEF_TEST_SERVER_USE		// LTE ?? ??? test
#define DEF_USB_LTE_MODEM_USE

#elif (BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY || BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_REAR_1CH || BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_3CH)
#define __FW_VERSION__			"1.1.0"		// 1080p 2.0.0

 #if BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_3CH
	#define DA_FIRMWARE_VERSION	"VR3CH/" __FW_VERSION__
 #elif BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY
	#define DA_FIRMWARE_VERSION	"VRSE/" __FW_VERSION__
 #else
 	#define DA_FIRMWARE_VERSION	"VRSE/1CH_" __FW_VERSION__
	#define DEF_REAR_CAMERA_ONLY
 #endif

#define DEF_VRSE_SECURITY_FILE_EXT		"fvfs"		//firstview file system
#define DEF_VRSE_FVFS_HEADER		"FVFS   CDR MOVIE"
#define ENABLE_AVI_SCRAMBLING 		1
//#define AVI_SCRAMBLE_CODES    "0x6461746563680635b5695b47d888d1590934138834535c9f63b6907357e724eb"
#define AVI_SCRAMBLE_CODES    "0x000000"

#define DEF_LOW_FRONT_720P
#define DEF_30FIX_FRAME				0
#define REAR_1080P 					0
#define ENABLE_DATA_LOG	 			1
//////////////////////////////////////////DATECH
#define DEF_SAFE_DRIVING_MONITORING			//Futaba
#define DEF_OSAKAGAS_DATALOG
#define	DEF_SAFE_DRIVING_MONITORING_ONOFF	//ONOFF  test_241127
//#define DEF_FRONT_1080P						//Front Image_1080P_241203				


#elif (BUILD_MODEL == BUILD_MODEL_VRHD_V1V2)
#define __FW_VERSION__			"0.0.27"
#define	DA_FIRMWARE_VERSION	"VRVX/" __FW_VERSION__

#define ENABLE_AVI_SCRAMBLING 		0

#define DEF_LOW_FRONT_720P
 #define DEF_30FIX_FRAME			0
#define REAR_1080P 					0
#define ENABLE_DATA_LOG	 			1


#elif (BUILD_MODEL == BUILD_MODEL_VRHD_220S)
#define __FW_VERSION__			"1.0.0"

#define DA_FIRMWARE_VERSION	"VRSS/" __FW_VERSION__


#define DEF_VRSE_SECURITY_FILE_EXT		"fvfs"		//firstview file system
#define DEF_VRSE_FVFS_HEADER		"FVFS   CDR MOVIE"
#define ENABLE_AVI_SCRAMBLING 1
//#define AVI_SCRAMBLE_CODES    "0x6461746563680635b5695b47d888d1590934138834535c9f63b6907357e724eb"
#define AVI_SCRAMBLE_CODES    "0x000000"

#define DEF_LOW_FRONT_720P
#define DEF_30FIX_FRAME				0
#define REAR_1080P 					0
#define ENABLE_DATA_LOG	 			1


#define DEF_DO_NOT_USE_DIGITAL_IO

#undef USE_DA_PULSE
#define USE_DA_PULSE  0
#endif


//////////////////////////////////////////////////////
//Revision Note
/*
1.1.0 : 20250219
 1) Standard F/W ver. change 1.0.0 => 1.1.0 

2.0.0 : 20241218
 1)rearcam bitrate : 2.25M

2.0.0 : 20241203_reboot
 1) Front : 2M / Rear : 1M
 2) Low : 6fps
 3) SafeMonitoring ON/OFF

1.0.0 : 20241126
 1) SafeMonitoring value change

1.0.0 : 20241021
 1) Initial MS version

0.0.7 : 20240924 (Futaba ver.)
 1) Low : bitrate 4M -> 3.5M 로 수정
 2) Keyframe interval restoration
 3) Active the gps_tx pin
 4) Always on the GPS PWR of PMIC

0.0.6 : 20240805
 1) Picture Number 빠지는 부분 API 수정 (한상은대표)
 2) imx307 sensor window, frame_delay 수정

0.0.5 : 20240522
 1) Viewer scroll bar 45초에서 next file 넘어가는 문제 수정
 2) Rear CAM1 detect 수정

0.0.4 : 20240416
 1) Pulse count 오차 수정

0.0.3 : 20230925
 1) DDR CLK 변경 : 792M -> 768M (GPS Band x) => Test
 
0.0.3 : 20230724
 1) RearCAM Auto Detect
 2) pop noise 수정(mono -> stereo)
 
0.0.1 : 20230406
 1) initial version
 

*/

#endif // _DA_VERSION_H_
