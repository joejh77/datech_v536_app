// ConfigTextFile.cpp: implementation of the CTParser class.
//
//////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include "datools.h"
#include "sysfs.h"
#include "tinyxml.h"
#include "ConfigTextFile.h"


#define BoolAttribute(b)	b ? bool_true : bool_false

#define G_SENSOR_DEFAULT_TRIGGERLEVEL 		0.3f

#define ZONE_CFG_PRINT	1
#define ZONE_CFG_ERROR 1

FILE* FOpen( const char* filename, const char* mode )
{
	FILE* fp = 0;
	
	#if defined(_MSC_VER) && (_MSC_VER >= 1400 )
		
		errno_t err = fopen_s( &fp, filename, mode);
		if (err )
			return 0;
	#else
		fp =  fopen( filename, mode );
	#endif
	if(!fp) {
		DEBUGMSG(ZONE_CFG_ERROR, ("%s(): %s file open error!\r\n", __func__, filename));
		return 0;
	}

	return fp;
}

CConfigText::CConfigText(void)
{
}

CConfigText::~CConfigText(void)
{
}

BOOL CConfigText::Load(LPST_CFG_DAVIEW blackbox_config)
{
	return Load(BLACKBOX_CONFIG_FILE, blackbox_config);
}

BOOL CConfigText::Load(const char * cfg_file_name, LPST_CFG_DAVIEW blackbox_config)
{
	return CfgParserFile(cfg_file_name, blackbox_config);
}

BOOL CConfigText::Load(const char * cfg_file_name, LPST_CFG_TIME spTime)
{
	long length = 0;

	FILE* file = FOpen(cfg_file_name,"r");

	if(!file)
		return false;
	
	fseek( file, 0, SEEK_END );
	length = ftell( file );
	fseek( file, 0, SEEK_SET );

	// Strange case, but good to handle up front.
	if ( length <= 0 )
	{
		fclose(file);
		return false;
	}

	// Subtle bug here. TinyXml did use fgets. But from the XML spec:
	// 2.11 End-of-Line Handling
	// <snip>
	// <quote>
	// ...the XML processor MUST behave as if it normalized all line breaks in external 
	// parsed entities (including the document entity) on input, before parsing, by translating 
	// both the two-character sequence #xD #xA and any #xD that is not followed by #xA to 
	// a single #xA character.
	// </quote>
	//
	// It is not clear fgets does that, and certainly isn't clear it works cross platform. 
	// Generally, you expect fgets to translate from the convention of the OS to the c/unix
	// convention, and not work generally.

	/*
	while( fgets( buf, sizeof(buf), file ) )
	{
		data += buf;
	}
	*/

	char* buf = new char[ length+1 ];
	buf[0] = 0;

	if ( fread( buf, length, 1, file ) != 1 ) {
		delete [] buf;
		fclose(file);
		return false;
	}

	if ( TIXML_SSCANF( buf, "%04d %02d %02d %02d %02d %02d", &spTime->nYear, &spTime->nMonth, &spTime->nDate, &spTime->nHour, &spTime->nMinute, &spTime->nSec) == 6 )
		DEBUGMSG(1, ("%s(): %04d %02d %02d %02d %02d %02d\r\n", __func__, spTime->nYear, spTime->nMonth, spTime->nDate, spTime->nHour, spTime->nMinute, spTime->nSec));
	else
		DEBUGMSG(ZONE_CFG_ERROR, ("%s(): error!\r\n", __func__));

	delete[] buf;
	fclose(file);
	return TRUE;
}

BOOL CConfigText::Save(LPST_CFG_DAVIEW spConfig)
{
	return Save(BLACKBOX_CONFIG_FILE, spConfig);
}

BOOL CConfigText::Save(const char * cfg_file_name, LPST_CFG_TIME spTime)
{
	FILE* fp = FOpen(cfg_file_name, "w");

	if(fp){	
		CfgPrintfToFile(fp, "%04d %02d %02d %02d %02d %02d", spTime->nYear, spTime->nMonth, spTime->nDate, spTime->nHour, spTime->nMinute, spTime->nSec);
		fclose(fp);
		
		return TRUE;
	}

	return FALSE;
}

BOOL CConfigText::Save(const char * cfg_file_name, LPST_CFG_DAVIEW spConfig)
{
//	int ch;
//	int ret;

//	bool		b;
//	int		i;
//	double		d;
//	const char*		c;
	const char *bool_true = "TRUE";
	const char *bool_false = "FALSE";

	TiXmlDocument xmlDoc;

	if ( 0 != access(cfg_file_name, R_OK ) )                              // ?????? ????
		MakeDoctype(cfg_file_name);
	
	if(!xmlDoc.LoadFile(cfg_file_name))
		return FALSE;
	
	TiXmlElement* pSetup = xmlDoc.FirstChildElement("setup");

	if(!pSetup) {
		xmlDoc.Clear();
		return FALSE;
	}

	TiXmlElement* pEleItem = pSetup->FirstChildElement("item");
	while( pEleItem )
	{
		const char* lpszId = pEleItem->Attribute( "id" );

		if(!strcmp(lpszId, "Name")){
			//c = pEleItem->Attribute( "value" );
			//if(c) strcpy(spConfig->strName, c);
			pEleItem->SetAttribute( "value", /*spConfig->strName*/ DA_MODEL_NAME);
		}
		else if(!strcmp(lpszId, "FWVersion")){
			//c = pEleItem->Attribute( "value" );
			//if(c) strcpy(spConfig->strFWVersion, c);
			pEleItem->SetAttribute( "value", /*spConfig->strFWVersion*/ DA_FIRMWARE_VERSION);
		}
		else if(!strcmp(lpszId, "DriverCode")){
			pEleItem->SetAttribute( "value", spConfig->strDriverCode);
		}
		else if(!strcmp(lpszId, "ProductSerialNo")){
			const char * tmp_sn_path = DEF_DEFAULT_SSID_TMP_PATH;
			char sn[16] = {0,};
			strcpy(sn, "0");
			
			if(access(tmp_sn_path, R_OK ) == 0){
				u32 number  = 0;
				if(sysfs_scanf(tmp_sn_path, "%u", &number)){
					sprintf(sn, "%u", number);
				}
				else
					strcpy(sn, "0");
			}
			
			pEleItem->SetAttribute( "value", (const char *)sn);
		}
		else if(!strcmp(lpszId, "GSensorSensi")){
			pEleItem->SetAttribute( "value", spConfig->iGsensorSensi);
		}
		else if(!strcmp(lpszId, "GMT")){
			pEleItem->SetAttribute( "value", spConfig->iGmt);
		}
		else if(!strcmp(lpszId, "AudioRecEnable")){
			if(spConfig->iAudioRecEnable)
				pEleItem->SetAttribute( "value", "TRUE");
			else
				pEleItem->SetAttribute( "value", "FALSE");
		}
		else if(!strcmp(lpszId, "SpeakerVol")){
			pEleItem->SetAttribute( "value", spConfig->iSpeakerVol);
		}
#if !DEF_VIDEO_QUALITY_ONLY		
		else if(!strcmp(lpszId, "FrontResolution")){
			if(spConfig->iFrontResolution==CFG_RESOLUTION_1080P)
				pEleItem->SetAttribute( "value", "1080p" );
			else
				pEleItem->SetAttribute( "value", "720p" );
		}
		else if(!strcmp(lpszId, "RearResolution")){
			if(spConfig->iRearResolution==CFG_RESOLUTION_1080P)
				pEleItem->SetAttribute( "value", "1080p" );
			else
				pEleItem->SetAttribute( "value", "720p" );
		}
#endif		
		else if(!strcmp(lpszId, "VideoQuality")){
			if(spConfig->iVideoQuality==2)
				pEleItem->SetAttribute( "value", "HIGH" );
			else if(spConfig->iVideoQuality==0)
				pEleItem->SetAttribute( "value", "LOW" );
			else
				pEleItem->SetAttribute( "value", "MIDDLE" );
		}
		else if(!strcmp(lpszId, "EventMode")){
			if(spConfig->iEventMode == 0)
				pEleItem->SetAttribute( "value", "A" );
			else if(spConfig->iEventMode == 1)
				pEleItem->SetAttribute( "value", "B" );
			else 
				pEleItem->SetAttribute( "value", "C" );
		}
		else if(!strcmp(lpszId, "RecordingMode")){
			pEleItem->SetAttribute( "value", spConfig->iRecordingMode);
		}
		else if(!strcmp(lpszId, "AutoPmEnable")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bAutoPmEnable));
		}
		else if(!strcmp(lpszId, "AutoPmEnterTime")){
			pEleItem->SetAttribute( "value", spConfig->iAutoPmEnterTime);
		}
		else if(!strcmp(lpszId, "PmImpactSensi")){
			pEleItem->SetAttribute( "value", spConfig->iPmImpactSensi);
		}
		else if(!strcmp(lpszId, "PmMotionEnable")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bPmMotionEnable));
		}
		else if(!strcmp(lpszId, "PmMotionSensi")){
			pEleItem->SetAttribute( "value", spConfig->iPmMotionSensi);
		}
		else if(!strcmp(lpszId, "EventMotionSensi")){
			pEleItem->SetAttribute( "value", spConfig->iEventMotionSensi);
		}
		else if(!strcmp(lpszId, "CarBatVoltage")){
			if( spConfig->iCarBatVoltage )
				pEleItem->SetAttribute( "value", "24V" );
			else
				pEleItem->SetAttribute( "value", "12V" );
		}
		else if(!strcmp(lpszId, "CarBatVoltCalib")){
			pEleItem->SetAttribute( "value", spConfig->iCarBatVoltageCalib);
		}
		else if(!strcmp(lpszId, "CarBatSafeEnable")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bCarBatSafeEnable));
		}
		else if(!strcmp(lpszId, "CarBatSafeVoltage")){
			pEleItem->SetAttribute( "value", spConfig->iCarBatSafeVoltage);
		}
		else if(!strcmp(lpszId, "TempSafeEnable")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bTempSafeEnable));
		}
		else if(!strcmp(lpszId, "TempSafeValue")){
			pEleItem->SetAttribute( "value", spConfig->iTempSafeValue);
		}
		else if(!strcmp(lpszId, "TempStableValue")){
			pEleItem->SetAttribute( "value", spConfig->iTempStableValue);
		}
		else if(!strcmp(lpszId, "SecurityLEDEnable")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bSecurityLEDEnable));
		}
		else if(!strcmp(lpszId, "SecurityLEDPeriod")){
			pEleItem->SetAttribute( "value", spConfig->iSecurityLEDPeriod);
		}
		else if(!strcmp(lpszId, "SecurityLEDMDWarning")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bSecurityLEDMDWarning));
		}
		else if(!strcmp(lpszId, "SecurityLEDOperating")){
			pEleItem->SetAttribute( "value", spConfig->iSecurityLEDOperating);
		}
		else if(!strcmp(lpszId, "LcdOffTime")){
			pEleItem->SetAttribute( "value", spConfig->iLcdOffTime);
		}
		else if(!strcmp(lpszId, "VideoOut")){
			pEleItem->SetAttribute( "value", spConfig->iVideoOut);
		}
		else if(!strcmp(lpszId, "FactoryReset")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bFactoryReset));
		}
		else if(!strcmp(lpszId, "PulseReset")){
			pEleItem->SetAttribute( "value", spConfig->iPulseReset);
		}
		else if(!strcmp(lpszId, "BRAKE")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bBrake));
		}
		else if(!strcmp(lpszId, "INPUT1")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bInput1));
		}
		else if(!strcmp(lpszId, "INPUT2")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bInput2));
		}
		else if(!strcmp(lpszId, "BLINK")){
			pEleItem->SetAttribute( "value", spConfig->iBlink);
		}
		else if(!strcmp(lpszId, "EngineCylinders")){
			pEleItem->SetAttribute( "value", spConfig->iEngine_cylinders);
		}
		else if(!strcmp(lpszId, "SpeedLimitValue")){
			pEleItem->SetAttribute( "value", spConfig->iSpeed_limit_kmh);
		}
		else if(!strcmp(lpszId, "OSDSpeed")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bOsdSpeed));
		}
		else if(!strcmp(lpszId, "OSDRpm")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bOsdRpm));
		}
////++{ for VMD		
		else if(!strcmp(lpszId, "G_Sensi_B")){
			pEleItem->SetAttribute( "value", spConfig->G_Sensi_B);
		}
		else if(!strcmp(lpszId, "G_Sensi_C")){
			pEleItem->SetAttribute( "value", spConfig->G_Sensi_C);
		}
#ifdef DEF_SAFE_DRIVING_MONITORING_ONOFF
		else if(!strcmp(lpszId, "SafeMonitoring")){ //test_241127
			pEleItem->SetAttribute( "value", spConfig->bsafemonitoring);
		}
#endif
		else if(!strcmp(lpszId, "SuddenAccelerationSensi")){
			pEleItem->SetAttribute( "value", spConfig->iSuddenAccelerationSensi);
		}
		else if(!strcmp(lpszId, "SuddenDeaccelerationSensi")){
			pEleItem->SetAttribute( "value", spConfig->iSuddenDeaccelerationSensi);
		}
		else if(!strcmp(lpszId, "RapidRotationSensi")){
			pEleItem->SetAttribute( "value", spConfig->iRapidRotationSensi);
		}
		else if(!strcmp(lpszId, "OverspeedSpeed")){
			pEleItem->SetAttribute( "value", spConfig->Overspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "OverspeedTime")){
			pEleItem->SetAttribute( "value", spConfig->Overspeed.nTime);
		}
		else if(!strcmp(lpszId, "OverspeedAlarm")){
			pEleItem->SetAttribute( "value", spConfig->Overspeed.nAlarm);
		}
		else if(!strcmp(lpszId, "FastspeedSpeed")){
			pEleItem->SetAttribute( "value", spConfig->Fastspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "FastspeedTime")){
			pEleItem->SetAttribute( "value", spConfig->Fastspeed.nTime);
		}
		else if(!strcmp(lpszId, "FastspeedAlarm")){
			pEleItem->SetAttribute( "value", spConfig->Fastspeed.nAlarm);
		}
		else if(!strcmp(lpszId, "HighspeedSpeed")){
			pEleItem->SetAttribute( "value", spConfig->Highspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "HighspeedTime")){
			pEleItem->SetAttribute( "value", spConfig->Highspeed.nTime);
		}
////++}		
		else if(!strcmp(lpszId, "PulseParam")){
			pEleItem->SetDoubleAttribute( "value", spConfig->dPulseParam);
		}
		else if(!strcmp(lpszId, "LastSetupTime")){
			pEleItem->SetAttribute( "value", spConfig->strLastSetupTime);
		}
		else if(!strcmp(lpszId, "WiFi_SET")){
			pEleItem->SetAttribute( "value", BoolAttribute(spConfig->bWifiSet));
		}
		else if(!strcmp(lpszId, "AP_SSID")){
			pEleItem->SetAttribute( "value", spConfig->strApSsid);
		}
		else if(!strcmp(lpszId, "WiFi_SSID")){
			pEleItem->SetAttribute( "value", spConfig->strWiFiSsid[0]);
		}
		else if(!strcmp(lpszId, "WiFi_PW")){
			pEleItem->SetAttribute( "value", spConfig->strWiFiPassword[0]);
		}
		else if(!strcmp(lpszId, "TEL_NO")){
			pEleItem->SetAttribute( "value", spConfig->strTelNumber[0]);
		}
//
		else if(!strncmp(lpszId, "WiFi_SSID_USER", 14)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[14]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO)
				pEleItem->SetAttribute( "value", spConfig->strWiFiSsid[user_no-1]);
		}
		else if(!strncmp(lpszId, "WiFi_PW_USER", 12)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[12]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO)
				pEleItem->SetAttribute( "value", spConfig->strWiFiPassword[user_no-1]);
		}
		else if(!strncmp(lpszId, "TEL_NO_USER", 11)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[11]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO)
				pEleItem->SetAttribute( "value", spConfig->strTelNumber[user_no-1]);
		}
//
		else if(!strcmp(lpszId, "SeriarNo")){
			pEleItem->SetAttribute( "value", spConfig->strSeriarNo);
		}
		else if(!strcmp(lpszId, "Application_IP")){
			pEleItem->SetAttribute( "value", spConfig->strApplication_IP);
		}
		else if(!strcmp(lpszId, "Application_PORT")){
			pEleItem->SetAttribute( "value", spConfig->iApplication_PORT);
		}
		pEleItem = pEleItem->NextSiblingElement();
	}

#if 0 //???????? Microsoft edge???? Open ???? ??? ????? ??u????, ??????? ????
	//8K fix size use
	{
		FILE *fp = fopen(cfg_file_name, "r+");
		char temp[8 * 1024] = { 0, };
		fwrite((void *)temp, 1, sizeof(temp), fp);
		fseek( fp, 0, SEEK_SET );
		xmlDoc.SaveFile(fp);
		fclose(fp);
	}
#else		
	xmlDoc.SaveFile(cfg_file_name);
#endif
	xmlDoc.Clear();
	
	return TRUE;
}

BOOL CConfigText::updateTethering(LPST_CFG_DAVIEW spConfig, const char * ssid, const char * password, const char * number)
{
	ST_TETHERING_INFO tr_info;
	TETHERING_INFO_POOL list_tr;
	BOOL bNew_ssid = TRUE;
	int i;
		
	for(i = 0; i < DEF_MAX_TETHERING_INFO; i++){
		if(strcmp(spConfig->strWiFiSsid[i], ssid) == 0 && \
			strcmp(spConfig->strWiFiPassword[i], password) == 0 && \
			strcmp(spConfig->strTelNumber[i], number) == 0){
			bNew_ssid = FALSE;
		}
		else if (strlen(spConfig->strWiFiSsid[i])){
			strcpy(tr_info.strId, spConfig->strWiFiSsid[i]);
			strcpy(tr_info.strPw, spConfig->strWiFiPassword[i]);
			strcpy(tr_info.strNo, spConfig->strTelNumber[i]);
			list_tr.push_back(tr_info);
		}
	}

	strcpy(tr_info.strId, ssid);
	strcpy(tr_info.strPw, password);
	strcpy(tr_info.strNo, number);
	list_tr.push_front(tr_info);

	ITER_TETHERING_INFO iTI_S = list_tr.begin();
	int tr_size = list_tr.size();
			
	for(i = 0; i < DEF_MAX_TETHERING_INFO ; i++, iTI_S++){
		if(i < tr_size){
			strcpy(spConfig->strWiFiSsid[i], iTI_S->strId);
			strcpy(spConfig->strWiFiPassword[i], iTI_S->strPw);
			strcpy(spConfig->strTelNumber[i], iTI_S->strNo);
			DEBUGMSG(ZONE_CFG_PRINT, ("user %d [SSID: %s, TEL: %s] \r\n", i, spConfig->strWiFiSsid[i], spConfig->strTelNumber[i]));
		}
		else {
			strcpy(spConfig->strWiFiSsid[i], "");
			strcpy(spConfig->strWiFiPassword[i], "");
			strcpy(spConfig->strTelNumber[i], "");
		}
	}

	list_tr.clear();

	return 0;
}

BOOL CConfigText::CfgParserFile(const char * cfg_file_name, LPST_CFG_DAVIEW spConfig)
{
//	int ch;
	
	/* Some temporary variables to hold query results */
	bool		b;
//	int		i;
//	double		d;
	const char*		c;

	TiXmlDocument xmlDoc;

	if(!xmlDoc.LoadFile(cfg_file_name))
		return FALSE;
	
	TiXmlElement* pSetup = xmlDoc.FirstChildElement("setup");

	if(!pSetup) {
		xmlDoc.Clear();
		return FALSE;
	}
	
	DEBUGMSG(ZONE_CFG_PRINT, ("-------------------------------\n"));

	TiXmlElement* pEleItem = pSetup->FirstChildElement("item");
	while( pEleItem )
	{
		const char* lpszId = pEleItem->Attribute( "id" );

		if(!strcmp(lpszId, "Name")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strName, c);
		}
		else if(!strcmp(lpszId, "FWVersion")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strFWVersion, c);
		}
		else if(!strcmp(lpszId, "DriverCode")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strDriverCode, c);
		}
		else if(!strcmp(lpszId, "GSensorSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iGsensorSensi);
		}
		else if(!strcmp(lpszId, "GMT")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iGmt);
		}
		else if(!strcmp(lpszId, "AudioRecEnable")){
			pEleItem->QueryBoolAttribute( "value", &b);
			spConfig->iAudioRecEnable = (int)b;
		}
		else if(!strcmp(lpszId, "SpeakerVol")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSpeakerVol);
		}
#if !DEF_VIDEO_QUALITY_ONLY		
		else if(!strcmp(lpszId, "FrontResolution")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "1080p")) spConfig->iFrontResolution = CFG_RESOLUTION_1080P;
				else  spConfig->iFrontResolution = 0;
			}
		}
		else if(!strcmp(lpszId, "RearResolution")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "1080p")) spConfig->iRearResolution = CFG_RESOLUTION_1080P;
				else  spConfig->iRearResolution = 0;
			}
		}
#endif		
		else if(!strcmp(lpszId, "VideoQuality")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "LOW")) spConfig->iVideoQuality = 0;
				else if(!strcmp(c, "HIGH")) spConfig->iVideoQuality = 2;
				else  spConfig->iVideoQuality = 1;
			}
		}
		else if(!strcmp(lpszId, "EventMode")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "A")) spConfig->iEventMode = 0;
				else if(!strcmp(c, "C")) spConfig->iEventMode = 2;
				else spConfig->iEventMode = 1;
			}
		}
		else if(!strcmp(lpszId, "RecordingMode")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iRecordingMode);
		}
		else if(!strcmp(lpszId, "AutoPmEnable")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bAutoPmEnable);
		}
		else if(!strcmp(lpszId, "AutoPmEnterTime")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iAutoPmEnterTime);
		}
		else if(!strcmp(lpszId, "PmImpactSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iPmImpactSensi);
		}
		else if(!strcmp(lpszId, "PmMotionEnable")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bPmMotionEnable);
		}
		else if(!strcmp(lpszId, "PmMotionSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iPmMotionSensi);
		}
		else if(!strcmp(lpszId, "EventMotionSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iEventMotionSensi);
		}
		else if(!strcmp(lpszId, "CarBatVoltage")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "24V") || !strcmp(c, "24")) spConfig->iCarBatVoltage = 1;
				else spConfig->iCarBatVoltage = 0;
			}
		}
		else if(!strcmp(lpszId, "CarBatVoltCalib")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iCarBatVoltageCalib);
		}
		else if(!strcmp(lpszId, "CarBatSafeEnable")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bCarBatSafeEnable);
		}
		else if(!strcmp(lpszId, "CarBatSafeVoltage")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iCarBatSafeVoltage);
		}
		else if(!strcmp(lpszId, "TempSafeEnable")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bTempSafeEnable);
		}
		else if(!strcmp(lpszId, "TempSafeValue")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iTempSafeValue);
		}
		else if(!strcmp(lpszId, "TempStableValue")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iTempStableValue);
		}
		else if(!strcmp(lpszId, "SecurityLEDEnable")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bSecurityLEDEnable);
		}
		else if(!strcmp(lpszId, "SecurityLEDPeriod")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSecurityLEDPeriod);
		}
		else if(!strcmp(lpszId, "SecurityLEDMDWarning")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bSecurityLEDMDWarning);
		}
		else if(!strcmp(lpszId, "SecurityLEDOperating")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSecurityLEDOperating);
		}
		else if(!strcmp(lpszId, "LcdOffTime")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iLcdOffTime);
		}
		else if(!strcmp(lpszId, "VideoOut")){
			c = pEleItem->Attribute( "value" );
			if(c) {
				if(!strcmp(c, "LCD")) spConfig->iVideoOut = 1;
				else spConfig->iVideoOut = 0;
			}
		}
		else if(!strcmp(lpszId, "FactoryReset")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bFactoryReset);
		}
		else if(!strcmp(lpszId, "PulseReset")){
			bool bReset = (bool)spConfig->iPulseReset;
			if(TIXML_SUCCESS == pEleItem->QueryBoolAttribute( "value", &bReset))
				spConfig->iPulseReset = (int)bReset;
			else
				pEleItem->QueryIntAttribute( "value", &spConfig->iPulseReset);
		}
		else if(!strcmp(lpszId, "BRAKE")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bBrake);
		}
		else if(!strcmp(lpszId, "INPUT1")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bInput1);
		}
		else if(!strcmp(lpszId, "INPUT2")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bInput2);
		}
		else if(!strcmp(lpszId, "BLINK")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iBlink);
		}
		else if(!strcmp(lpszId, "EngineCylinders")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iEngine_cylinders);
		}
		else if(!strcmp(lpszId, "SpeedLimitValue")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSpeed_limit_kmh);
		}
		else if(!strcmp(lpszId, "OSDSpeed")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bOsdSpeed);
		}
		else if(!strcmp(lpszId, "OSDRpm")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bOsdRpm);
		}
////++{ for VMD		
		else if(!strcmp(lpszId, "G_Sensi_B")){
			pEleItem->QueryIntAttribute( "value", &spConfig->G_Sensi_B);
		}
		else if(!strcmp(lpszId, "G_Sensi_C")){
			pEleItem->QueryIntAttribute( "value", &spConfig->G_Sensi_C);
		}
#ifdef DEF_SAFE_DRIVING_MONITORING_ONOFF
		else if(!strcmp(lpszId, "SafeMonitoring")){ //test_241127
			pEleItem->QueryBoolAttribute( "value", &spConfig->bsafemonitoring);
		}
#endif
		else if(!strcmp(lpszId, "SuddenAccelerationSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSuddenAccelerationSensi);
		}
		else if(!strcmp(lpszId, "SuddenDeaccelerationSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iSuddenDeaccelerationSensi);
		}
		else if(!strcmp(lpszId, "RapidRotationSensi")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iRapidRotationSensi);
		}
		else if(!strcmp(lpszId, "OverspeedSpeed")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Overspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "OverspeedTime")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Overspeed.nTime);
		}
		else if(!strcmp(lpszId, "OverspeedAlarm")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Overspeed.nAlarm);
		}
		else if(!strcmp(lpszId, "FastspeedSpeed")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Fastspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "FastspeedTime")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Fastspeed.nTime);
		}
		else if(!strcmp(lpszId, "FastspeedAlarm")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Fastspeed.nAlarm);
		}
		else if(!strcmp(lpszId, "HighspeedSpeed")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Highspeed.nSpeed);
		}
		else if(!strcmp(lpszId, "HighspeedTime")){
			pEleItem->QueryIntAttribute( "value", &spConfig->Highspeed.nTime);
		}
////++}
		else if(!strcmp(lpszId, "DebugMode")){ //Hidden
			pEleItem->QueryBoolAttribute( "value", &spConfig->bDebugMode);
		}
		else if(!strcmp(lpszId, "TestMode")){	//Hidden
			pEleItem->QueryBoolAttribute( "value", &spConfig->bTestMode);
		}
		else if(!strcmp(lpszId, "PulseParam")){
			pEleItem->QueryDoubleAttribute( "value", &spConfig->dPulseParam);
		}
		else if(!strcmp(lpszId, "LastSetupTime")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strLastSetupTime, c);
		}
		else if(!strcmp(lpszId, "WiFi_SET")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bWifiSet);
		}
		else if(!strcmp(lpszId, "AP_SSID")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strApSsid, c);
		}
		else if(!strcmp(lpszId, "WiFi_SSID")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strWiFiSsid[0], c);
		}
		else if(!strcmp(lpszId, "WiFi_PW")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strWiFiPassword[0], c);
		}
		else if(!strcmp(lpszId, "TEL_NO")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strTelNumber[0], c);
		}
//
		else if(!strncmp(lpszId, "WiFi_SSID_USER", 14)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[14]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO){
				c = pEleItem->Attribute( "value" );
				if(c) strcpy(spConfig->strWiFiSsid[user_no-1], c);
			}
		}
		else if(!strncmp(lpszId, "WiFi_PW_USER", 12)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[12]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO){
				c = pEleItem->Attribute( "value" );
				if(c) strcpy(spConfig->strWiFiPassword[user_no-1], c);
			}
		}
		else if(!strncmp(lpszId, "TEL_NO_USER", 11)){
			char strNo[16] = {0,};
			int user_no = 0;
			
			strcpy(strNo, &lpszId[11]);
			
			user_no = atoi(strNo);
			if(user_no > 0 && user_no <= DEF_MAX_TETHERING_INFO){
				c = pEleItem->Attribute( "value" );
				if(c) strcpy(spConfig->strTelNumber[user_no-1], c);
			}
		}
//
		else if(!strcmp(lpszId, "SeriarNo")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strSeriarNo, c);
		}
		else if(!strcmp(lpszId, "Application_IP")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strApplication_IP, c);
		}
		else if(!strcmp(lpszId, "Application_PORT")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iApplication_PORT);
		}
		else if(!strcmp(lpszId, "DebugServer_PORT")){ //Hidden
			pEleItem->QueryIntAttribute( "value", &spConfig->iDebugServer_PORT);
		}
		else if(!strcmp(lpszId, "CloudServerName")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strCloudServerName, c);
		}
		else if(!strcmp(lpszId, "CloudServerApiUrl")){
			c = pEleItem->Attribute( "value" );
			if(c) strcpy(spConfig->strCloudServerApiUrl, c);
		}
		else if(!strcmp(lpszId, "CloudServerPort")){
			pEleItem->QueryIntAttribute( "value", &spConfig->iCloudServerPort);
		}
		else if(!strcmp(lpszId, "CloudServerSSL")){
			pEleItem->QueryBoolAttribute( "value", &spConfig->bCloudServerSSL);
		}
		pEleItem = pEleItem->NextSiblingElement();
	}

	xmlDoc.Clear();
	return TRUE;

}

BOOL CConfigText::CfgDefaultSet(LPST_CFG_DAVIEW pCfg)
{
	strcpy(pCfg->strName, DA_MODEL_NAME);
	sprintf(pCfg->strFWVersion,"%s [%s %s]", DA_FIRMWARE_VERSION, __DATE__, __TIME__);

	strcpy(pCfg->strDriverCode, "");
	
	//pCfg->iGsensorSensi = 5;
	pCfg->iGsensorSensi = DEF_DEFAULT_GSENSOR_SENS;     //
	pCfg->iGmt = 9;
	pCfg->iAudioRecEnable = 1;
	pCfg->iSpeakerVol = 4;  	// 0.0.20 5 => 3 => 4(1.0.0)
#if !DEF_VIDEO_QUALITY_ONLY		
	pCfg->iFrontResolution = CFG_RESOLUTION_720P;
	pCfg->iRearResolution = CFG_RESOLUTION_720P;
#endif	
	pCfg->iVideoQuality = DEF_DEFAULT_VIDEOQUALITY; 	// LOW, MIDDLE, HIGH 
	pCfg->iEventMode = DEF_DEFAULT_EVENT_SPACE_MODE;		// A, B, C
	pCfg->iRecordingMode = CFG_REC_MODE_NORMAL;
	pCfg->bAutoPmEnable = 0;
	pCfg->iAutoPmEnterTime = 10;
	pCfg->iPmImpactSensi = 10;
	pCfg->bPmMotionEnable = 1;
	pCfg->iPmMotionSensi = 5;	// 1 ~ 6
	pCfg->iEventMotionSensi = 3;
	pCfg->iCarBatVoltage = 0;	//0: 12V, 1: 24V
	pCfg->iCarBatVoltageCalib = 0;
	pCfg->bCarBatSafeEnable = 0;
	pCfg->iCarBatSafeVoltage = 118;
	pCfg->bTempSafeEnable = 0;
	pCfg->iTempSafeValue = 90;
	pCfg->iTempStableValue = 60;
	pCfg->bSecurityLEDEnable = 1;
	pCfg->iSecurityLEDPeriod = 500;
	pCfg->bSecurityLEDMDWarning = 1;
	pCfg->iSecurityLEDOperating = 2;
	pCfg->iLcdOffTime = 30;
	pCfg->iVideoOut = 0;	//TV, LCD
	pCfg->bFactoryReset = 0;
#if BUILD_MODEL == BUILD_MODEL_VRHD_SECURITY_REAR_1CH
	pCfg->iPulseReset = 2; //manual mode
	pCfg->dPulseParam = 197.8;	// 637rpm to 1400rpm, 1400 / 637 = 2.1978???? 9??????  9 * 2.1978 * 10 = 197.8
#else
	pCfg->iPulseReset = 1;
	pCfg->dPulseParam = 80.0;		// 8 ???
#endif

	pCfg->bBrake = 0;
	pCfg->bInput1 = 0;
	pCfg->bInput2 = 0;
	pCfg->iBlink = 2;
	pCfg->iEngine_cylinders = 4;
	pCfg->iSpeed_limit_kmh = 20; // km/h
	pCfg->bOsdSpeed = 0;
	pCfg->bOsdRpm = 0;

	pCfg->G_Sensi_B = 1;
	pCfg->G_Sensi_C= 1;
 #if defined(DEF_SAFE_DRIVING_MONITORING) && defined(DEF_OSAKAGAS_DATALOG)
 #ifdef DEF_SAFE_DRIVING_MONITORING_ONOFF
  	pCfg->bsafemonitoring = 0;			//ON/OFF test_241127
#endif
	#if 0	// 1 : default_241126
  		pCfg->iSuddenAccelerationSensi = 12;
		pCfg->iSuddenDeaccelerationSensi = 12;
		pCfg->iRapidRotationSensi = 60;		//26=>60
	#else
		pCfg->iSuddenAccelerationSensi = 300;
		pCfg->iSuddenDeaccelerationSensi = 300;
		pCfg->iRapidRotationSensi = 3600;		
	#endif
 #else
	pCfg->iSuddenAccelerationSensi = 5;
	pCfg->iSuddenDeaccelerationSensi = 5;
	pCfg->iRapidRotationSensi = 5;
#endif
	#if 0	// 1 : default_241126
		pCfg->Overspeed.nSpeed = 60;	//Km, General overspeed determination speed 
		pCfg->Overspeed.nTime = 30;		//Sec,
		pCfg->Overspeed.nAlarm = 600;	//Sec,
		pCfg->Fastspeed.nSpeed = 90;	//Km, Fast overspeed determination speed
		pCfg->Fastspeed.nTime = 30;		
		pCfg->Fastspeed.nAlarm = 600;
		pCfg->Highspeed.nSpeed = 80;	// Km, High-speed determination speed
		pCfg->Highspeed.nTime = 120;
	#else
		pCfg->Overspeed.nSpeed = 280;	//Km, General overspeed determination speed 
		pCfg->Overspeed.nTime = 30;		//Sec,
		pCfg->Overspeed.nAlarm = 600;	//Sec,
		pCfg->Fastspeed.nSpeed = 300;	//Km, Fast overspeed determination speed
		pCfg->Fastspeed.nTime = 30;		
		pCfg->Fastspeed.nAlarm = 600;
		pCfg->Highspeed.nSpeed = 80;	// Km, High-speed determination speed
		pCfg->Highspeed.nTime = 120;
	#endif
	
	pCfg->bDebugMode = 0;
	pCfg->bTestMode = 0;

	strcpy(pCfg->strLastSetupTime, make_time_string(time(0)).c_str());
	pCfg->bWifiSet = 0;
	strcpy(pCfg->strApSsid, DEF_DEFAULT_SSID_STRING);
	memset((void *)pCfg->strWiFiSsid, 0, sizeof(pCfg->strWiFiSsid));
	memset((void *)pCfg->strWiFiPassword, 0, sizeof(pCfg->strWiFiPassword));
	memset((void *)pCfg->strTelNumber, 0, sizeof(pCfg->strTelNumber));
	strcpy(pCfg->strSeriarNo, "0");

	strcpy(pCfg->strApplication_IP, "");
	pCfg->iApplication_PORT = 0;
	pCfg->iDebugServer_PORT = 0;


	strcpy(pCfg->strCloudServerName, "");
	strcpy(pCfg->strCloudServerApiUrl, "");
	pCfg->iCloudServerPort = 0;
	pCfg->bCloudServerSSL = 0;
	return TRUE;
}

/*
PulseReset=??뫹 ???e?
Brake=??????
Input1=?????e?1
Input2=?????e?2
EventCapacity=?????????		//0%(off), 10%,	20%
*/

BOOL CConfigText::MakeDoctype(const char * cfg_file_name)
{
	const char* doctype =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<setup>"
		"<item value=\"\" id=\"Name\"/>"
		"<item value=\"\" id=\"FWVersion\"/>"
		"<item value=\"0\" id=\"ProductSerialNo\"/>"
		"<item value=\"\" id=\"DriverCode\"/>"		/* 20221019 added */
		"<item value=\"5\" id=\"GSensorSensi\"/>"  	/* DEF_DEFAULT_GSENSOR_SENS */
		"<item value=\"1\" id=\"G_Sensi_B\"/>"
		"<item value=\"1\" id=\"G_Sensi_C\"/>"
		"<item value=\"2020-01-01 09:00:00\" id=\"LastSetupTime\"/>"
		"<item value=\"9\" id=\"GMT\"/>"
		"<item value=\"TRUE\" id=\"AudioRecEnable\"/>"
		"<item value=\"3\" id=\"SpeakerVol\"/>"
#if !DEF_VIDEO_QUALITY_ONLY			
		"<item value=\"720p\" id=\"FrontResolution\"/>"
		"<item value=\"720p\" id=\"RearResolution\"/>"
		"<!-- 720p, 1080p -->"
#endif		
		"<item value=\"LOW\" id=\"VideoQuality\"/>"
		"<!-- LOW, MIDDLE, HIGH -->"
		"<item value=\"C\" id=\"EventMode\"/>"
		"<!-- A, B, C -->"
		"<item value=\"0\" id=\"RecordingMode\"/>"
		"<!-- 0:Normal, 1:Event, 2:Time-lapse -->"
		"<item value=\"FALSE\" id=\"AutoPmEnable\"/>"
		"<item value=\"10\" id=\"AutoPmEnterTime\"/>"
		"<item value=\"1\" id=\"PmImpactSensi\"/>"
		"<item value=\"TRUE\" id=\"PmMotionEnable\"/>"
		"<item value=\"3\" id=\"PmMotionSensi\"/>"
		"<item value=\"3\" id=\"EventMotionSensi\"/>"
		"<item value=\"12V\" id=\"CarBatVoltage\"/>"
		"<!-- 12V, 24V -->"
		"<item value=\"+0\" id=\"CarBatVoltCalib\"/>"
		"<item value=\"FALSE\" id=\"CarBatSafeEnable\"/>"
		"<item value=\"118\" id=\"CarBatSafeVoltage\"/>"
		"<item value=\"FALSE\" id=\"TempSafeEnable\"/>"
		"<item value=\"90\" id=\"TempSafeValue\"/>"
		"<item value=\"60\" id=\"TempStableValue\"/>"
		"<item value=\"TRUE\" id=\"SecurityLEDEnable\"/>"
		"<item value=\"500\" id=\"SecurityLEDPeriod\"/>"
		"<item value=\"TRUE\" id=\"SecurityLEDMDWarning\"/>"
		"<item value=\"2\" id=\"SecurityLEDOperating\"/>"
		"<item value=\"30\" id=\"LcdOffTime\"/>"
		"<item value=\"TV\" id=\"VideoOut\"/>"
		"<!-- TV, LCD -->"
		"<item value=\"FALSE\" id=\"FactoryReset\"/>"
		"<item value=\"FALSE\" id=\"PulseReset\"/>"
		"<item value=\"FALSE\" id=\"BRAKE\" />"
		"<item value=\"FALSE\" id=\"INPUT1\" />"
		"<item value=\"FALSE\" id=\"INPUT2\"/>"
		"<item value=\"2\" id=\"BLINK\"/>"
		"<item value=\"4\" id=\"EngineCylinders\"/>"
		"<item value=\"20\" id=\"SpeedLimitValue\"/>"
		"<item value=\"FALSE\" id=\"OSDSpeed\"/>"
		"<item value=\"FALSE\" id=\"OSDRpm\"/>"
		"<!-- LOG, SAFETY -->"
#ifdef DEF_SAFE_DRIVING_MONITORING_ONOFF
		"<item value=\"1\" id=\"SafeMonitoring\"/>" //test_241127
#endif
		"<item value=\"5\" id=\"SuddenAccelerationSensi\"/>"
		"<item value=\"5\" id=\"SuddenDeaccelerationSensi\"/>"
		"<item value=\"5\" id=\"RapidRotationSensi\"/>"
		"<item value=\"60\" id=\"OverspeedSpeed\"/>"
		"<item value=\"30\" id=\"OverspeedTime\"/>"
		"<item value=\"600\" id=\"OverspeedAlarm\"/>"
		"<item value=\"90\" id=\"FastspeedSpeed\"/>"
		"<item value=\"30\" id=\"FastspeedTime\"/>"
		"<item value=\"600\" id=\"FastspeedAlarm\"/>"
		"<item value=\"80\" id=\"HighspeedSpeed\"/>"
		"<item value=\"120\" id=\"HighspeedTime\"/>"
		"<!-- Internal Use -->"
		"<item value=\"80.0\" id=\"PulseParam\"/>"
		"<item value=\"FALSE\" id=\"WiFi_SET\"/>"
		"<item value=\"driverecorder_sn\" id=\"AP_SSID\"/>"
#if 0		
		"<item value=\"\" id=\"WiFi_SSID\"/>"
		"<item value=\"\" id=\"WiFi_PW\"/>"
#endif		
		"<item value=\"\" id=\"WiFi_SSID_USER1\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER1\"/>"
		"<item value=\"\" id=\"TEL_NO_USER1\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER2\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER2\"/>"
		"<item value=\"\" id=\"TEL_NO_USER2\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER3\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER3\"/>"
		"<item value=\"\" id=\"TEL_NO_USER3\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER4\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER4\"/>"
		"<item value=\"\" id=\"TEL_NO_USER4\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER5\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER5\"/>"
		"<item value=\"\" id=\"TEL_NO_USER5\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER6\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER6\"/>"
		"<item value=\"\" id=\"TEL_NO_USER6\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER7\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER7\"/>"
		"<item value=\"\" id=\"TEL_NO_USER7\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER8\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER8\"/>"
		"<item value=\"\" id=\"TEL_NO_USER8\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER9\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER9\"/>"
		"<item value=\"\" id=\"TEL_NO_USER9\"/>"
		"<item value=\"\" id=\"WiFi_SSID_USER10\"/>"
		"<item value=\"\" id=\"WiFi_PW_USER10\"/>"
		"<item value=\"\" id=\"TEL_NO_USER10\"/>"
		"<item value=\"0\" id=\"SeriarNo\"/>"
		"<!-- SeriarNo 0,1... ( 0 is AP Mode) -->"
		"<item value=\"\" id=\"Application_IP\"/>"
		"<item value=\"0\" id=\"Application_PORT\"/>"
		"</setup>";

		TiXmlDocument doc;
#if 1		
//8K fix size use		
		FILE *fp = fopen(cfg_file_name, "w");
		char temp[8 * 1024] = { 0, };
		fwrite((void *)temp, 1, sizeof(temp), fp);
		fseek( fp, 0, SEEK_SET );

		doc.Parse( doctype );
		doc.SaveFile(fp);
		
		fclose(fp);
#else		
		doc.Parse( doctype );
		doc.SaveFile( cfg_file_name );
#endif
		doc.Clear();
		return TRUE;
}

BOOL CConfigText::CfgDefaultSet(const char * cfg_file_name)
{
		ST_CFG_DAVIEW Cfg;
		CfgDefaultSet(&Cfg);
		return Save(cfg_file_name, &Cfg);
}


BOOL CConfigText::CfgPrintfToFile(FILE* fp, const char *fmt, ... )
{
	BOOL bRet = FALSE;
   	va_list argP;
   	char	string[255];
   
   	va_start(argP, fmt);
   	vsprintf(string, fmt, argP);
   
	if(fp)
	{
		fprintf( fp, string);
		bRet = TRUE;
	}
	
	va_end(argP);

	return bRet;
}
