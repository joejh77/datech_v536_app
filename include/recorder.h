#ifndef _recorder_h
#define _recorder_h


#include "daappconfigs.h"
#include "daversion.h"
#include "datypes.h"
#include "pai_r_data.h"
#include "pai_r_datasaver.h"
#include "data_log.h"

#include "SB_System.h"
#if ENABLE_NETWORKING
#include "DaUpStreamDelegate.h"
#endif
#if ENABLE_DATA_LOG
#include "data_log.h"
#endif
#include "daSystemSetup.h"
#include "led_process.h"
#include "bb_micom.h"
#include "bb_api.h"
#include "system_log.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	kStartRecording = 0,
	kStopRecording,
	kRestartRecording,
	kStartEventRecording,	//gsensor  "E"
	kStartEventInput1,		// input1 "A"
	kStartEventInput2,		// input2 "B"
	kStartEventNone,
	kStartEventSuddenAcceleration, // 급발진 : 정차 상태에서 급가속
	kStartEventSuddenStart, //급가속
	kStartEventSuddenDeacceleration, // 급감속
	kStartEventRapidLeft,	//급회전 좌회전
	kStartEventRapidRight, //급회전 오른쪽
	kStartEventOverSpeed, // General overspeed
	kStartEventFastSpeed, // Fast overspeed
	kStartEventGSensorLevelA,
	kStartEventGSensorLevelB,
	kStartEventGSensorLevelC,
	kStopEventRecording,
	kStartMotionRecording,
	kStopMotionRecording,
	kExternalPowerFall,
	kExternalPowerGood,
	kRecorderExitNow,
	kSwapSurfaces,
	kChangeRecordingMode,
	kTakeFrontSnapshot,
	kTakeFrontEventSnapshot,
	kTakeRearSnapshot,

	kRecordingCompleted,
	kRecordingCompleted2,

	kRecordingCompletedMotion,
	kRecordingCompletedMotion2,
	
	kRecordingCompletedNormal,
	kRecordingCompletedNormal2,
	
	kSDCardOk,
	kSDCardError,
};

typedef struct {
	int32_t rec_secs;
	int32_t front_resolution; // 0 : 720p 	1 : 1080p	2:480
	int32_t rear_resolution;
	int32_t front_bitrate;
	int32_t rear_bitrate;
	int32_t front_fps;
	int32_t rear_fps;
	bool 	bAudioRecEnable;
	bool		bMotionRecEnable;
	int32_t motionSensi;
	int32_t	RecordingMode;
//#if DEF_VIDEO_FIX_BITRATE
	int32_t	fix_iqp;
	int32_t	fix_pqp;
//#endif
	int32_t	httpdDebugEnable;

	//pic eeprom load data
	u32 		serial_no;
	u32		hw_config;	// bit 0~1: 0 720p, 1 1080P, 2 1920x480(NTSC) 
	
}REC_ENV;

typedef struct tagRecoderParam
{
#if DEF_SUB_MICOM_USE
	CBbMicom *pSubmcu;
#endif
#if ENABLE_DATA_LOG
	CDatalog	*pDatalog;
#endif
	REC_ENV *pRec_env;
	CDaSystemSetup *pSys_setup;
	bool *pSd_exist;
	bool *pNet_exist;

	ST_TEST_RESULT *pTestResult;

} ST_RECORDER_PARAM;

extern void recorder_reboot(void);
extern ST_RECORDER_PARAM * RecorderGetParam(void);
extern u32 RecorderSetSerialNo(u32 serial);

extern void RecorderPostCommand(int32_t cmd);
extern bool RecorderAddStreamData(void *stream_data, int length);

extern bool Recorder_IsRecording(void);
extern bool Recorder_isEventRunning(void);
extern bool Recorder_isMotionRunning(void);
extern bool Recorder_isMotionMode(void);
extern bool Recorder_isNetExist(void);
extern int Recorder_get_HttpdDebugEnable(void);

#ifdef __cplusplus
}
#endif

#endif //_recorder_h