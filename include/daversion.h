#ifndef _DA_VERSION_H_
#define _DA_VERSION_H_


#define	BUILD_MODEL_VRHD_PAI_R								1		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_SECURITY							2		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_SECURITY_REAR_1CH 					3
#define	BUILD_MODEL_VRHD_V1V2								4		//H:1080p, M:720p, L:720p/360p
#define	BUILD_MODEL_VRHD_220S								5		//AC200 삭제, Pulse, digital IO 삭제
#define BUILD_MODEL_VRHD_SECURITY_3CH						6		//3ch

//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_PAI_R
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY_REAR_1CH
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_V1V2
//#define	BUILD_MODEL 			BUILD_MODEL_VRHD_220S
#define	BUILD_MODEL 			BUILD_MODEL_VRHD_SECURITY_3CH


#define USE_DA_PULSE  1

#define ENABLE_BOARD_CHECK	0
#define DEF_PULSE_DEBUG		1

#define DEF_BOARD_WITHOUT_DCDC		1 	// new board, without dcdc
#define DEF_SUB_MICOM_USE					1

#define DEF_PAI_R_SPD_INTERVAL	500

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
#define DEF_TEST_SERVER_USE		// LTE 모뎀 통신 test
#define DEF_USB_LTE_MODEM_USE

#elif (BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY || BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_REAR_1CH || BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_3CH)
#define __FW_VERSION__			"0.0.3"

 #if BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_3CH
	#define DA_FIRMWARE_VERSION	"VRSE/3CH" __FW_VERSION__
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
//////////////////////////////////////////DA 양산
#define DEF_SAFE_DRIVING_MONITORING		//오사카 가스향에서 사용
#define DEF_OSAKAGAS_DATALOG

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
0.0.3 : 20230925
 1) DDR CLK 변경 : 792M -> 768M (GPS Band x) => Test
 
0.0.3 : 20230724
 1) RearCAM Auto Detect
 2) pop noise 수정(mono -> stereo)
 
0.0.1 : 20230406
 1) initial version
 

*/

#endif // _DA_VERSION_H_
