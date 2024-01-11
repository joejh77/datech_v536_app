// daSystemSetup.cpp: implementation of the CDaSystemSetup class.
//
//////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include "datools.h"
#include "gsensor.h"
#include "evdev_input.h"
#include "mixwav.h"
#include "wavplay.h"
#include "system_log.h"
#include "led_process.h"
#include "pai_r_data.h"
#if ENABLE_DATA_LOG
#include "data_log.h"
#endif
#include "daSystemSetup.h"

#define ZONE_DSS_FUNC	1
#define ZONE_DSS_ERROR 1


const char * sd_setup_file = DA_SETUP_FILE_PATH_SD;
const char * backup_setup_file = DA_SETUP_FILE_PATH_BACKUP;

bool CDaSystemSetup::cfg_get(ST_CFG_DAVIEW *p_cfg, ST_CFG_DAVIEW *p_back_cfg)
{
	bool result = true;
	ST_CFG_DAVIEW	back_cfg;
	ST_CFG_DAVIEW	sd_cfg;
	
	memset((void *)&back_cfg, 0, sizeof(ST_CFG_DAVIEW));
	memset((void *)&sd_cfg, 0, sizeof(ST_CFG_DAVIEW));
	CConfigText::CfgDefaultSet(&back_cfg);
	CConfigText::CfgDefaultSet(&sd_cfg);

	//setup.XML file check
	const char * xmlcheck_exe = "/datech/app/da-start";
	if ( access(xmlcheck_exe, R_OK ) == 0) {                             // 파일이 있음 
		system(xmlcheck_exe);
	}

	if(!CConfigText::Load(backup_setup_file, &back_cfg)){
		CConfigText::CfgDefaultSet(backup_setup_file);
	}

	
	if(!CConfigText::Load(sd_setup_file, &sd_cfg)){
		//sd card에 setup.XML이 없으면 backup된 xml 파일을 저장한다
		sd_cfg = back_cfg;
		result = false;
	}
	else {
		if(sd_cfg.bWifiSet){
			sd_cfg.bWifiSet = 0;
			cfg_set(&sd_cfg);
			dprintf(ZONE_DSS_FUNC, "%s() : Update Wi-Fi settings internal values.\r\n", __func__);
		}
		else {
			dprintf(ZONE_DSS_FUNC, "%s() : WiFi setting use internal values.\r\n", __func__);
			strcpy(sd_cfg.strApSsid, back_cfg.strApSsid);
			memcpy((void *)sd_cfg.strWiFiSsid, (void *)back_cfg.strWiFiSsid, sizeof(back_cfg.strWiFiSsid));
			memcpy((void *)sd_cfg.strWiFiPassword, (void *)back_cfg.strWiFiPassword, sizeof(back_cfg.strWiFiPassword));
			memcpy((void *)sd_cfg.strTelNumber, (void *)back_cfg.strTelNumber, sizeof(sd_cfg.strTelNumber));
			strcpy(sd_cfg.strSeriarNo, back_cfg.strSeriarNo);
		}
	}

	*p_cfg = sd_cfg;

	if(p_back_cfg)
		*p_back_cfg = back_cfg;
	
	return result;
}

bool CDaSystemSetup::cfg_set(ST_CFG_DAVIEW *p_cfg)
{	
	bool result = false;

	sprintf(p_cfg->strFWVersion, "%s [%s %s]", DA_FIRMWARE_VERSION, __DATE__, __TIME__);
	result = CConfigText::Save(sd_setup_file, p_cfg);
	result = CConfigText::Save(backup_setup_file, p_cfg);

	return result;
}

CDaSystemSetup::CDaSystemSetup(void)
{
	is_config_init = false;
}

CDaSystemSetup::~CDaSystemSetup(void)
{
}


bool CDaSystemSetup::Init(void)
{
	const char * time_file = DA_TIME_FILE_NAME;
	int retry = 0;
	bool ret = false;
	bool sd_error = false;
	bool sd_setup_update = false;
	char szCmd[128];
	
	ST_CFG_DAVIEW	back_cfg;

	if ( access(time_file, R_OK ) == 0) {                             // 파일이 있음 
		ST_CFG_TIME cfg_time;
		if(CConfigText::Load(time_file, &cfg_time)){
			//if(cfg_time.nTimeSet)
			{
				SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE ,RTC_SRC_SETUP_SD, "Before");
				
				sprintf(szCmd, "date -s \"%04d-%02d-%02d %02d:%02d:%02d\"", cfg_time.nYear, cfg_time.nMonth, cfg_time.nDate, cfg_time.nHour, cfg_time.nMinute, cfg_time.nSec);
				system(szCmd);

				//cfg_time.nTimeSet = 0;
				//CConfigText::Save(&cfg_time);

				sprintf(szCmd, "rm %s", time_file);
				system(szCmd);
				system("hwclock -w");
				SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE ,RTC_SRC_SETUP_SD, "After");
			}
		}
	}

	if(this->cfg_get(&this->cfg, &back_cfg) == false)
		sd_setup_update =true;
	
	if(this->cfg.bFactoryReset){
		CConfigText::CfgDefaultSet(&this->cfg);
		sprintf(szCmd, "rm %s", sd_setup_file);
		system(szCmd);
		sd_setup_update = true;
	}

	if(strcasecmp(this->cfg.strFWVersion, DA_MODEL_NAME) != 0){
		sprintf(this->cfg.strFWVersion,"%s [%s %s]", DA_FIRMWARE_VERSION, __DATE__, __TIME__);
	}

	if(memcmp((void *)&back_cfg, (void *)&this->cfg, sizeof(ST_CFG_DAVIEW)) != 0)
		CConfigText::Save(backup_setup_file, &this->cfg);
	
	if(this->cfg.iEventMode > CFG_EVENT_MODE_MAX)
		this->cfg.iEventMode = CFG_EVENT_MODE_MAX;


	dprintf(ZONE_DSS_FUNC, "FWVersion : %s \r\n", this->cfg.strFWVersion);
	dprintf(ZONE_DSS_FUNC, "FactoryReset : %d \r\n", this->cfg.bFactoryReset);
	dprintf(ZONE_DSS_FUNC, "PulseReset : %d \r\n", this->cfg.iPulseReset);
	dprintf(ZONE_DSS_FUNC, "Gmt : %d \r\n", this->cfg.iGmt);
	dprintf(ZONE_DSS_FUNC, "GsensorSensi : %d \r\n", this->cfg.iGsensorSensi);
#ifdef DEF_SAFE_DRIVING_MONITORING	
	dprintf(ZONE_DSS_FUNC, "G_Sensi_B : %d \r\n", this->cfg.G_Sensi_B);
	dprintf(ZONE_DSS_FUNC, "G_Sensi_C : %d \r\n", this->cfg.G_Sensi_C);
#endif
	dprintf(ZONE_DSS_FUNC, "AudioRecEnable : %d \r\n", this->cfg.iAudioRecEnable);
	dprintf(ZONE_DSS_FUNC, "SpeakerVol : %d \r\n", this->cfg.iSpeakerVol);
	
#if !DEF_VIDEO_QUALITY_ONLY	
	if(this->cfg.iFrontResolution >= CFG_RESOLUTION_END)
		this->cfg.iFrontResolution = CFG_RESOLUTION_720P;
	
	dprintf(ZONE_DSS_FUNC, "FrontResolution : %d \r\n", this->cfg.iFrontResolution);
	dprintf(ZONE_DSS_FUNC, "RearResolution : %d \r\n", this->cfg.iRearResolution);
#endif
	dprintf(ZONE_DSS_FUNC, "VideoQuality : %d \r\n", this->cfg.iVideoQuality);
	dprintf(ZONE_DSS_FUNC, "EventMode : %d \r\n", this->cfg.iEventMode);
	dprintf(ZONE_DSS_FUNC, "RecordingMode : %d \r\n", this->cfg.iRecordingMode);
	dprintf(ZONE_DSS_FUNC, "CarBatVoltage : %d \r\n", this->cfg.iCarBatVoltage);
	dprintf(ZONE_DSS_FUNC, "CarBatSafeEnable : %d \r\n", this->cfg.bCarBatSafeEnable);
	dprintf(ZONE_DSS_FUNC, "CarBatSafeVoltage : %d \r\n", this->cfg.iCarBatSafeVoltage);
	dprintf(ZONE_DSS_FUNC, "BREAK : %d \r\n", this->cfg.bBrake);
	dprintf(ZONE_DSS_FUNC, "INPUT1 : %d \r\n", this->cfg.bInput1);
	dprintf(ZONE_DSS_FUNC, "INPUT2 : %d \r\n", this->cfg.bInput2);
	dprintf(ZONE_DSS_FUNC, "BLINK : %d \r\n", this->cfg.iBlink);
	dprintf(ZONE_DSS_FUNC, "EngineCylinders : %d \r\n", this->cfg.iEngine_cylinders);
	dprintf(ZONE_DSS_FUNC, "SpeedLimitValue : %d \r\n", this->cfg.iSpeed_limit_kmh);
	dprintf(ZONE_DSS_FUNC, "OSDSpeed : %d \r\n", this->cfg.bOsdSpeed);
	dprintf(ZONE_DSS_FUNC, "OSDRpm : %d \r\n", this->cfg.bOsdRpm);

#ifdef DEF_SAFE_DRIVING_MONITORING
	dprintf(ZONE_DSS_FUNC, "SuddenAccelerationSensi : %d \r\n", this->cfg.iSuddenAccelerationSensi);
	dprintf(ZONE_DSS_FUNC, "SuddenDeccelerationSensi : %d \r\n", this->cfg.iSuddenDeaccelerationSensi);
	dprintf(ZONE_DSS_FUNC, "RapidRotationSensi : %d \r\n", this->cfg.iRapidRotationSensi);

	dprintf(ZONE_DSS_FUNC, "OverspeedSpeed : %d \r\n", this->cfg.Overspeed.nSpeed);
	dprintf(ZONE_DSS_FUNC, "OverspeedTime : %d \r\n", this->cfg.Overspeed.nTime);
	dprintf(ZONE_DSS_FUNC, "OverspeedAlarm : %d \r\n", this->cfg.Overspeed.nAlarm);
	dprintf(ZONE_DSS_FUNC, "FastspeedSpeed : %d \r\n", this->cfg.Fastspeed.nSpeed);
	dprintf(ZONE_DSS_FUNC, "FastspeedTime : %d \r\n", this->cfg.Fastspeed.nTime);
	dprintf(ZONE_DSS_FUNC, "FastspeedAlarm : %d \r\n", this->cfg.Fastspeed.nAlarm);
	dprintf(ZONE_DSS_FUNC, "HighspeedSpeed : %d \r\n", this->cfg.Highspeed.nSpeed);
	dprintf(ZONE_DSS_FUNC, "HighspeedTime : %d \r\n", this->cfg.Highspeed.nTime);
#endif
	dprintf(ZONE_DSS_FUNC, "DebugMode : %d \r\n", this->cfg.bDebugMode);
	dprintf(ZONE_DSS_FUNC, "TestMode : %d \r\n", this->cfg.bTestMode);

	dprintf(ZONE_DSS_FUNC, "PulseParam : %0.2f \r\n", this->cfg.dPulseParam);

	dprintf(ZONE_DSS_FUNC, "LastSetupTime : %s \r\n", this->cfg.strLastSetupTime);
	dprintf(ZONE_DSS_FUNC, "AP_SSID : %s \r\n", this->cfg.strApSsid);
	dprintf(ZONE_DSS_FUNC, "WiFi_SSID : %s \r\n", this->cfg.strWiFiSsid[0]);
	dprintf(ZONE_DSS_FUNC, "WiFi_PW : %s \r\n", this->cfg.strWiFiPassword[0]);
	dprintf(ZONE_DSS_FUNC, "TEL_NO : %s \r\n", this->cfg.strTelNumber[0]);
	dprintf(ZONE_DSS_FUNC, "SeriarNo : %s \r\n", this->cfg.strSeriarNo);
	
#if 0
	if(this->cfg.iSpeakerVol * 6 > 25)
		mixwav_set_volume(this->cfg.iSpeakerVol * 6);
	else
		mixwav_set_volume(25);
#else
	mixwav_set_volume(this->cfg.iSpeakerVol * 6);
#endif

	if(1) { 
		struct tm tmInit;
		time_t t_init;
		time_t     t_rtc;

		tmInit.tm_year = 100+((2023)%100);
		tmInit.tm_mon = 1-1;		// Month, 0 - jan
		tmInit.tm_mday = 1;				// Day of the month
		tmInit.tm_hour = 0;
		tmInit.tm_min = 0;
		tmInit.tm_sec = 0;

		tmInit.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

		t_init = mktime(&tmInit) + (this->cfg.iGmt * 60);
		//t_init = mktime(&tmInit);
		printf("INIT Time : %ld\n", (long) t_init);
		// Get current time
	    time(&t_rtc);
		printf("RTC Time : %ld\n", (long) t_rtc);
		// Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
	    //

		if(t_rtc < t_init){
			char szCmd[128];
#if 0		
			tmInit = *localtime(&t_init);
#else
			time_t     t_init_utc = mktime(&tmInit) + (this->cfg.iGmt * 60 * 60);

			localtime_r(&t_init_utc, &tmInit);
#endif

			sprintf(szCmd, "date -s \"%04d-%02d-%02d %02d:%02d:%02d\"", \
				tmInit.tm_year + 1900, \
				tmInit.tm_mon + 1, \
				tmInit.tm_mday, \
				tmInit.tm_hour, \
				tmInit.tm_min, \
				tmInit.tm_sec);

			printf("RTC Time Set : %s\n", szCmd);
			
			system(szCmd);

			system("hwclock -w"); 
		} 
	}
	
SD_FORMAT:
	//SD CARD Format check
	if ( sd_error || access(DA_FORMAT_FILE_NAME, R_OK ) == 0 || access(DA_FORMAT_FILE_NAME2, R_OK ) == 0 || access(DA_FACTORY_DEFAULT_FILE_NAME, R_OK ) == 0) {                             // 파일이 있음 
//		int umount_check = 30;

		if(access(DA_FACTORY_DEFAULT_FILE_NAME, R_OK ) == 0) {		
			system("rm " DEF_SYSTEM_LOG_PATH);
			system("rm " DEF_SYSTEM_LOG_PATH_SD);

			system("rm " DA_SETUP_FILE_PATH_SD);
			system("rm " DA_SETUP_FILE_PATH_BACKUP);

			system("rm " DA_FACTORY_DEFAULT_FILE_NAME);
			
			CConfigText::CfgDefaultSet(&this->cfg);
			
			CConfigText::Save(DA_SETUP_FILE_PATH_BACKUP, &this->cfg);
			dprintf(ZONE_DSS_FUNC, "FACTORY DEFAULT SET ... \r\n");
		}
			
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_MEMORY_FORMAT , 0, "SD");
		led_process_postCommand(LED_WORK_MODE, LED_WORK_FORMAT, LED_TIME_INFINITY);
		
		mixwav_system_aplay(kMixwaveSDInit);

		sprintf(szCmd, "rm -rf %s", DEF_SYSTEM_DIR);
		system(szCmd);

#if ENABLE_DATA_LOG
		sprintf(szCmd, "rm -rf %s", DEF_DATA_LOG_DIR);
		system(szCmd);
#endif

		msleep(3000);
#if 0
		do{
			system("umount /mnt/extsd");
			msleep(1000);
		}while(access("/mnt/extsd/NORMAL", R_OK ) == 0 && umount_check--);

		if(umount_check == 0){
			sd_error = true;
		}
		else 
#endif			
		{
			const char * offs_file_name = OFFS_CFG_FILE_NAME;
#if DEF_VIDEO_QUALITY_ONLY
			int resolution = CFG_RESOLUTION_720P;
 #ifdef DEF_REAR_CAMERA_ONLY
			offs_file_name = OFFS_REAR_CAM_ONLY_FILE_NAME;
 #else
			if(this->cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH)
				resolution = CFG_RESOLUTION_1080P;
 #endif
#else
			int resolution = this->cfg.iFrontResolution;
#endif
			if ( access("/dev/mmcblk0p1", R_OK ) == 0 && access("/dev/mmcblk0p2", R_OK ) != 0){			
				sprintf(szCmd, "mkfs.offs /dev/mmcblk0p1 /datech/app/%s driving%d_%d_%d", offs_file_name, this->cfg.iEventMode, this->cfg.iVideoQuality, resolution);
			}
			else { // raw format
				//offs-3.11.22 raw format의 경우 파티션 블록(mmcblk0p1)이 아닌 mmc 블록 전체(mmcblk)를 인자로 주어야 합니다
				sprintf(szCmd, "mkfs.offs -f /dev/mmcblk /datech/app/%s driving%d_%d_%d", offs_file_name, this->cfg.iEventMode, this->cfg.iVideoQuality, resolution);
			}
			
			system(szCmd);
#ifdef TARGET_CPU_V536
			system("offs.fuse -d /dev/mmcblk0p1 /mnt/extsd");
#else
			system("offs.fuse /mnt/extsd");
#endif
			sd_error = false;
		}
		sd_setup_update = true;
	}

	//Folder check
	if ( access("/mnt/extsd/NORMAL", R_OK ) != 0) {
		sd_error = true;
	}

	if(this->cfg.iEventMode){
		if ( access("/mnt/extsd/EVENT", R_OK ) != 0) {
			sd_error = true;
		}
	}

	if(sd_error){
		if(retry != 0) {//처음에는 무음으로
			mixwav_system_aplay(kMixwaveSDError);
		}

		if(retry++ < 1)
			goto SD_FORMAT;
		else
			return 0;
	}
	else
		ret = true;

	if(access(DA_FORMAT_FILE_NAME, R_OK ) == 0){
		sprintf(szCmd, "rm %s", DA_FORMAT_FILE_NAME);
		system(szCmd);
	}

	if(access(DA_FORMAT_FILE_NAME2, R_OK ) == 0){
		sprintf(szCmd, "rm %s", DA_FORMAT_FILE_NAME2);
		system(szCmd);
	}

	if(sd_setup_update){
		CConfigText::Save(sd_setup_file, &this->cfg);
	}

	if(memcmp((void *)&back_cfg, (void *)&this->cfg, sizeof(ST_CFG_DAVIEW)) != 0)
		CConfigText::Save(backup_setup_file, &this->cfg);
	
	this->is_config_init = true;
	
	mixwav_set_volume(this->cfg.iSpeakerVol * 6);
	
	evdev_input_set_pulse_parameter(this->cfg.bBrake, this->cfg.bInput1, this->cfg.bInput2, this->cfg.iBlink, (this->cfg.iPulseReset == 1 ? false : true) | this->cfg.bTestMode, this->cfg.dPulseParam, this->cfg.iGmt, (double)this->cfg.iEngine_cylinders, this->cfg.iSpeed_limit_kmh);

	evdev_input_start(this->cfg.bDebugMode);
	
	return ret;
}

bool CDaSystemSetup::Deinit(void)
{
	bool ret = false;
	
	evdev_input_stop();

	this->cfg_backup();

	this->is_config_init = false;
	
	return ret;
}

bool CDaSystemSetup::cfg_backup(void)
{
	if(is_config_init == false)
		return false;

	if(this->cfg.bTestMode == false && ((this->cfg.iPulseReset == 1 && evdev_input_get_pulse_parameter()->pulse_init)  ||  evdev_input_get_pulse_parameter()->pulse_init > 1)){
		this->cfg.iPulseReset = false;
		this->cfg.dPulseParam = evdev_input_get_pulse_parameter()->pulse_param;
		CConfigText::Save(DA_SETUP_FILE_PATH_SD, &this->cfg);
		CConfigText::Save(DA_SETUP_FILE_PATH_BACKUP, &this->cfg);

		dprintf(ZONE_DSS_FUNC, "%s() : saved pulse parameter value %0.2f \r\n", __func__, this->cfg.dPulseParam);
	}

	return true;
}
