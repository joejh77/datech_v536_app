// ConfigTextFile.h: interface for the CConfigText class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONFIG_TEXT_FILE_H__INCLUDED_)
#define AFX_CONFIG_TEXT_FILE_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"
#include "daversion.h"

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                Definition

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#define DEF_VIDEO_QUALITY_ONLY	1			//Resolution High 1080, Middle/low 720

#define DEF_VIDEO_FIX_BITRATE		0

#define CONFIG_USE_IR_LED		0

#define CONFIG_MAX_CAMERA		2
#define CONFIG_MAX_RESOLUTION	1080 // QVGA = 320, VGA = 6400, D1 = 720, 720P = 1280, 1080P = 1920


#define DEF_DEFAULT_GSENSOR_SENS						2 // 5 => 2 (1.0.0)

//#ifdef TARGET_CPU_V536
//#define DEF_DEFAULT_VIDEOQUALITY						CFG_VIDEO_QUALITY_LOW
//#else
#define DEF_DEFAULT_VIDEOQUALITY						CFG_VIDEO_QUALITY_LOW
//#endif

#define DEF_DEFAULT_EVENT_SPACE_MODE			1

#define DEF_DEFAULT_SSID_										"driverecorder"
#define DEF_DEFAULT_SSID_STRING							DEF_DEFAULT_SSID_ "_sn"
#define DEF_DEFAULT_SSID_FMT								DEF_DEFAULT_SSID_ "_%u"
#define DEF_DEFAULT_SSID_TMP_PATH					"/tmp/" DEF_DEFAULT_SSID_STRING

//----------------------------------------------------------------------


#ifndef ON
 #define 	ON		1
 #endif

 #ifndef OFF
  #define OFF 	0
 #endif
 
enum
{
#if RES_VGA_MODE_USE
	RES_QVGA = 0,
	RES_VGA,
#endif	
	RES_D1,
	RES_HD,
	RES_USER
};


#define LOW_VOLTAGE_MAX_STEP			10
#define LOW_VOLTAGE_MIN_LEVEL			11.6


#define DEFAULT_LOW_VOLTAGE_DETECTED	12.2



//----------------------------------------------------------------------
//Time
typedef struct{
    int    	nYear;
    int    	nMonth;
    int		nDate;
    int     nHour;
    int     nMinute;
    int     nSec;
	int		nTimeSet;
}ST_CFG_TIME, *LPST_CFG_TIME;

typedef struct{
    int    	nSpeed;	//Km
    int    	nTime;	//Sec
    int		nAlarm;	//Sec
}ST_CFG_OVSPEED, *LPST_CFG_OVSPEED;
//-----------------------------------------------------------------------

#define GSENSOR_TRIGGER_LEVEL_MAX 	10
#define CFG_EVENT_MODE_MAX 2
#define DEF_MAX_TETHERING_INFO			10
enum {
	CFG_RESOLUTION_720P = 0,
	CFG_RESOLUTION_1080P,
	CFG_RESOLUTION_360P,
	CFG_RESOLUTION_480I,
	CFG_RESOLUTION_END
};

enum {
	CFG_VIDEO_QUALITY_LOW = 0,
	CFG_VIDEO_QUALITY_MIDDLE,
	CFG_VIDEO_QUALITY_HIGH,

	CFG_VIDEO_QUALITY_END
};

enum {
	CFG_REC_MODE_NORMAL = 0,
	CFG_REC_MODE_EVENT,
	CFG_REC_MODE_TIMELAPSE,
	
	CFG_REC_MODE_END
};

typedef struct{
	char		strName[128];
	char		strFWVersion[64];
	char    strDriverCode[64];
	int		iGsensorSensi;		// 1(Insensitivity)  ~ 10 (Sensitivity)
	int		iGmt;
	int		iAudioRecEnable;
	int		iSpeakerVol;		//0(off) ~ 5
#if !DEF_VIDEO_QUALITY_ONLY		
	int		iFrontResolution; 	// 0 720p, 1 1080p
	int		iRearResolution; 		// 0 720p, 1 1080p
#endif	
	int		iVideoQuality; 	// 0 LOW, 1 MIDDLE, 2 HIGH 
	int		iEventMode;		// A 0%, B 10%, C 20%
	int 		iRecordingMode;		//0 Normal, Event,Time-lapse
	bool		bAutoPmEnable;
	int		iAutoPmEnterTime;
	int		iPmImpactSensi;		// 1(Insensitivity)  ~ 10 (Sensitivity)
	bool		bPmMotionEnable;
	int		iPmMotionSensi;		// 1~6
	int		iEventMotionSensi;	// 1~6
	int		iCarBatVoltage;	//0: 12V(auto), 1: 24V
	int		iCarBatVoltageCalib;
	bool		bCarBatSafeEnable;
	int		iCarBatSafeVoltage;	//60(6v) ~ 255(25.5v)
	bool		bTempSafeEnable;
	int		iTempSafeValue;
	int		iTempStableValue;
	bool		bSecurityLEDEnable;
	int		iSecurityLEDPeriod;
	bool		bSecurityLEDMDWarning;
	int		iSecurityLEDOperating;
	int		iLcdOffTime;
	int		iVideoOut;	//TV, LCD
	bool		bFactoryReset;
	int		iPulseReset;  // 0 or 2(manual)이면 초기화 완료

	bool		bBrake; //FALSE p/d, TRUE p/u
	bool		bInput1;
	bool		bInput2;
	int		iBlink;  //vaule 1 : 1초 / value 2 : 1.5초 / value 3 : 2초 선택 가능

	int		iEngine_cylinders;  // 4, 6, 8 기통
	int		iSpeed_limit_kmh; //20 km/h
	
	bool 	bOsdSpeed; // TRUE "000km/h"표시
	bool 	bOsdRpm; // TRUE "0000rpm"표시
	
	bool		bTestMode; 		//hidden
	bool		bDebugMode;	//hidden

	//internal use
	double 	dPulseParam;

	char		strLastSetupTime[32]; //%4d-%02d-%02d %02d:%02d:%02d

	bool		bWifiSet;
	
	char		strApSsid[64];
	char		strWiFiSsid[DEF_MAX_TETHERING_INFO][64];
	char		strWiFiPassword[DEF_MAX_TETHERING_INFO][68];
	char		strTelNumber[DEF_MAX_TETHERING_INFO][16];
	char		strSeriarNo[64];

	char		strApplication_IP[64];
	int		iApplication_PORT;
	int		iDebugServer_PORT;	//hidden, Application_IP use

	char		strCloudServerName[64]; //hidden, mar-i.com
	char		strCloudServerApiUrl[128]; //hidden, /driverecorder/api_driverecorder/firstview/1/insert_driverecorder.php
	int		iCloudServerPort;				//hidden, 80
	bool		bCloudServerSSL;

	//Log, Safety
	int		G_Sensi_B;
	int		G_Sensi_C;
	int		iSuddenAccelerationSensi;		// 1(Insensitivity)  ~ 10 (Sensitivity)
	int		iSuddenDeaccelerationSensi;	// 1(Insensitivity)  ~ 10 (Sensitivity)
	int		iRapidRotationSensi;					// 1(Insensitivity)  ~ 10 (Sensitivity)
	ST_CFG_OVSPEED		Overspeed;	//General overspeed
	ST_CFG_OVSPEED		Fastspeed;	//Fast overspeed
	ST_CFG_OVSPEED		Highspeed;	// High-Speed
 }ST_CFG_DAVIEW, *LPST_CFG_DAVIEW;


typedef struct{
    char		strId[64];
	char		strPw[68];
	char		strNo[16];
}ST_TETHERING_INFO, *LPST_TETHERING_INFO;

typedef std::list<ST_TETHERING_INFO>					TETHERING_INFO_POOL;
typedef TETHERING_INFO_POOL::iterator					ITER_TETHERING_INFO;
//------------------------------------------------------------------------

//#define BLACKBOX_CONFIG_FOLDER		_T("Config\\")
//#define BLACKBOX_CONFIG_FILE_NAME		_T("Config.ini")
#define BLACKBOX_CONFIG_FOLDER		""
#define BLACKBOX_CONFIG_FILE_NAME		"setup.xml"

#define BLACKBOX_CONFIG_FILE		(BLACKBOX_CONFIG_FOLDER BLACKBOX_CONFIG_FILE_NAME)

class CConfigText  
{
public:

	CConfigText(void);
	virtual ~CConfigText(void);
	
	static BOOL Load(LPST_CFG_DAVIEW blackbox_config);
	
	static BOOL Load(const char * cfg_file_name, LPST_CFG_DAVIEW blackbox_config);
	static BOOL Load(const char * cfg_file_name, LPST_CFG_TIME spTime);
	

	static BOOL Save(LPST_CFG_DAVIEW spConfig);
	static BOOL Save(const char * cfg_file_name, LPST_CFG_DAVIEW spConfig);
	static BOOL Save(const char * cfg_file_name, LPST_CFG_TIME spTime);
	static BOOL updateTethering(LPST_CFG_DAVIEW spConfig, const char * ssid, const char * password, const char * number);
	
	static BOOL MakeDoctype(const char * cfg_file_name);
	static BOOL CfgDefaultSet(LPST_CFG_DAVIEW pCfg);
	static BOOL CfgDefaultSet(const char * cfg_file_name);
protected:
	static BOOL CfgParserFile(const char * cfg_file_name, LPST_CFG_DAVIEW blackbox_config);
	static BOOL CfgPrintfToFile(FILE* fp, const char *fmt, ... );
};

#endif // !defined(AFX_CONFIG_TEXT_FILE_H__INCLUDED_)
