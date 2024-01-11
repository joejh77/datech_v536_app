#ifndef _mixwav_h
#define _mixwav_h

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	kMixwaveRecordStart = 0,	//start.wav
	kMixwaveRecordStop,		//Power_off.wav
	kMixwaveEventRecording,  	//event.wav
	kMixwaveGPS,				//GPS.wav
	kMixwaveNone,				//none.wav
	kMixwaveSDError,			//SD_card.wav
	kMixwaveSDInit,				//SD_init.wav
	kMixwaveUpdate,			//Update.wav
	kMixwaveUpdateEnd,			//Update_end.wav
	kMixwaveDingdong,			//dingdong.wav

/*10*/	kMixwaveWiFi_connection_failed,	//Communication_error.wav
		kMixwaveServer_connection_failed,	//Server_connection_failed.wav
		kMixwavWiFi_setting_mode,			//check_the_network_settings.wav
		kMixwavDriverecorder_registration_failed,	//Driverecorder_registration_failed.wav
		kMixwavUpdate_new_version,				//Update_new_version.wav

	kMixwaveWAVE1,		//WAVE1.wav
	kMixwaveWAVE2,
	kMixwaveWAVE3,
	kMixwaveWAVE4,
	kMixwaveWAVE5,		//WAVE5.wav

	kMixwaveRecord_wa,		//Record_wa.wav
	kMixwave10SecBlocking,
	kMixwaveExitNow,
}eMIXWAVECMD;

int mixwav_play(eMIXWAVECMD cmd);
int mixwav_start(void);
int mixwav_stop(void);

//volume 0 ~ 31
int mixwav_set_volume(int volume);
//
bool mixwav_get_file_path(char * file_path, eMIXWAVECMD cmd);
void mixwav_system_aplay(eMIXWAVECMD cmd);
#ifdef __cplusplus
}
#endif

#endif //_mixwav_h