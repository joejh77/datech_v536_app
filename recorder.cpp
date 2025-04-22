#include "OasisAPI.h"
#include "OasisLog.h"

#include "OasisMedia.h"
#include "OasisUI.h"
#include "OasisFS.h"
#include "OasisUtil.h"
#include "OasisPreview.h"
#include "OasisAdas.h"
#include "OasisMotion.h"
#include "OasisDisplay.h"

#include "datools.h"
#include "sysfs.h"
#include "evdev_input.h"
#include "mixwav.h"

#include "safe_driving_monitoring.h"
#include "gsensor.h"

#include "ttyraw.h"

//rtc
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "recorder.h"


#define DLOG_ADAS	0x00010000
#define DLOG_RECORD	0x00020000
#define DLOG_FRAME	0x00040000
#define DLOG_TRACE	0x00080000

#undef DLOG_FLAGS
//#define  DLOG_FLAGS	(DLOG_FLAGS_DEFAULT|DLOG_ADAS|DLOG_RECORD|DLOG_TRACE/*|DLOG_FRAME*/)
#define  DLOG_FLAGS	(DLOG_FLAGS_DEFAULT|DLOG_ADAS|DLOG_RECORD|DLOG_TRACE|DLOG_FRAME)

#undef DLOG_TAG
#define DLOG_TAG "RECORDER"

#if defined(TARGET_CPU_V5) || defined(TARGET_CPU_V536)
	#define OFFS_QUEUE_SIZE_KBYTES	20480
	//#define OFFS_QUEUE_SIZE_KBYTES	4096
	//#define OFFS_QUEUE_SIZE_KBYTES	2048
	//#define OFFS_QUEUE_SIZE_KBYTES	1024

	#define OFFS_CACHE_SIZE_KBYTES	10240

	#define OFFS_WRITE_ALIGNMENT_BYTES	4096 //ThinkWare 8GB SD
	//#define OFFS_WRITE_ALIGNMENT_BYTES	8192 //ThinkWare 16GB SD
	//#define OFFS_WRITE_ALIGNMENT_BYTES	16384
	//#define OFFS_WRITE_ALIGNMENT_BYTES	65536

	#define MEDIA_CACHE_SIZE_KBYTES 41920
	//#define MEDIA_CACHE_SIZE_KBYTES	4096
	//#define MEDIA_CACHE_SIZE_KBYTES	2048
	//#define MEDIA_CACHE_SIZE_KBYTES	1024
#else

#if TARGET_BOARD_MA
	//MA board
	#define OFFS_QUEUE_SIZE_KBYTES	(40*1024)
	#define OFFS_CACHE_SIZE_KBYTES	(1024)
	#define OFFS_WRITE_ALIGNMENT_BYTES	8192
	#define MEDIA_CACHE_SIZE_KBYTES (40*1024)
#else
	//#define OFFS_QUEUE_SIZE_KBYTES	10240
	#define OFFS_QUEUE_SIZE_KBYTES	8192
	//#define OFFS_QUEUE_SIZE_KBYTES	4096
	//#define OFFS_QUEUE_SIZE_KBYTES	2048
	//#define OFFS_QUEUE_SIZE_KBYTES	1024


	#define OFFS_CACHE_SIZE_KBYTES	512

	//#define OFFS_WRITE_ALIGNMENT_BYTES	4096 //ThinkWare 8GB SD
	#define OFFS_WRITE_ALIGNMENT_BYTES	8192 //ThinkWare 16GB SD
	//#define OFFS_WRITE_ALIGNMENT_BYTES	16384
	//#define OFFS_WRITE_ALIGNMENT_BYTES	65536
	//#define OFFS_WRITE_ALIGNMENT_BYTES	(512*1024)

	//#define MEDIA_CACHE_SIZE_KBYTES 10240
	#define MEDIA_CACHE_SIZE_KBYTES 8192
	//#define MEDIA_CACHE_SIZE_KBYTES	4096
	//#define MEDIA_CACHE_SIZE_KBYTES	2048
	//#define MEDIA_CACHE_SIZE_KBYTES	1024
#endif

#endif


#define OFFS_WRITE_CHUNK_BYTES		(16*8192)	//128K, multiple of write alignment

//#define VGA_HEIGH_RESOLUTION	"480"	//"360"
#define VGA_HEIGH_RESOLUTION	"360"

#define DISPLAY_PIP_CONTROL_USE		0 // ???? Rear ????? ?????? ??? ?????? ??? ??????? ????
#define DISPLAY_PIP_SIZE 	3 // 2 is 1/4 size
#define DISPLAY_PIP_FRONT_CAMERA_ONLY 	60

#define REAR_CAMERA_CHECK_ONFRAME		1

#define FIXED_FONT_USED	1
//#define DEF_FONT_DIR 	"/datech/"
#define DEF_FONT_DIR 	"/data/"

#define DEF_SNAPSHOT_TYPE_MASK		0x80000000
using namespace oasis;

static TTYRaw tty_;

enum {
	RECORD_NormalMode = 0,
	RECORD_EventMode,
	RECORD_TimelapseMode,
	RECORD_Parking_Motion,
	RECORD_Parking_Timelapse
};


#define DEF_REBOOT_FLAG		0x80000000

typedef struct {
	ui::LabelRef rtc;
	ui::LabelRef wifi;
	ui::LabelRef setup;
	ui::LabelRef pulse;
	ui::LabelRef pulse_Rpm;
	ui::LabelRef pulse_Br;
	ui::LabelRef pulse_R;
	ui::LabelRef pulse_L;
	ui::LabelRef pulse_Bg;
	ui::LabelRef pulse_T;
	ui::LabelRef gps;
	ui::LabelRef gsensor;
	ui::LabelRef submcu;
	ui::LabelRef ext_cam;
}TEST_OSD_WIDGETS;

#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
#define FILE_PATH_BUFFER_COUNT	2
typedef struct {
	u32 idx;
	char path[FILE_PATH_BUFFER_COUNT][128];
}ST_FILE_PATH_BUFFER;

static ST_FILE_PATH_BUFFER l_st_recording_file_path = { 0, };
static ST_FILE_PATH_BUFFER l_st_motion_recording_file_path = { 0, };
static char l_normal_recording_file_path[128] = { 0,};
#endif

#define led_set_normal_recording_start()	led_process_postCommand(LED_RED, LED_TIME_INFINITY, 0)
#define led_set_normal_recording_end()		led_process_postCommand(LED_RED, 0, LED_TIME_INFINITY)

#define led_set_event_recording_start()		led_process_postCommand(LED_WORK_MODE, LED_WORK_EVENT, 30000)
#define led_set_event_recording_end()		led_process_postCommand(LED_WORK_MODE, LED_WORK_NORMAL, LED_TIME_INFINITY)

#define led_set_motion_recording_start()	led_process_postCommand(LED_RED, LED_TIME_INFINITY, 0)
#define led_set_motion_recording_wait()		led_process_postCommand(LED_RED, 100, 2900)

static bool timer_running_ = true;
static bool is_avi_format_ = false;
static bool auto_recording_event_ = false;
#if ENABLE_ADAS
static bool adas_enabled_ = false;
#endif
static bool rear_disabled_ = false;
static bool rear_camera_detected = false;
static bool power_disconnected = false;
static bool power_low_voltage = false;

static int32_t l_recorder_camera1_id = 0;
static int32_t l_recorder_camera2_id = 1;
static int32_t l_recorder_camera3_id = -1;
static int32_t l_recorder_camera4_id = -1;
static int32_t l_recorder_camera_counts = 1;
	
static bool auto_stop_and_restart_normal_recording_ = false;

static bool l_is_recording = false;
static bool l_is_parking_mode = false;
static u32 snapshot_id_ = 0;

static REC_ENV l_rec_env = { 0, };
static CDaSystemSetup l_sys_setup;

static RecorderRef l_recorder_ref = nullptr;
static ui::LabelRef l_osd_date_label = nullptr;
static TEST_OSD_WIDGETS l_test_osd_label;
static ST_TEST_RESULT l_test_result = {0,};

static bool l_sd_exist = false;
static bool l_net_exist = false;
#ifdef DEF_PAI_R_DATA_SAVE
static u32 l_local_autoid = 0;
#endif
static char l_normal_recording_type = REC_FILE_TYPE_NORMAL;

static int64_t l_durationUs = 0;
static size_t	l_pre_writerStat_fileLength = 0;
static u32 l_writerStat_fileLength_error_count = 0;

#if DEF_SUB_MICOM_USE
static CBbMicom l_sub_mcu;
#endif
#if ENABLE_DATA_LOG
static CDatalog	l_datalog;
#endif

struct _UserDataHeader_t {
	uint8_t   	Separator;    // ???
	uint8_t   	Header;       // ??I?
	char   		ProductInfo[6]; // VRHD
	char   		VersionInfo[20]; // 00.00.01
	char     	ChannelInfo[4];  // 2
} __attribute__((packed));

static struct _UserDataHeader_t ma_strd_data_header_;

struct RecorderCommand {
	int32_t cmd_;
	RecorderCommand() = default;
	RecorderCommand(int32_t cmd) : cmd_(cmd) {}
	RecorderCommand(const RecorderCommand& src) : cmd_(src.cmd_) {}
	RecorderCommand& operator=(const RecorderCommand& src) {
		if(this != &src) {
			cmd_ = src.cmd_;
		}
		return *this;
	}
};


static std::condition_variable cmdq_cond_;
static std::mutex cmdq_lock_;
static std::list<RecorderCommand> cmdq_;


static void postCommand(int32_t cmd) {
	try {
		std::lock_guard<std::mutex> _l(cmdq_lock_);
		cmdq_.emplace_back(cmd);
		cmdq_cond_.notify_one();
	} catch(...) {

	}
}

void RecorderPostCommand(int32_t cmd)
{
	postCommand(cmd);
}

ST_RECORDER_PARAM * RecorderGetParam(void)
{
	static ST_RECORDER_PARAM rec_param;

#if DEF_SUB_MICOM_USE
	rec_param.pSubmcu = &l_sub_mcu;
#endif
#if ENABLE_DATA_LOG
	rec_param.pDatalog = &l_datalog;
#endif
	rec_param.pRec_env = &l_rec_env;
	rec_param.pSys_setup = &l_sys_setup;
	rec_param.pSd_exist = &l_sd_exist;
	rec_param.pNet_exist = &l_net_exist;

	rec_param.pTestResult = &l_test_result;
	return &rec_param;
}

u32 RecorderSetSerialNo(u32 serial)
{
#if DEF_SUB_MICOM_USE
	l_sub_mcu.SetEepromData(EEPROM_SERIAL_NO, serial);
	
	l_rec_env.serial_no = l_sub_mcu.GetEepromData(EEPROM_SERIAL_NO);
#endif		
	sysfs_printf(DEF_DEFAULT_SSID_TMP_PATH, "%u", l_rec_env.serial_no);
	return l_rec_env.serial_no;
}

bool RecorderAddStreamData(void *stream_data, int length)
{	
	//DLOG(DLOG_RECORD, "%d : %s\r\n", recorderIsRunning(l_recorder_ref), (char *)stream_data);

	if (l_recorder_ref != nullptr && recorderIsRunning(l_recorder_ref)) {
		addRecordingVideoStreamData(l_recorder_ref, stream_data, length);
		return true;
	}

	return false;
}
bool Recorder_IsRecording(void)
{
	if(l_recorder_ref)
		return isRecorderRecording(l_recorder_ref);  //return recorderIsRunning(l_recorder_ref);

	return false;
}
bool Recorder_isEventRunning(void)
{
	//return isEventRecording(isEventRecording);
	if(l_recorder_ref)
		return isEventRunning(l_recorder_ref);

	return false;
}

bool Recorder_isMotionRunning(void)
{
	//return isEventRecording(isEventRecording);
	if(l_recorder_ref)
		return isMotionRecording(l_recorder_ref);

	return false;
}

bool Recorder_isMotionMode(void)
{
	return (l_sys_setup.cfg.iRecordingMode == CFG_REC_MODE_EVENT);
}

bool Recorder_isNetExist(void)
{
	return l_net_exist;
}

int Recorder_get_HttpdDebugEnable(void)
{
	return l_rec_env.httpdDebugEnable;
}

char Recorder_get_recording_file_type(bool motion_recording)
{
	CPulseData pulse = evdev_input_get_pulsedata();

	char type = REC_FILE_TYPE_NORMAL;

	if(l_rec_env.RecordingMode == RECORD_TimelapseMode || l_rec_env.RecordingMode == RECORD_Parking_Timelapse)
		type = REC_FILE_TYPE_TIMELAPSE;

	if(motion_recording)
		type = REC_FILE_TYPE_MOTION;
	
	if(pulse.m_bTR){
		if(pulse.m_bBgr)
			type = REC_FILE_TYPE_INPUT2;
		else
			type = REC_FILE_TYPE_INPUT1;
	}

	return type;
}

std::string Recorder_get_recording_file_name(const char * dir, struct tm tm_t, char file_type, int ch, std::string ext)
{
#if (defined(DEF_VRSE_SECURITY_FILE_EXT))
	ext = DEF_VRSE_SECURITY_FILE_EXT;
#endif

	return oasis::format("%s/%4d%02d%02d_%02d%02d%02d_%c%d.%s", dir, tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec, file_type, ch, ext.c_str());
}

std::string Recorder_get_recording_file_name_pai_r(const char * dir, struct tm tm_t, char file_type, int ch, std::string ext, int event_type)
{
	char szFileType[3] = { 'G', 0, 0};
	
#if (defined(DEF_VRSE_SECURITY_FILE_EXT))
	ext = DEF_VRSE_SECURITY_FILE_EXT;
#endif

	switch(event_type){
		case kStartEventSuddenAcceleration:		strcpy(szFileType, "SA");	break;
		case kStartEventSuddenStart:					strcpy(szFileType, "SS");	break;
		case kStartEventSuddenDeacceleration:	strcpy(szFileType, "SD");	break;
		case kStartEventRapidLeft:						strcpy(szFileType, "RL");	break;
		case kStartEventRapidRight:						strcpy(szFileType, "RR");	break;
		case kStartEventOverSpeed:					strcpy(szFileType, "OG");	break;
		case kStartEventFastSpeed:						strcpy(szFileType, "OF");	break;
		case kStartEventGSensorLevelA:				strcpy(szFileType, "GA");	break;
		case kStartEventGSensorLevelB:				strcpy(szFileType, "GB");	break;
		case kStartEventGSensorLevelC:				strcpy(szFileType, "GC");	break;
	}

	return oasis::format("%s/%4d%02d%02d_%02d%02d%02d_%c%d_%s.%s", dir, tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec, file_type, ch, szFileType, ext.c_str());
}

#if ENABLE_DATA_LOG
void Recorder_datalog_add(time_t t)
{
#ifdef DEF_SAFE_DRIVING_MONITORING
	if(l_datalog.m_last_add_time != t){
#ifdef DEF_SAFE_DRIVING_MONITORING_ONOFF
		if(l_sys_setup.cfg.bsafemonitoring)		//test_241127	
			safe_driving_monitoring_osakagas();
#else
		safe_driving_monitoring_osakagas();
#endif
	}
#endif
		if(l_datalog.m_last_add_time != t && l_sys_setup.isInit()){
			ST_DATA_LOG_ITEM log_data;
			PST_GSENSOR_CONTROL gctl = G_sensorGetControl();
			CPulseData pulse = evdev_input_get_pulsedata();
			
			l_datalog.m_last_add_time = t;

			log_data.Time = t;		 /* log time */
 			log_data.OperatingTime = get_tick_count() / 1000;
 
 			log_data.GpsConnectionState = pulse.m_bGpsConnectionState;
 			log_data.GpsSignalState = pulse.m_bGpsSignalState;
 			log_data.Latitude = pulse.m_fLatitude;
 			log_data.Longitude = pulse.m_fLongitude;

			if(pulse.m_bPulseState && pulse.m_pulse_count_accrue)
 				log_data.Speed = (u32)pulse.m_fPulseSpeed;	// Km/h
 			else
				log_data.Speed = (u32)pulse.m_fGpsSpeed;
				
 			log_data.Distance = pulse.m_fDistance / 1000; // Km
 			log_data.RPM = (u16)pulse.m_iRpm;
 
 			log_data.GS_Event = l_datalog.m_event_recording;
			l_datalog.m_event_recording = false;
			
 			log_data.GS_X = gctl->X;
 			log_data.GS_Y = gctl->Y;
 			log_data.GS_Z = gctl->Z;

 			log_data.Brake = (u8)pulse.m_bBrk;
 			log_data.Winker_L = (u8)pulse.m_bSL;
 			log_data.Winker_R =(u8)pulse.m_bSR;
 			log_data.InputTrigger1 = (u8)pulse.m_bTR;

			if(pulse.m_bInput1Evt){
				log_data.InputTrigger1 = (u8)evdev_input_get_input1Event();
			}
			
 			log_data.InputTrigger2 = (u8)pulse.m_bBgr;

			if(pulse.m_bInput2Evt){
				log_data.InputTrigger2 = (u8)evdev_input_get_input2Event();
			}
			
#if DEF_SUB_MICOM_USE
			log_data.Volt = l_sub_mcu.m_volt;
#else
 			log_data.Volt = 0.0;
#endif
			log_data.Cause = l_datalog.m_event_cause_category;
			log_data.Difference = l_datalog.m_event_cause_difference;
#ifdef DEF_SAFE_DRIVING_MONITORING			
			//add
			if(safe_driving_get_fastspeed_mode() == true){
				l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_FASTSPEED;
				l_datalog.m_event_cause_difference = safe_driving_get_overspeed_count();
			}
			else if(safe_driving_get_overspeed_mode() == true){
				l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_OVERSPEED;
				l_datalog.m_event_cause_difference = safe_driving_get_overspeed_count();
			}
			else{
				l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_NO_EVENT;
				l_datalog.m_event_cause_difference = 0;
			}
			//log_data.Cause = l_datalog.m_event_cause_category;
			//log_data.Difference = l_datalog.m_event_cause_difference;
			log_data.Highway = (int)safe_driving_get_highway_mode();
#else
			log_data.Cause = l_datalog.m_event_cause_category;
			l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_NO_EVENT;

			log_data.Difference = l_datalog.m_event_cause_difference;
			l_datalog.m_event_cause_difference = 0;
#endif
			
			l_datalog.add_log(&log_data);
		}
}
#endif

static uint8_t strd_data_header_[24] = {0, };

namespace oasis {
	uint64_t getFreeMemorySize();
	int32_t get_profile_string(const char *section, const char *key, const char *default_str, char *buf, int32_t buf_size, const char *file);

	void dumpParameters(const char *tag, key_value_map_t &parameters, const char *key = nullptr);
}

class MyRecordingObserver : public MediaObserver 
{
  public:
	virtual void onStarted(void* user_data, const char* file_path) {
		DLOG(DLOG_RECORD |DLOG_INFO, "Recording started, file path \"%s\"\r\n", file_path);
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(strlen(file_path) > 20) // skip "[sniffing]"
			strcpy(l_normal_recording_file_path, file_path);
#endif		
	}

	virtual void onStopped(void* user_data, media_report_reason_t reason, void* details) {
		DLOG(DLOG_RECORD |DLOG_INFO, "Recording stopped, reason: %d(%s)\r\n", reason, mediaReportReasonString(reason));
		if(reason != kMediaReportNoError) {
			postCommand(kStopRecording);
		}

#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(strlen(l_normal_recording_file_path)) {
			u32 idx = l_st_motion_recording_file_path.idx % FILE_PATH_BUFFER_COUNT;
			l_st_motion_recording_file_path.idx++;
			
			strcpy(l_st_motion_recording_file_path.path[idx], l_normal_recording_file_path);
			//postCommand( kRecordingCompleted + idx);
			
			strcpy(l_normal_recording_file_path, "");
			//sysfs_replace(file_path, "FVFS", 4, 0); //RIFF => FVFS
		}
#endif	
	
	}

	virtual void onFileChanged(void* user_data, const char* new_file_path) {
		DLOG(DLOG_RECORD |DLOG_INFO, "New file used: \"%s\"\r\n", new_file_path);
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(strlen(l_normal_recording_file_path)) {
			u32 idx = l_st_motion_recording_file_path.idx % FILE_PATH_BUFFER_COUNT;
			l_st_motion_recording_file_path.idx++;
			
			strcpy(l_st_motion_recording_file_path.path[idx], l_normal_recording_file_path);
			postCommand( kRecordingCompletedNormal + idx);
		}
		
		strcpy(l_normal_recording_file_path, new_file_path);
#endif
	}

	virtual void onError(void* user_data, media_report_reason_t reason, void* details) {

		if(reason == kRecordingErrorMediaDataTimeout) {
			int32_t camera_id = (int32_t)details;
			DLOG(DLOG_RECORD|DLOG_ERROR, "Recording error, reason: %d(%s), camera<%d>\r\n", reason, mediaReportReasonString(reason), camera_id);
			if(camera_id == 0){	//imx307 error_reboot 250228
				recorder_reboot();
				//postCommand(kStopRecording);
			}
		} else if(reason == kRecordingErrorMediaCacheReadErrorBegin) {
			DLOG(DLOG_RECORD|DLOG_WARN, "Recording error, reason: %d(%s)\r\n", reason, mediaReportReasonString(reason));
		} else if(reason == kRecordingErrorMediaCacheReadErrorEnd) {
			DLOG(DLOG_RECORD|DLOG_WARN, "Recording error, reason: %d(%s)\r\n", reason, mediaReportReasonString(reason));
		}
	}

	virtual void onInfo(void* user_data, MediaInfo* info) {
		DLOG(DLOG_RECORD, "Recording duration %lld.%06lld sec\r\n", info->durationUs / 1000000ll, info->durationUs % 1000000ll);
	}

#define ENABLE_REAR_NO_SIGNAL 	0
#if ENABLE_REAR_NO_SIGNAL
	u32 preVideo2Lengt = 0;
	int Video2error = 0;
#endif

	virtual void onInfoEx(void* user_data, MediaInfoEx* info) {
		int h, m, s, u;
		char normal_qsize[128], event_qsize[128], in_size[128], out_size[128], cache_in_size[128];
		char video1_size[128], video2_size[128], meta_size[128], audio_size[128], file_size[128];

		l_durationUs = info->durationUs;
		parseUsec(info->durationUs, h, m, s, u);

		DLOG0(DLOG_RECORD, "Recording duration \033[33m%02d:%02d:%02d.%06d\033[0m, wq#%d(%d), eq#%d(%d), meq#%d(%d), nohits#%u, mb.avail#%d/%d (free mem %.3fMB, cpu %.2f%%)\r\n", h, m, s, u, info->writerStat.curSamples, info->writerStat.maxSamples, info->timeshiftStat.curSamples, info->timeshiftStat.maxSamples, info->writerStat.motionStat.curSamples, info->writerStat.motionStat.maxSamples, info->mediaCacheStat.nohits_, info->mediaCacheStat.free_buffer_count_, info->mediaCacheStat.total_buffer_count_, (double)oasis::getFreeMemorySize()/1024.0/1024.0, getCPUUsage());

		//offs state
		bytesToString(info->offsStat.qNormalSize, normal_qsize);
		bytesToString(info->offsStat.qEventSize, event_qsize);

		bytesToString(info->offsStat.inSize, in_size);
		bytesToString(info->offsStat.outSize, out_size);

		bytesToString(info->mediaCacheStat.in_size_, cache_in_size);
		

		TRACE0("    cache: during %d msec, in %s\r\n", info->mediaCacheStat.check_duration_, cache_in_size);
		TRACE0("    offs: n#%d, e#%d, nz#%s, ez#%s, during %d msec: in %s out %s elapsed %d msec \r\n", info->offsStat.qNormalCount, info->offsStat.qEventCount, normal_qsize, event_qsize, info->offsStat.checkDuration, in_size, out_size, info->offsStat.elapsedSum);

		//TRACE0("    cache: hits#%u, nohits#%u, free#%u, gets#%u, puts#%u\r\n", info->mediaCacheStat.hits_, info->mediaCacheStat.nohits_, info->mediaCacheStat.free_buffer_count_, info->mediaCacheStat.get_buffer_count_, info->mediaCacheStat.put_buffer_count_);

		//print recording stat in details
		bytesToString(info->writerStat.video1Length, video1_size);
		bytesToString(info->writerStat.video2Length, video2_size);
		bytesToString(info->writerStat.metaLength, meta_size);
		bytesToString(info->writerStat.audioLength, audio_size);
		bytesToString(info->writerStat.fileLength, file_size);
		TRACE0("    %s: video1 %s, video2 %s, meta %s, audio %s, file %s\n", info->sniffing?"sniffing":"recording", video1_size, video2_size, meta_size, audio_size, file_size);
		
		if(info->writerStat.motionStat.recording) {
			bytesToString(info->writerStat.motionStat.video1Length, video1_size);
			bytesToString(info->writerStat.motionStat.video2Length, video2_size);
			bytesToString(info->writerStat.motionStat.metaLength, meta_size);
			bytesToString(info->writerStat.motionStat.audioLength, audio_size);
			bytesToString(info->writerStat.motionStat.fileLength, file_size);
			TRACE0("    motion: video1 %s, video2 %s, meta %s, audio %s, file %s\n", video1_size, video2_size, meta_size, audio_size, file_size);
		}

		if(info->timeshiftStat.recording) {
			bytesToString(info->timeshiftStat.video1Length, video1_size);
			bytesToString(info->timeshiftStat.video2Length, video2_size);
			bytesToString(info->timeshiftStat.metaLength, meta_size);
			bytesToString(info->timeshiftStat.audioLength, audio_size);
			bytesToString(info->timeshiftStat.fileLength, file_size);
			TRACE0("    event: video1 %s, video2 %s, meta %s, audio %s, file %s\n", video1_size, video2_size, meta_size, audio_size, file_size);
		}
		
#if ENABLE_REAR_NO_SIGNAL
		if(preVideo2Lengt < info->writerStat.video2Length){
			int size_kb = (info->writerStat.video2Length - preVideo2Lengt) / 1024;

			if(rear_camera_detected && size_kb < 10){ // 8kb is no signal
				if(Video2error++ >= 3){
					rear_camera_detected = false;
					Video2error = 0;
					DLOG0(DLOG_ERROR, " error!, rear camera disconnected. (%d KB)\r\n", size_kb );
				}
			}

			//DLOG0(DLOG_WARN, " %d		%d KB\r\n", info->writerStat.video2Length - preVideo2Lengt, size_kb );
		}
		preVideo2Lengt = info->writerStat.video2Length;
#endif

		//SD Writing Check
		if(info->writerStat.fileLength == l_pre_writerStat_fileLength){
			if(l_writerStat_fileLength_error_count++ > 5){
				DLOG0(DLOG_ERROR, " SD Card with insufficient writing speed. Buffering time %d Sec.\r\n", l_writerStat_fileLength_error_count );

				SYSTEMLOG(LOG_TYPE_ERROR, LOG_ERROR_SD_DISK_INIT_FAIL ,0 , "WRITING SPEED");
				l_writerStat_fileLength_error_count = 0;
			}
		}
		else {
			l_writerStat_fileLength_error_count = 0;
			l_pre_writerStat_fileLength = info->writerStat.fileLength;
		}

	}

	//player paused and resumed
	virtual void onPaused(void *user_data, MediaInfo *info) {}
	virtual void onResumed(void *user_data, MediaInfo *info) {}

	virtual void onEventRecordingStarted(void* user_data, media_report_reason_t reason, const char* file_path) {
		DLOG(DLOG_RECORD, "Event Recording started @ \"%s\".\r\n", file_path);
		led_set_event_recording_start();		
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(l_st_recording_file_path.idx)
			postCommand( kRecordingCompleted + ((l_st_recording_file_path.idx - 1) % FILE_PATH_BUFFER_COUNT));
#endif	

#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		{
			u32 idx = l_st_recording_file_path.idx % FILE_PATH_BUFFER_COUNT;
			l_st_recording_file_path.idx++;
			
			strcpy(l_st_recording_file_path.path[idx], file_path);
			//postCommand( kRecordingCompleted + idx);
			
			//sysfs_replace(file_path, "FVFS", 4, 0); //RIFF => FVFS
		}
#endif
	}

	virtual void onEventRecordingCompleted(void* user_data, media_report_reason_t reason, const char* file_path) {
		DLOG(DLOG_RECORD, "Event Recording completed @ \"%s\".\r\n", file_path);
		
		if(auto_recording_event_) {
			postCommand(kStartEventRecording);
		}
		else{
			led_set_event_recording_end();
		}		
	}

	virtual void onMotionRecordingStarted(void *user_data, media_report_reason_t reason, const char *file_path) {
		DLOG(DLOG_RECORD, "Motion Recording started @ \"%s\".\n", file_path);
		led_set_motion_recording_start();
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(l_st_motion_recording_file_path.idx)
			postCommand( kRecordingCompletedMotion + ((l_st_motion_recording_file_path.idx - 1) % FILE_PATH_BUFFER_COUNT));
#endif

#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		{
			u32 idx = l_st_motion_recording_file_path.idx % FILE_PATH_BUFFER_COUNT;
			l_st_motion_recording_file_path.idx++;
			
			strcpy(l_st_motion_recording_file_path.path[idx], file_path);
			//postCommand( kRecordingCompleted + idx);
			
			//sysfs_replace(file_path, "FVFS", 4, 0); //RIFF => FVFS
		}
#endif	
	}
	
	virtual void onMotionRecordingCompleted(void *user_data, media_report_reason_t reason, const char *file_path) {
		DLOG(DLOG_RECORD, "Motion Recording completed @ \"%s\".\n", file_path);
		led_set_motion_recording_wait();				
	}
	virtual void queryNewFilePath(void* user_data, oasis::recording_mode_t file_type, bool rear_camera_on, bool sound_on, std::string& file_path) {
#if 0
		//test for the same file name
		std::string new_file_path;
		if(is_avi_format_) {
			new_file_path = "/mnt/extsd/NORMAL/rec-1234567890.avi";
		} else {
			new_file_path = "/mnt/extsd/NORMAL/rec-1234567890.mp4";
		}

		DLOG(DLOG_RECORD, "queryNewFilePath: mode: %d, rear on %d, sound on %d, path: %s -> %s\r\n", file_type, rear_camera_on, sound_on, file_path.c_str(), new_file_path.c_str());
		file_path = new_file_path;
#else

		std::string ext = "mp4";

		l_normal_recording_type = Recorder_get_recording_file_type(false);

		if(is_avi_format_) {
			ext = "avi";
		}
		struct tm tm_t;
			
		//oasis v4.1.14 
		// /mnt/extsd/NORMAL/2020_05_01_02_20_59.avi
		int result = 0;
		int camera_counts = rear_disabled_ ? 1 : 2;
		const char * file_fmt = VIDEO_NORMAL_DIR "/%4d_%02d_%02d_%02d_%02d_%02d";

		
		if(strstr(file_path.c_str(), "oasis-") != NULL)
			file_fmt = VIDEO_NORMAL_DIR "/oasis-%4d_%02d_%02d_%02d_%02d_%02d";
		
		result = sscanf(file_path.c_str(), file_fmt, &tm_t.tm_year, &tm_t.tm_mon, &tm_t.tm_mday, &tm_t.tm_hour, &tm_t.tm_min, &tm_t.tm_sec);

#ifdef TARGET_CPU_V536	
			camera_counts = l_recorder_camera_counts;
#endif

		if(result == 6){
			tm_t.tm_year -= 1900;
			tm_t.tm_mon -= 1;
			file_path = Recorder_get_recording_file_name(VIDEO_NORMAL_DIR, tm_t, l_normal_recording_type, camera_counts, ext);
		}
		else {
	 		//std::string new_file_path = std::strtok((char *)file_path.c_str(), ".");
			file_path.resize(file_path.size() - 4);
				
			file_path = oasis::format("%s_%c%d.%s", file_path.c_str(), l_normal_recording_type, camera_counts, ext.c_str());
		}

		
		DLOG(DLOG_RECORD, "queryNewFilePath: mode: %d, rear on %d, sound on %d, path: %s\r\n", file_type, rear_camera_on, sound_on, file_path.c_str());
#endif
#ifdef DEF_PAI_R_DATA_SAVE
		if(l_net_exist){
			pai_r_datasaver_addLocation_new(file_path, false, 0);

			postCommand(kTakeFrontSnapshot);
		}
#endif // end of DEF_PAI_R_DATA_SAVE
	}


	virtual void onSnapshotCompleted(void *user_data, uint32_t snapshot_id, int32_t camera_id, int error, const std::vector<char> &image_data, const struct timeval &timestamp) {

		DLOG(DLOG_RECORD, "snapshot completed: id 0x%x, camera %d, error %d, size %d bytes @ %d sec\n", snapshot_id, camera_id, error, image_data.size(), timestamp.tv_sec);

		if((error == 0 || error == 600) && image_data.size() > 0) {
#ifdef DEF_PAI_R_DATA_SAVE
			if(l_net_exist){
				bool is_event = false;

				if(snapshot_id & DEF_SNAPSHOT_TYPE_MASK)
					is_event = true;

				pai_r_datasaver_addThumbnail((u8 *)image_data.data(), image_data.size(), is_event);				
			}
#else			
			time_t t = (time_t)timestamp.tv_sec;
			struct tm tm_t;
			localtime_r(&t, &tm_t);
			char path[512];
			sprintf(path, "/tmp/mnt/camera%d-snapshot-%4d%02d%02d-%02d%02d%02d.jpg", camera_id, tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
			FILE *fp = fopen(path, "wb");
			if(fp) {
				fwrite(image_data.data(), 1, image_data.size(), fp);
				fclose(fp);
				DLOG(DLOG_INFO, "%s saved %d bytes.\n", path, image_data.size());
			} else {
				DLOG(DLOG_ERROR, "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
			}
#endif	

#if 1
			if( (snapshot_id) % 2 == 0){
				postCommand(kTakeRearSnapshot);
			}
#endif
		}
		else {
			if( (snapshot_id) % 2 == 0){
				postCommand(kTakeFrontSnapshot);
			}
			else
				postCommand(kTakeRearSnapshot);
		}
	}

};

class MyFrameObserver : public ui::PreviewFrameObserver {
  public:
	MyFrameObserver() { }

	int Video2error = 0;
	int Video2OkCnt = 0;
	int Video3error = 0;
	int Video3OkCnt = 0;
	int frame_cnt = 0;
	void onFrame(int32_t camera_id, uint8_t *frame, int32_t stride_width, int32_t width, int32_t height)
	{
#if !ENABLE_DATA_LOG
		time_t t=0;
		time(&t);
		Recorder_datalog_add(t);
#endif

#if 1	
		//DLOG0(DLOG_ERROR, " %d camera  (%dx%d)\r\n", camera_id, width, height );
		if((frame_cnt++ % 10) == 0){
			if(camera_id == 2){
				//fram yuv420
				//n = Height x Width
				//Y=n | Cb=n/4 | cr=n/4
				//frame data size = n + (n/4) + (n/4)
				//int size = width * height;
				u64 size = width; // 1 line check
				int start = width * (height / 2);
				u64 value = 0;
				for (unsigned int i = start; i < (size + start); i++) {
					value += (u64)frame[i];
				}

				if(value == (16 * size)){ // 16 is no signal pixel
					Video2OkCnt = 0;
					//if(rear_camera_detected){
					if((l_recorder_camera2_id != -1) && (Video2error++ >= 30)){
						
						if(l_recorder_camera_counts == 2)
							rear_camera_detected = false;
						
						rear_disabled_ = rear_camera_detected; // restart recording
						l_recorder_camera2_id = -1;
						l_recorder_camera_counts--;
						DLOG0(DLOG_ERROR, "error!, rear camera #1 disconnected. (%d)\r\n", value);
					}
				}
				else{
					Video2error= 0;
					if(l_recorder_camera2_id == -1){
						if(value > (16 * size)) //Aging?? ???????? ?????? ??? ????? ??츸 2Ch?? ???
							Video2OkCnt++;
						else
							Video2OkCnt = 0;
						
						if(Video2OkCnt >= 10){//10
							rear_camera_detected = true;
							rear_disabled_ = true;
							l_recorder_camera2_id = 2;
							l_recorder_camera_counts++;
							DLOG0(DLOG_WARN, " rear camera #1 detected. (%d)\r\n", value);
						}
					}
				}
			}
		
			if(camera_id == 3){
				u64 size1 = width; // 1 line check
				int start1 = width * (height / 2);
				u64 value1 = 0;
				for (unsigned int j = start1; j < (size1 + start1); j++) {
					value1 += (u64)frame[j];
				}

				if(value1 == (16 * size1)){ // 16 is no signal pixel
					Video3OkCnt = 0;
					if((l_recorder_camera3_id != -1) && (Video3error++ >= 30)){
						
						if(l_recorder_camera_counts == 2)
							rear_camera_detected = false;
						
						rear_disabled_ = rear_camera_detected; // restart recording
						l_recorder_camera3_id = -1;
						l_recorder_camera_counts--;
						DLOG0(DLOG_ERROR, "error!, rear camera #2 disconnected. (%d)\r\n", value1);	
					}
				}
				else{
					Video3error= 0;
					if(l_recorder_camera3_id == -1){
						if(value1 > (16 * size1)) //Aging?? ???????? ?????? ??? ????? ??츸 2Ch?? ???
							Video3OkCnt++;
						else
							Video3OkCnt = 0;
						
						if(Video3OkCnt >= 12){//10
							rear_camera_detected = true;
							rear_disabled_ = true;
							l_recorder_camera3_id = 3;
							l_recorder_camera_counts++;
							DLOG0(DLOG_WARN, " rear camera #2 detected. (%d)\r\n", value1);
						}
					}
				}
			}
			
			//printf("CAM2 ID = %d, CAM3 ID = %d, frame_cnt = %d\r\n", l_recorder_camera2_id, l_recorder_camera3_id, frame_cnt);
			
			if(frame_cnt >= 60){
				frame_cnt = 0;
				//DLOG(DLOG_FRAME, "%d %dx%d, [%lld]\n", camera_id, width, height, value);
			}
		}
	}	
#endif
	
};

class MyAdasObserver : public AdasObserver 
{
 public:
	MyAdasObserver() {}
	virtual ~MyAdasObserver() {}

	virtual void onCarCrashWarning(int32_t camera_id, CarInfo &car) {}
	virtual void onLane(int32_t camera_id, LaneInfo &lane, unsigned char lt_warn, unsigned char rt_warn) {}
	virtual void onCars(int32_t camera_id, int32_t num_cars, CarInfo *cars) {}
	virtual void onPlates(int32_t camera_id, int32_t num_plates, PlateInfo *plates) {}
	virtual void onSaveData(int32_t camera_id, uint16_t vanishline, uint8_t vanishline_ok, int32_t width, int32_t height, uint16_t *data) {}
};

class MyMotionDetectObserver : public MotionDetectObserver 
{
 public:
	MyMotionDetectObserver() {}
	virtual ~MyMotionDetectObserver() {}

	virtual void onMotionDetected(int32_t camera_id, int32_t detected) {
		TRACE0("camera<%d> motion detected#%d\n", camera_id, detected);

		if (detected) {
			if(!isMotionRecording(l_recorder_ref)){
				//DLOG(DLOG_TRACE, "image#%d, detected? %d\r\n", camera_id, detected);
				postCommand(kStartMotionRecording);
			}
		}
	}

};


void print_menu()
{
	DLOG(DLOG_TRACE, "------------------------------------------------------\n");
	DLOG(DLOG_TRACE, "[Menu]\n");
	DLOG0(DLOG_INFO, "    w: swap preview surfaces\n");
	DLOG0(DLOG_INFO, "    e: start event recording\n");
	DLOG0(DLOG_INFO, "    r: restart ecording\n");
	DLOG0(DLOG_INFO, "    t: toggle auto-start event recording (current: %s)\n", auto_recording_event_?"ENABLED":"DISABLED");
	DLOG0(DLOG_INFO, "    n: toggle auto stop and restart normal recording when event recording starts and ends (current: %s).\n", auto_stop_and_restart_normal_recording_?"ENABLED":"DISABLED");
#if  0//ENABLE_ADAS
	DLOG0(DLOG_INFO, "    a: enable/disable ADAS\n");
#endif
	DLOG0(DLOG_INFO, "    p: start / stop recording\n");
	DLOG0(DLOG_INFO, "    m: change recording mode between driving and parking\n");
	DLOG0(DLOG_INFO, "    s: take front-camera snapshot\n");
	if(rear_disabled_ == false) {
		DLOG0(DLOG_INFO, "    d: take rear-camera snapshot (if rear enabled)\n");
	}
	
	//DLOG0(DLOG_INFO, "    f: make OFFS queue full (test only)\n");
	//DLOG0(DLOG_INFO, "    h: make OFFS queue half-full (test only)\n");
	//DLOG0(DLOG_INFO, "    r: release OFFS queue full or half-full (test only)\n");
	DLOG0(DLOG_INFO, "    x: exit\n");
	DLOG(DLOG_TRACE, "------------------------------------------------------\n");
}


void cancel_handler(int signum) {
#if DEF_SUB_MICOM_USE	
	if(signum != 0)
		l_sub_mcu.SetWatchdogTme(0);
#endif
	timer_running_ = false;
	tty_.tty_cancel();
}

static void abort_main()
{
	ui::cleanup();
	oasis::finalize();	
}

struct parameter_info {
	const char *name;
	bool noprefix;
	bool optional;
};

struct prefixed_parameter_info {
	const char *prefixed_name;
	const char *parameter_name;
	bool optional;
};

static struct parameter_info source_parameter_list_[] = {
	{ "source%d-camera-id", false, false },
	{ "source%d-capture-resolution", false, true },
	{ "source%d-capture-width", false, true },
	{ "source%d-capture-height", false, true },
	{ "source%d-isp-id", false, true },
	{ "source%d-isp-wdr-mode", false, true },
	{ "source%d-capture-buffers", false, false },
	{ "source%d-fps", false, false },
	{ "source%d-preview-id", false, true },
	{ "source%d-loc", false, true },
	{ "source%d-subchannel-rotation", false, true },
	{ nullptr, false, false }
};


static struct parameter_info display_parameter_list_[] = {
	{ "tv-mode", true, true },
	{ "memory-type", true, true },
	{ "screen-width", true, true },
	{ "screen-height", true, true },
	{ nullptr, false, false }
};


static struct parameter_info camera_composer_parameter_list[] = {
	{ "component%d-camera-id", false, false },
	{ "component%d-x", false, false },
	{ "component%d-y", false, false },
	{ "component%d-width", false, false },
	{ "component%d-height", false, false },
	{ nullptr, false, false }
};	

static struct parameter_info ise_parameter_list_[] = {
	//ise settings
	{ "ise%d-camera-id", false, false },
	{ "ise%d-outbuf-num", false, false },
	{ "ise%d-dewarp-mode", false, false },
	{ "ise%d-mount-mode", false, false },

	{ "ise%d-normal-pan", false, true },
	{ "ise%d-normal-tilt", false, true },
	{ "ise%d-normal-zoom", false, true },

	{ "ise%d-ptz4in1-pan-0", false, true },
	{ "ise%d-ptz4in1-tilt-0", false, true },
	{ "ise%d-ptz4in1-zoom-0", false, true },
	{ "ise%d-ptz4in1-pan-1", false, true },
	{ "ise%d-ptz4in1-tilt-1", false, true },
	{ "ise%d-ptz4in1-zoom-1", false, true },
	{ "ise%d-ptz4in1-pan-2", false, true },
	{ "ise%d-ptz4in1-tilt-2", false, true },
	{ "ise%d-ptz4in1-zoom-2", false, true },
	{ "ise%d-ptz4in1-pan-3", false, true },
	{ "ise%d-ptz4in1-tilt-3", false, true },
	{ "ise%d-ptz4in1-zoom-3", false, true },

	{ "ise%d-flip-enable", false, true },
	{ "ise%d-mirror-enable", false, true },

	{ nullptr, false, false }
};

static struct parameter_info recorder_parameter_list_[] = {
	{ "filedir", true, false },
	{ "file-prefix", true, false },
	{ "file-extension", true, false },
	{ "file-duration-secs", true, false },
	{ "driving-folder-path", true, false },
	{ "event-folder-path", true, false },
	{ "motion-folder-path", true, false },
	{ "event-pre-recording-seconds", true, false },
	{ "event-post-recording-seconds", true, false },
	{ "motion-pre-recording-seconds", true, false },
	{ "motion-post-recording-seconds", true, false },
	{ "max-files", true, false },
	{ "delete-oldest-file-on-max-files", true, false },
	{ "recording-size-limit-threshold-seconds", true, false },
	{ "report-media-info-ex", true, false },
	{ "avi-strd-size-max", true, false },
	{ "mp4-udta-size-max", true, false },
	{ "enable-persistent-cache", true, true },
	{ "disable-event-recording", true, true },
	{ "disable-offs-recording", true, true },
	{ "recording-file-header-write-interval-secs", true, true },

	{ "snd-card", true, true },
	{ "snd-dev", true, true },
	{ "snd-path", true, true },
	{ "snd-input-sampling-duration-msec", true, true },
	{ "aec-disabled", true, true },
	{ "snd-input-channels", true, false },
	{ "snd-input-sample-size", true, false },
	{ "snd-input-sampling-rate", true, false },

	{ "osd-font-size", true, true },
	{ "osd-font-face", true, false },
	{ "osd-text-color", true, false },
	{ "osd-use-fixed-size", true, true },
	{ "osd-use-text-color", true, false },
	{ "osd-use-bg-color", true, false },
	{ "osd-bg-color", true, false },
	{ "osd-use-outline-color", true, false },
	{ "osd-outline-color", true, false },
	{ "osd-horz-align", true, false },
	{ "osd-vert-align", true, false },
	{ "osd-font-path", true, false },
	
	{ nullptr, false, false }
};

static struct parameter_info recorder_channel_parameter_list_[] = {
	{ "channel%d-camera-id", false, false },
	{ "channel%d-ise-id", false, true },
	{ "channel%d-hflip", false, true },
	{ "channel%d-vflip", false, true },
	{ "channel%d-isp-id", false, true },
	{ "channel%d-isp-wdr-mode", false, true },
	{ "channel%d-resolution", false, true },
	{ "channel%d-out-width", false, true },
	{ "channel%d-out-height", false, true },

	//overrides
	{ "channel%d-driving-folder-path", false, true },
	{ "channel%d-event-folder-path", false, true },
	{ "channel%d-motion-folder-path", false, true },
	{ "channel%d-enable-persistent-cache", false, true },
	{ "channel%d-recording-size-limit-threshold-seconds", false, true },

	{ "channel%d-bitrate", false, false },
	{ "channel%d-fps", false, false },
	{ "channel%d-avi-framerate", false, true },
	{ "channel%d-vencoder-type", false, false },
	
	{ "channel%d-venc-framerate", false, true },
	{ "channel%d-venc-keyframe-interval", false, true },

	//for h264
	{ "channel%d-h264-framerate", false, true },
	{ "channel%d-h264-keyframe-interval", false, true },
	{ "channel%d-h264-profile", false, true },
	{ "channel%d-h264-level", false, true },
	{ "channel%d-h264-enable-cabac", false, true },
	{ "channel%d-min-qp", false, true },
	{ "channel%d-max-qp", false, true },
	{ "channel%d-enable-fixqp", false, true },
	{ "channel%d-fix-iqp", false, true },
	{ "channel%d-fix-pqp", false, true },

	//for h265
	{ "channel%d-h265-framerate", false, true },
	{ "channel%d-h265-keyframe-interval", false, true },
	{ "channel%d-h265-profile", false, true },
	{ "channel%d-h265-level", false, true },
	{ "channel%d-h265-rc-mode", false, true },
	//for cbr, vbr
	{ "channel%d-h265-min-qp", false, true },
	{ "channel%d-h265-max-qp", false, true },
	//for abr
	{ "channel%d-h265-max-bitrate", false, true },
	{ "channel%d-h265-min-iqp", false, true },
	{ "channel%d-h265-max-iqp", false, true },
	//for fixqp
	{ "channel%d-h265-fix-iqp", false, true },
	{ "channel%d-h265-fix-pqp", false, true },

	{ "channel%d-h265-gop-mode", false, true },
	{ "channel%d-h265-gop-smart-virtual-interval", false, true },
	{ "channel%d-h265-fast-encoding", false, true },
	{ "channel%d-h265-enable-pframe-intra", false, true },

	{ "channel%d-media-wait-timeout-secs", false, true },
	{ "channel%d-media-wait-timeout-notify-oneshot", false, true },
	{ "channel%d-osd-font-size", false, true },

	{ nullptr, false, false }
};

static oasis::key_value_map_t camera_composer_parameters;
static MotionDetectorRef motion_detector = nullptr;
static std::list<MotionDetectorRef> motion_detectors;
static AdasRef adas = nullptr;
static bool l_has_preview = false;

void recorder_reboot(void)
{
	sleep(1);
#if DEF_SUB_MICOM_USE		
	l_sub_mcu.SetPowerMode(BB_MICOM_POWER_OFF_FALL);
	sleep(1);
#endif	
	setuid (0);
	cancel_handler(0);
	system("reboot");
}

bool recorder_fw_update_file_check(void)
{
	const char * firmware_file = DA_FIRMWARE_UPDATE_FILE_NAME;
	const char * firmware_dir = DA_FIRMWARE_UPDATE_FILE_DIR;
	char szCmd[128];
	
	char find_file[128] = {0, };
	char fw_file_path[128] = {0, };

	sprintf(fw_file_path, "%s%s", firmware_dir, firmware_file);
	
	sprintf(szCmd, "find %s -name %s", firmware_dir, DA_FIRMWARE_UPDATE_FILE_NAME_FIND);
		
	if(SB_CmdScanf(szCmd, "%s", find_file)){
		if( strncmp(fw_file_path, find_file, strlen(fw_file_path) - 4) == 0)
			strcpy(fw_file_path, find_file);
	}
	
	if (access(fw_file_path, F_OK) == 0) {
	// {++ checksum
		FILE *file = fopen(fw_file_path, "rb");
		if(!file){
			DLOG0(DLOG_ERROR," [%s] file open error!\n", fw_file_path);
			return false;
		}
		else {
			int length = 0, nReadSize = 0;
			u32 nChecksum = 0;
			
			fseek( file, 0, SEEK_END );
			length = ftell( file );
			fseek( file, 0, SEEK_SET );
			
			DLOG0(DLOG_WARN, "file size : %d Byte\r\n", length);

			length = 0;
			do{
				unsigned int dumy[10 * 1024];
				nReadSize = fread( (void *)dumy, 1, sizeof(dumy), file );
				if(nReadSize){
					length += nReadSize;
					for(u32 i = 0; i < nReadSize / sizeof(unsigned int); i++){
						nChecksum += dumy[i];
					}
				}
			}while(nReadSize > 0);

			DLOG0(DLOG_WARN, "file size : %d Byte (checksum 0x%x)\r\n", length, nChecksum);
			fclose(file);

			if(nChecksum != 0){
				for(int i=0; i < 4; i++)
					mixwav_play(kMixwaveEventRecording);
				
				sprintf(szCmd, "rm %s", fw_file_path);
				system(szCmd);

				DLOG0(DLOG_ERROR," [%s] checksum error!\n", fw_file_path);
				return false;
			}
		}
	//++}
#ifdef TARGET_CPU_V536	
		sprintf(szCmd, "mv %s %sfirmware_da300.img", fw_file_path, firmware_dir);
#else
		sprintf(szCmd, "mv %s %sfirmware.img", fw_file_path, firmware_dir);
#endif
		system(szCmd);

		DLOG0(DLOG_WARN, "%s\r\n", szCmd);
	
		sprintf(szCmd, "echo \"%s\" > %s", fw_file_path, DA_FIRMWARE_UPDATE_COMPLETE_FILE_PATH);
		system(szCmd);

		DLOG0(DLOG_WARN, "%s\r\n", szCmd);

		mixwav_play(kMixwaveUpdate);
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_UPDATE, _LOG_FIRMWARE_FILE, "START");
		sleep(6);
		recorder_reboot();
		
		return true;
	}

	return false;
}

bool recorder_fw_update_complete_check(void)
{
	bool result = false;
	const char * file_path = DA_FIRMWARE_UPDATE_COMPLETE_FILE_PATH;
	
	if (access(file_path, F_OK) == 0) {
		char szCmd[128];
	
		if(sysfs_scanf(file_path, "%s", szCmd)){
			DLOG0(DLOG_WARN, "%s : \"%s\"\r\n", file_path, szCmd);
		}
		
		sprintf(szCmd, "rm %s", file_path);
		system(szCmd);
		
		DLOG0(DLOG_WARN, "%s\r\n", szCmd);
		result =  true;
	}

	if (access(DA_FIRMWARE_UPDATE_COMPLETE_DIR, F_OK) == 0) {
		char szCmd[128];
	
		sprintf(szCmd, "rm -rf %s", DA_FIRMWARE_UPDATE_COMPLETE_DIR);
		system(szCmd);
		
		DLOG0(DLOG_WARN, "%s\r\n", szCmd);
		result =  true;
	}

	if (get_tick_count() > 75000 && get_tick_count() < 90000) {
		DLOG0(DLOG_WARN, "firmware update complete!\r\n");
		result =  true;
	}

	return result;
}

bool recorder_is_sd_exist(void)
{
	if (access("/dev/mmcblk0p1", F_OK) == 0 || access("/dev/mmcblk0", F_OK) == 0) 
		return true;

	return false;
}

/*
------------------------------
30FPS FIX
	720P
		L	
		M	
		H	
	---------------------------
	1080P
		L	
		M	
		H	
------------------------------
L:6, M:15, H:30 FPS
	720P (800K step)
		L	N=5,893540		E=3,053512			(2400K) N=12209734 E=5316726				(2800K) N=14124868 E=5872032		(3.5M) N=20955978 E=7309554		(1600K) N=9,370918		E=5,328998		
		M	N=15,142434		E=7,654740			(2400K) N=21385402 E=10788224			(2800K) N=24487558 E=12282592
		H	N=39,905804		E=20,002206		(2400K) N=39853426 E=19923416			(2800K) N=46113456 E=23058166
	---------------------------
	1080P (2M step)
		L	N=12,582,494		E=3,746,378		(6M)	N=18071172		E=8551318			(7M) N=27535244		E=11018082
		M	N=24,783,358		E=12,461,306		(6M)	N=37536794		E=18776016		(7M)	N=44464822		E=22009614
		H	N=68,985,558		E=34,449,910		(6M)	N=72166792		E=36147486		(7M)	N=85991702		E=42885936

*/
bool recorder_env_setup(REC_ENV *pEnv, bool is_parking_mode)
{
	if(l_sys_setup.isInit()){
#if ENABLE_DATA_LOG
		if(access(DEF_DRIVER_CODE_FILE_PATH, R_OK ) != 0)
			l_datalog.set_driver_code(l_sys_setup.cfg.strDriverCode );
#endif		
		//pEnv->rec_secs = (2 - l_sys_setup.cfg.iVideoQuality) * 60 + 60; 
		//pEnv->rec_secs = RECORDING_DURATION_SECS;

#if DEF_VIDEO_QUALITY_ONLY
		if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
 #ifdef DEF_REAR_CAMERA_ONLY
			pEnv->front_resolution = CFG_RESOLUTION_720P;
#else			
			pEnv->front_resolution = CFG_RESOLUTION_1080P;
#endif
			pEnv->rear_resolution = CFG_RESOLUTION_720P;
		}
#if 1	//(BUILD_MODEL == BUILD_MODEL_VRHD_V1V2)
		else if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_LOW){
 #ifdef DEF_FRONT_1080P		//Test_241203
 			pEnv->front_resolution = CFG_RESOLUTION_1080P;
 			pEnv->rear_resolution = CFG_RESOLUTION_720P;
 #else		
			pEnv->front_resolution = CFG_RESOLUTION_720P;
 			pEnv->rear_resolution = CFG_RESOLUTION_360P;
 #endif
		}
#endif
		else{ //MIDDLE
 #ifdef DEF_FRONT_1080P		//Test_241203
			pEnv->front_resolution = CFG_RESOLUTION_1080P;
			pEnv->rear_resolution = CFG_RESOLUTION_720P;
 #else		
			pEnv->front_resolution = CFG_RESOLUTION_720P;
			pEnv->rear_resolution = CFG_RESOLUTION_720P;
 #endif
		}
#else
		pEnv->front_resolution = l_sys_setup.cfg.iFrontResolution;
		pEnv->rear_resolution = l_sys_setup.cfg.iRearResolution;
#endif


#ifdef DEF_REAR_CAMERA_ONLY
		if((pEnv->hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_480I)
			pEnv->front_resolution = CFG_RESOLUTION_480I;
#endif

		if((pEnv->hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_480I)
			pEnv->rear_resolution = CFG_RESOLUTION_480I;

		if(pEnv->front_resolution == CFG_RESOLUTION_1080P)
			pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P + (BITRATE_1080P / 2);
		else //if(pEnv->front_resolution == CFG_RESOLUTION_720P)
			pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_720P + (BITRATE_720P / 2);
		//else
			//pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_480I + (BITRATE_480I / 2);

		if(pEnv->rear_resolution == CFG_RESOLUTION_1080P)
			pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P + (BITRATE_1080P / 2);
		else //if(pEnv->rear_resolution == CFG_RESOLUTION_720P)
			pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_720P + (BITRATE_720P / 2);
		//else
			//pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_480I + (BITRATE_480I / 2);

#if DEF_30FIX_FRAME// fix 30 fps	
 #if 0 //bitrate test 16M File
		if(l_sys_setup.cfg.iVideoQuality == 0){
			pEnv->front_bitrate -= (200*1024);
			pEnv->rear_bitrate -= (600*1024);
		}
 #endif
		pEnv->front_fps = pEnv->rear_fps = CAMERA_FPS;

//#if DEF_VIDEO_FIX_BITRATE
		pEnv->fix_iqp = 32;
		pEnv->fix_pqp = 32;  //90M File
	if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_MIDDLE){
		pEnv->fix_iqp = 32;
		pEnv->fix_pqp = 32;	//45M File
	}
	else if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_LOW){
 #if DEF_30FIX_FRAME	
		pEnv->fix_iqp = 35;	//25;
		pEnv->fix_pqp = 35;	//40; 	//27M File
 #else
		pEnv->fix_iqp = 35;	//25;
		pEnv->fix_pqp = 35;	//40; 	//27M File
 #endif
	}	
//#endif
		
#else	
 #ifdef TARGET_CPU_V536	
#if 0	//bitrate change_240528 (4M, 6M, 8M => 4M, 5.5M, 7M)
		pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P + BITRATE_1080P;
		pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_720P;
#endif		
		if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH) {		//6M,2.5M_file size 100M/36M
			pEnv->rear_fps = pEnv->front_fps = CAMERA_FPS;
			pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P;
			pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 0.5) * BITRATE_720P;
		}
		else if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_MIDDLE){		//5M,2M_file size 44M/16M
			pEnv->rear_fps = pEnv->front_fps = 15;
			pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P + BITRATE_720P;
			pEnv->rear_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_720P;
		}
		else {		//4M,1.5M_file size 20M/8M
#ifdef DEF_FRONT_1080P		//1080p, 720p Test_241203
			pEnv->rear_fps = pEnv->front_fps = 6;
			pEnv->front_bitrate = 5000000;
			pEnv->rear_bitrate = 2250000;
#else		
			pEnv->front_fps = 10;
			pEnv->rear_fps = 6;
			pEnv->front_bitrate = (l_sys_setup.cfg.iVideoQuality + 1) * BITRATE_1080P + BITRATE_1080P;
			pEnv->rear_bitrate = BITRATE_REAR_LOW;
#endif
		}
 #else
  #if 0
		if(pEnv->front_resolution == CFG_RESOLUTION_1080P)
			pEnv->front_bitrate = 3 * BITRATE_1080P + (BITRATE_1080P / 2);
		else if(pEnv->front_resolution == CFG_RESOLUTION_720P)
			pEnv->front_bitrate = 3 * BITRATE_720P + (BITRATE_720P / 2);
		else
			pEnv->front_bitrate = 3 * BITRATE_480I + (BITRATE_480I / 2);

		if(pEnv->rear_resolution == CFG_RESOLUTION_1080P)
			pEnv->rear_bitrate = 3 * BITRATE_1080P + (BITRATE_1080P / 2);
		else if(pEnv->rear_resolution == CFG_RESOLUTION_720P)
			pEnv->rear_bitrate = 3 * BITRATE_720P + (BITRATE_720P / 2);
		else
			pEnv->rear_bitrate = 3 * BITRATE_480I + (BITRATE_480I / 2);
  #endif		
		//pEnv->front_fps = CAMERA_FPS - ((2-l_sys_setup.cfg.iVideoQuality) * 10); // 30, 20 ,10 FPS
		if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
  #if DEF_VIDEO_QUALITY_ONLY
			pEnv->front_bitrate += BITRATE_720P;		//(BITRATE_720P / 2);
			//pEnv->rear_bitrate -= (BITRATE_720P / 2);
  #endif			
			pEnv->rear_fps = pEnv->front_fps = CAMERA_FPS;
		}
		else if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_MIDDLE){
			pEnv->rear_fps = pEnv->front_fps = CAMERA_FPS;
			
			pEnv->front_bitrate += (BITRATE_720P / 2);
			pEnv->rear_bitrate -= (BITRATE_720P / 2);

			if(pEnv->rear_resolution == CFG_RESOLUTION_720P){
				pEnv->rear_fps = 15;
				pEnv->rear_bitrate *= 2;
			}
		}
		else {		
			pEnv->front_fps = 10; // CAMERA_FPS ???? ??? ???? : 1,2,3,5,6,10,15
			pEnv->rear_fps = 5;
			
 #if DEF_VIDEO_QUALITY_ONLY		
  #ifdef DEF_LOW_FRONT_720P
 			//pEnv->front_bitrate = 2 * BITRATE_720P + (BITRATE_720P / 2) + (200*1024); // * 3 = 15/5FPS 18M create
 			//pEnv->rear_bitrate = 2 * BITRATE_480I + (BITRATE_480I / 2);  
 			pEnv->front_bitrate = 3 * BITRATE_720P + (BITRATE_720P / 2);
 			pEnv->rear_bitrate = 2 * BITRATE_720P; //640x360 5FPS
   #ifdef DEF_REAR_CAMERA_ONLY
			pEnv->front_bitrate += (BITRATE_720P / 4);
   #endif
  #else
			pEnv->front_bitrate = 3 * BITRATE_480I + (BITRATE_480I / 2); //15/5FPS is 19M create
			pEnv->rear_bitrate = 3 * BITRATE_480I + (BITRATE_480I / 2);
			//pEnv->rear_bitrate = pEnv->front_bitrate = 2 * BITRATE_480I + (BITRATE_480I / 2);  //15/6FPS is 15.7M create		
  #endif
 #endif			
		}
		
#endif
#endif

//++{****************** HIGH 100M Test
#if 0 //DEF_VIDEO_QUALITY_ONLY
		if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
			pEnv->front_bitrate = MBYTES(9);
		}
#endif
//++}******************
 
		pEnv->bAudioRecEnable = l_sys_setup.cfg.iAudioRecEnable;

		pEnv->motionSensi = l_sys_setup.cfg.iEventMotionSensi;
			
		
		G_sensorParkingModeEnterTime(l_sys_setup.cfg.iAutoPmEnterTime);
#if DEF_SUB_MICOM_USE			
		l_sub_mcu.SetCarBatVoltageCalib(l_sys_setup.cfg.iCarBatVoltageCalib);
#endif

		if(is_parking_mode){
			if(l_sys_setup.cfg.iPmImpactSensi < 1)
				l_sys_setup.cfg.iPmImpactSensi = 1;
			else if(l_sys_setup.cfg.iPmImpactSensi > GSENSOR_TRIGGER_LEVEL_MAX)
				l_sys_setup.cfg.iPmImpactSensi = GSENSOR_TRIGGER_LEVEL_MAX;
			
			G_sensorSetTrgLevel((GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.iPmImpactSensi) * G_SENSOR_MIN_TRGLEVEL + G_SENSOR_MIN_TRGLEVEL, 0.0, 0.0); // 0.3 ~ 0.03g
	
			if(l_sys_setup.cfg.bPmMotionEnable){
				pEnv->motionSensi = l_sys_setup.cfg.iPmMotionSensi;
				pEnv->bMotionRecEnable = true;
				pEnv->RecordingMode = RECORD_Parking_Motion;
			}
			else {
				pEnv->bMotionRecEnable = false;
				pEnv->RecordingMode = RECORD_Parking_Timelapse;
			}
		}
		else {
			if(l_sys_setup.cfg.iRecordingMode == CFG_REC_MODE_TIMELAPSE){
				pEnv->RecordingMode = RECORD_TimelapseMode;
			}
			else if(l_sys_setup.cfg.iRecordingMode == CFG_REC_MODE_EVENT){
				pEnv->bMotionRecEnable = true;
				pEnv->RecordingMode = RECORD_EventMode;
			}
			else {
				pEnv->bMotionRecEnable = false;
				pEnv->RecordingMode = RECORD_NormalMode;
			}
				
			if(l_sys_setup.cfg.iGsensorSensi < 1)
				l_sys_setup.cfg.iGsensorSensi = 1;
			else if(l_sys_setup.cfg.iGsensorSensi > GSENSOR_TRIGGER_LEVEL_MAX)
				l_sys_setup.cfg.iGsensorSensi = GSENSOR_TRIGGER_LEVEL_MAX;
			
			G_sensorSetTrgLevel((GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.iGsensorSensi) * 0.1 + 0.45, \
				(GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.G_Sensi_B) * 0.1 + 0.45, \
				(GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.G_Sensi_C) * 0.1 + 0.45); // 1.05 ~ 0.15g ==> 20201223 1.35 ~ 0.45g
		}

#ifdef DEF_SAFE_DRIVING_MONITORING
 #ifdef DEF_OSAKAGAS_DATALOG
  //?????: 1????? 12km ??? ????
  //??? ???: 1?? ???? 12km ??? ????
  //????? : ???????????10=26 ???
 	safe_driving_control_set(l_sys_setup.cfg.iSuddenAccelerationSensi , \
			l_sys_setup.cfg.iSuddenDeaccelerationSensi, \
			l_sys_setup.cfg.iRapidRotationSensi,\
			DX_ROTATE_0);

 	safe_driving_speed_set(l_sys_setup.cfg.Overspeed, l_sys_setup.cfg.Fastspeed, l_sys_setup.cfg.Highspeed);
 #else
		// iSuddenAccelerationSensi 20 ~ 110
		//iRapidRotationSensi 15 ~ 55
		safe_driving_control_set((GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.iSuddenAccelerationSensi) * 8 + 15, \
			(GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.iSuddenDeaccelerationSensi) * 8 + 15, \
			(GSENSOR_TRIGGER_LEVEL_MAX - l_sys_setup.cfg.iRapidRotationSensi) * 5 + 15,\
			DX_ROTATE_0);
 #endif
#endif

 		if(pEnv->RecordingMode == RECORD_TimelapseMode || pEnv->RecordingMode == RECORD_Parking_Timelapse){
#if !DEF_30FIX_FRAME			
			pEnv->front_bitrate = pEnv->front_bitrate / (CFG_VIDEO_QUALITY_END - l_sys_setup.cfg.iVideoQuality);
			pEnv->rear_bitrate = pEnv->rear_bitrate / (CFG_VIDEO_QUALITY_END - l_sys_setup.cfg.iVideoQuality);
#endif			
			
			pEnv->rear_fps = pEnv->front_fps = TIMELAPSE_FPS;
			pEnv->rec_secs = RECORDING_DURATION_SECS * 30;
 		}

 #if ENABLE_BOARD_CHECK
 		pEnv->bMotionRecEnable = true;
 #endif

 #if DEF_SUB_MICOM_USE
 		if(l_sys_setup.cfg.iCarBatVoltage)
			l_sub_mcu.SetBatteryMode(l_sys_setup.cfg.iCarBatVoltage+1);
 #endif
 
		return true;
	}
	return false;
}

#ifdef DEF_PAI_R_DATA_SAVE
void recorder_pai_r_local_autoid_save(void)
{
	if(pai_r_datasaver_get_unique_local_autoid() != l_local_autoid){
		l_local_autoid = pai_r_datasaver_get_unique_local_autoid();
#if DEF_SUB_MICOM_USE	
		if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
			l_sub_mcu.SetEepromData(EEPORM_AUTO_ID, l_local_autoid);
		}
		else
#endif
		{
			FILE *fp = fopen("/data/local_autoid", "wb");
			if(fp){
				fwrite( (void *)&l_local_autoid, 1, sizeof(l_local_autoid), fp );
				fclose(fp);
			}
		}
	}
}

void recorder_pai_r_local_autoid_init(void)
{
	if(access(DA_FACTORY_DEFAULT_FILE_NAME, R_OK ) == 0) {
			l_local_autoid = 0;
#if DEF_SUB_MICOM_USE	
		if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
			l_sub_mcu.SetEepromData(EEPORM_AUTO_ID, l_local_autoid);
		}
		else
#endif	
		{
			system("rm /data/local_autoid");
		}
	}
	else			
 #if DEF_SUB_MICOM_USE
 	if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
		l_local_autoid = l_sub_mcu.GetEepromData(EEPORM_AUTO_ID);
		if(l_local_autoid == 0xffffffff)
			l_local_autoid = 0;
	}
	else
 #endif
	{
		FILE *fp = fopen("/data/local_autoid", "rb");
		if(fp){
			int ret = fread( (void *)&l_local_autoid, 1, sizeof(l_local_autoid), fp );
			if(ret != sizeof(l_local_autoid))
				l_local_autoid = 0;
			
			fclose(fp);
		}
	}
 	pai_r_datasaver_set_unique_local_autoid(l_local_autoid);
}
#endif // end of DEF_PAI_R_DATA_SAVE

#if DEF_SUB_MICOM_USE
int recorder_low_battery_check(void)
{
	#define LOW_BATTERY_ENABLE_TIME		30 //sec
	#define THRESHOLD_VOLTAGE		3 // 300mv
	
	static u32 low_battery_count = 0;
	
	int volt = (int)(l_sub_mcu.getBatteryVoltage() * 10);

	if(volt < LOW_BATTERY_MIN_VOLT){
		msleep(100);
		volt = (int)(l_sub_mcu.getBatteryVoltage() * 10);
		if(volt < LOW_BATTERY_MIN_VOLT){
			if(l_sd_exist == false || get_tick_count() < SAFETY_START_RECORDING_SECONDS)
				power_disconnected = true;
		}
	}

	if(l_sys_setup.cfg.bCarBatSafeEnable && l_is_parking_mode){
		int safe_volt = l_sys_setup.cfg.iCarBatSafeVoltage;

		if(l_sys_setup.cfg.iCarBatVoltage == 0) // auto
			safe_volt *= l_sub_mcu.m_i2c_reg.st.BATT_MODE; // 12V or 24V
		
		if(power_low_voltage == false && volt < safe_volt) {
			if( ++low_battery_count >= LOW_BATTERY_ENABLE_TIME){
				power_low_voltage = true;
				l_sub_mcu.SetLowVoltage(safe_volt/l_sub_mcu.m_i2c_reg.st.BATT_MODE + THRESHOLD_VOLTAGE);
				
				SYSTEMLOG(LOG_TYPE_BATTERY, LOG_EVENT_BATTCHANGE, eBB_BATT_STATE_LOW, "%d.%d V", volt / 10, volt % 10);

			
				//mixwav_play(kMixwaveRecordStop);
				postCommand(kStopRecording);
			}
		}
		else {
			low_battery_count = 0;
			
			if(power_low_voltage && volt > safe_volt + THRESHOLD_VOLTAGE){
				power_low_voltage = false;
				l_sub_mcu.SetLowVoltage(LOW_BATTERY_MIN_VOLT);
				SYSTEMLOG(LOG_TYPE_BATTERY, LOG_EVENT_BATTCHANGE, eBB_BATT_STATE_NORMAL, "%d.%d V", volt / 10, volt % 10);

				postCommand(kStartRecording);
			}
		}
	}

	return volt;
}

int recorder_power_off_check(void)
{
	if((power_low_voltage || power_disconnected) && l_is_recording == false ){
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_SYSEND ,0 , "%0.1f V", l_sub_mcu.getBatteryVoltage());
		
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		if(l_recorder_ref){
//			int i;
			destroyRecorder(l_recorder_ref);
			l_recorder_ref = nullptr;
		}
#endif		
#if ENABLE_DATA_LOG
		l_datalog.save_log(true);
#endif

		if(l_net_exist)
			pai_r_datasaver_send_msg_to_httpd(QMSG_EXIT, 0, 0, 0);
		
		//mixwav_play(kMixwave10SecBlocking);

		led_process_postCommand(LED_WORK_MODE, LED_WORK_POWER_OFF, LED_TIME_INFINITY);

#if DEF_DEBUG_SERIAL_LOG_FILE_SAVE //for serial log test
		cancel_handler(-9);
		return 0;
#endif

		l_sys_setup.cfg_backup();

#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
		{
			int i;
		
			for( i=0; i < FILE_PATH_BUFFER_COUNT; i++ ) {
				if(strlen(l_st_recording_file_path.path[i]))
					postCommand( kRecordingCompleted + i);
			}
			for( i=0; i < FILE_PATH_BUFFER_COUNT; i++ ) {
				if(strlen(l_st_motion_recording_file_path.path[i]))
					postCommand( kRecordingCompletedMotion + i);
			}
		}
#endif
		
//		if(l_sd_exist && get_tick_count() > SAFETY_START_RECORDING_SECONDS){
//				mixwav_system_aplay(kMixwaveRecordStop);
//		}
		
#ifdef DEF_PAI_R_DATA_SAVE
		if(l_net_exist){
			while(pai_r_datasaver_data_list_size())
			{
				printf("%s(): %d \r\n", __func__,  pai_r_datasaver_data_list_size());
				msleep(250);
			}
		}
		recorder_pai_r_local_autoid_save();
#endif

		while(SYSTEMLOG_IS_SYSBOOT())	msleep(100);
		
		//sleep(2);
		if(power_low_voltage)
			l_sub_mcu.SetPowerMode(BB_MICOM_POWER_OFF_SLEEP);
		else
			l_sub_mcu.SetPowerMode(BB_MICOM_POWER_OFF_FALL);


		power_low_voltage =  power_disconnected = true;
		sleep(3);
		timer_running_ = false;
	}

	return 0;
}

#endif

int recorder_parking_check(void)
{
	if(l_sys_setup.cfg.bAutoPmEnable){
		bool mode = G_sensorGetParkingMode();
		
		if(mode != l_is_parking_mode){
			l_is_parking_mode = mode;

			recorder_env_setup(&l_rec_env, mode);
			rear_disabled_ = rear_camera_detected; // restart recording
		}
	}

	return 0;
}

int read_backup_pid(const char *fname)
{
	long int pid;
	FILE *fp;

	fp = fopen(fname, "r");

	fscanf(fp, "%ld", &pid);

	fclose(fp);

	return pid;
}

void kill_surveillant_app(void)
{
	int pid;
	const char * pid_file = "tmp/mnt/surveillant.pid";
	
	if ( access(pid_file, R_OK ) == 0){
		pid = read_backup_pid(pid_file);
		if(pid){
			char cmd[64];
			sprintf(cmd, "kill -9 %d", pid);
			DLOG0(DLOG_INFO, "surveillant exit. %s\r\n", cmd);
			system(cmd);
		}
	}
}

static void recorder_env_process(void)
{
	bool is_recording_start = false;
	u32 l_sd_error_tm_ms = 0;
	u32 l_sd_check_tm_ms = 0;
	u32 retry = 0;
	u32 error_count = 0;
	u32 duration_time_error_count = 0;
	int64_t durationUs = 0;
	bool is_sd_error = false;

//
#ifdef TARGET_CPU_V536
	if (get_tick_count() > 130000 && get_tick_count() < 170000) 
#else
	if (get_tick_count() > 75000 && get_tick_count() < 90000) 
#endif		
	{
		DLOG0(DLOG_WARN, "firmware update complete!\r\n");
		recorder_reboot();
	}
//

	SYSTEMLOG_START();
	
	SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_SYSBOOT ,0 , "%s [%s]", DA_FIRMWARE_VERSION, __TIME__);

	
#if DEF_SUB_MICOM_USE	
	SYSTEMLOG(LOG_TYPE_BATTERY, LOG_EVENT_BATTCHANGE, eBB_BATT_STATE_NORMAL, "%0.1f V", l_sub_mcu.getBatteryVoltage());
	SYSTEMLOG(LOG_TYPE_TEMPERATURE, LOG_EVENT_TEMPCHANGE, eBB_TEMP_STATE_NORMAL, "%0.1f C", l_sub_mcu.getTemp());
#endif

	SB_MakeStartTime();
	mixwav_set_volume(24);//test_update, SD noinsert speaker x _add
	mixwav_start();
	
	if(recorder_fw_update_complete_check()){
		mixwav_system_aplay(kMixwaveUpdateEnd);
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_UPDATE, _LOG_FIRMWARE_FILE, "END");
		
#if 0			
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_UPDATE, _LOG_FIRMWARE_FILE, "REBOOT");
		recorder_reboot();
		timer_running_ = false;
		goto exit_env_process;
#endif				
	}

	if (recorder_is_sd_exist()) {
		l_sd_exist = true;
	}
	else {
		SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_SD_CD ,0 , "NO SD");
	}

	evdev_gsensor_start();
	
	do {

		if(is_time_out_ms(&l_sd_check_tm_ms, 1000)){
			
			if (recorder_is_sd_exist()) {
				if(l_sd_exist == false){
					SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_SD_CD ,0 , "DETECTED");

					sleep(1);
					
					if(recorder_fw_update_file_check()){
						break;
					}
					//system("fsck.offs /dev/mmcblk0p1"); //check
					//system("offs.fuse /mnt/extsd");
					SYSTEMLOG(LOG_TYPE_ERROR, LOG_ERROR_SD_DISK_INIT_FAIL ,0 , "REBOOT");
					
					recorder_reboot();
					timer_running_ = false;
					break;
				}
				
		       l_sd_exist = true;
				is_sd_error = false;
		    }
			else {

				if(l_sd_exist){
					SYSTEMLOG(LOG_TYPE_ERROR, LOG_EVENT_SD_CD ,0 , "EJECTED");
				}
				
				l_sd_exist = false;
				
				if(is_recording_start){
					is_recording_start = false;
					is_sd_error = true;
					//postCommand(kStopRecording);
					//postCommand(kStopEventRecording);
					
					system("umount /mnt/extsd");
					msleep(1);
					//if(oasis::recorderIsRunning(l_recorder_ref))
					//	mixwav_play(kMixwaveRecordStop);

					//datool_led_red_set(is_recording_start);
					led_process_postCommand(LED_WORK_MODE, LED_WORK_SD_ERROR, LED_TIME_INFINITY);
				}
				
				if(is_time_out_ms(&l_sd_error_tm_ms, 5000)){
					if(error_count == 0)
						led_process_postCommand(LED_WORK_MODE, LED_WORK_SD_ERROR, LED_TIME_INFINITY);

					if(error_count++ < 3 || is_sd_error ) { //??? ?????(3? ???_5s ???), ??? ?? ???? (??????_5s ???)
						mixwav_play(kMixwaveSDError);
					}
					else {
						led_process_postCommand(LED_WORK_MODE, LED_WORK_LED_OFF, LED_TIME_INFINITY);
					}
				}
			}

			//.RTC -> ?y??? ?ð?
			//$ hwclock --hctosys
			//?y??? ?ð? -> RTC
			//$ hwclock --systohc
#if DEF_SUB_MICOM_USE			
			recorder_low_battery_check();
#endif
#ifdef DEF_PAI_R_DATA_SAVE
			recorder_pai_r_local_autoid_save();
#endif

			if(l_sd_exist && is_recording_start == true)
				recorder_parking_check();


			if(l_sd_exist && is_recording_start && get_tick_count() > SAFETY_START_RECORDING_SECONDS){
				if(l_durationUs == durationUs){
#ifdef DEF_PAI_R_DATA_SAVE					
					if(l_net_exist ){
						if(pai_r_datasaver_is_ready())
							duration_time_error_count++;
					}
					else 
#endif
					{
						duration_time_error_count++;
					}
					
					if(duration_time_error_count > 30 && l_normal_recording_type != ' '){
						//kill_surveillant_app();
						//cancel_handler(-9);
						
						DLOG(DLOG_ERROR, "recording duration time error!\r\n");
						SYSTEMLOG(LOG_TYPE_ERROR, LOG_ERROR_INTERNAL_ERR ,0 , "duration time error!");
						postCommand(kStopRecording);
						sleep(1);
						is_recording_start = false;
						duration_time_error_count = 0;
						postCommand(kStartRecording);//OnOff Test_230420
					}
				}
				else {
					duration_time_error_count = 0;
					durationUs = l_durationUs;
				}
			}
		}
		
#if DEF_SUB_MICOM_USE	
		recorder_power_off_check();
#endif

		if(l_sd_exist && is_recording_start == false)		
		{

			if(recorder_fw_update_file_check()){
				break;
			}
	
			if(!l_sys_setup.Init()){
				if(retry++ >= 1){
					SYSTEMLOG(LOG_TYPE_ERROR, LOG_ERROR_SD_DISK_INIT_FAIL ,0 , "REBOOT");

					recorder_reboot();
				}
			}
			else{	
#if 1
				float dOffsVer;
				int iResult = -1;
				
				iResult = system("fsck.offs -x /dev/mmcblk0p1");
/* 
/tmp/offs.info

version=3.13
result=0
mode=driving
folders=2
folder1=NORMAL
folder1_size=16
folder1_percent=85
folder1_files=3235
folder2=EVENT
folder2_size=8
folder2_percent=10
folder2_files=762

*/
				//sysfs_scanf("/tmp/offs.info", "version=%f\nresult=%d", &dOffsVer, &iResult);
				DLOG(DLOG_INFO, "OFFS result=%d\r\n", iResult);
				//if(iResult != 0) {
				if(WEXITSTATUS(iResult) != 0) {
					do {
						if(is_time_out_ms(&l_sd_error_tm_ms, 5000)){
							if(error_count == 0)
								led_process_postCommand(LED_WORK_MODE, LED_WORK_SD_ERROR, LED_TIME_INFINITY);

							if(error_count++ < 5 || is_sd_error ) { //??? ?????(5? ???_5s ???), ??? ?? ???? (??????_5s ???)
								mixwav_play(kMixwaveSDError);
							}
							else {
								led_process_postCommand(LED_WORK_MODE, LED_WORK_LED_OFF, LED_TIME_INFINITY);
							}
							
							if(!recorder_is_sd_exist())
								continue;
						}
						msleep(50);
#if DEF_SUB_MICOM_USE	
						recorder_power_off_check();
#endif
					}while(timer_running_);
					continue;
				}
#else 
				system("fsck.offs /dev/mmcblk0p1");
				system("offs.fuse /mnt/extsd");
#endif	

				is_recording_start = true;
				error_count = 0;
				
				RecorderPostCommand(kStopRecording);
				RecorderPostCommand(kStartRecording);
	
				if(auto_recording_event_) {
					sleep(1);
					postCommand(kStartEventRecording);
				}

			}
		}
		
		msleep(50);
	} while(timer_running_);

exit_env_process :

	SYSTEMLOG_STOP();
		

	if(is_recording_start){
		mixwav_play(kMixwaveRecord_wa);
		mixwav_play(kMixwaveRecordStop);
	}
	
	postCommand(kRecorderExitNow);
	
	l_sys_setup.Deinit();

	mixwav_stop();
		
}


void snapshot_parameters_set(oasis::key_value_map_t &parameters, int resolution)
{
	parameters.clear();
	//parameters["width"] = "0"; //full-width
	//parameters["height"] = "0"; //full-height
	if(resolution == CFG_RESOLUTION_480I){
		parameters["width"] = "640";			// 480 fix for recorder_camera_check
		parameters["height"] = VGA_HEIGH_RESOLUTION;
		parameters["quality"] = "80";
	}
	else {
		parameters["width"] = "480";
		parameters["height"] = "270";
		parameters["quality"] = "80";
	}
		
	parameters["timeout"] = "3000"; //wait timeout max in msec
}

int camera_parameters_set(oasis::key_value_map_t &parameters, int32_t camera1_id, int32_t camera2_id, int32_t rotation, std::string &ext, REC_ENV &rec_env, std::string &res_front, std::string &res_rear, bool audio_enable, const char * ini_path)
{
	int32_t err;
	int32_t i, c, n;
	char key[1024], value[1024], section[1024], key2[1024];
	struct parameter_info *param;

	int32_t camera_id;
	static std::list<int32_t> camera_composer_ids;
	
	err = oasis::get_profile_string("sources", "source-count", nullptr, value, sizeof(value), ini_path);
	if(err < 0) {
		DLOG0(DLOG_ERROR, "[sources] source-count not found!\n");
		abort_main();
		return -1;
	}

	parameters["source-count"] = value;
	c = atoi(value);
	if(c == 0) {
		DLOG(DLOG_ERROR, "no sources!\n");
		abort_main();
		return -1;
	}

	for(i=1; i<=c; i++) {
		param = source_parameter_list_;
		for(; param->name != nullptr; param++) {
			if(param->noprefix) {
				strcpy(key, param->name);
			} else {
				sprintf(key, param->name, i);
			}
			err = oasis::get_profile_string("sources", key, nullptr, value, sizeof(value), ini_path);
			if(err < 0 && param->optional == false) {
				DLOG0(DLOG_ERROR, "[sources] %s not found!\n", key);
				abort_main();
				return -1;
			} else if(err > 0) {
				DLOG(DLOG_DEBUG, "[sources]       %s=%s\n", key, value);		
				if(strstr(key, "-camera-id") != NULL) {
					camera_id = atoi(value);
					if(camera_id >= 500 && camera_id < 1000) {
						camera_composer_ids.push_back(camera_id);
					}
				}				
				parameters[key] = value;
			}
		}
	}

	for(int32_t camera_composer_id : camera_composer_ids) {
		// network camera id >= 1000
		DLOG0(DLOG_INFO, "get camera composer<%d> settings\n", camera_composer_id);
		sprintf(section, "camera-composer-%d", camera_composer_id);

		err = oasis::get_profile_string(section, "component-count", nullptr, value, sizeof(value), ini_path);
		if(err < 0) {
			DLOG0(DLOG_ERROR, "[%s] component-count not found!\n", section);
			return -1;
		}
		c = atoi(value);
		if(c == 0) {
			DLOG(DLOG_ERROR, "no components!\n");
			return -1;
		}

		sprintf(key, "%s-component-count", section);
		camera_composer_parameters[key] = value;

		err = oasis::get_profile_string(section, "master-component", nullptr, value, sizeof(value), ini_path);
		if(err < 0) {
			DLOG0(DLOG_ERROR, "[%s] master-component not found!\n", section);
			return -1;
		}
		n = atoi(value);
		if(n <= 0 || n > c) {
			DLOG0(DLOG_ERROR, "[%s] master-component invalid: %d\n", n);
			return -1;
		}

		sprintf(key, "%s-master-component", section);
		camera_composer_parameters[key] = std::to_string(n);

		for(i=1; i<=c; i++) {
			param = camera_composer_parameter_list;
			for(; param->name != nullptr; param++) {
				if(param->noprefix) {
					strcpy(key, param->name);
				} else {
					sprintf(key, param->name, i);
				}
				err = oasis::get_profile_string(section, key, nullptr, value, sizeof(value), ini_path);
				if(err < 0 && param->optional == false) {
					DLOG0(DLOG_ERROR, "[%s] %s not found!\n", section, key);
					return -1;
				} else if(err > 0) {
					DLOG(DLOG_DEBUG, "[%s]       %s=%s\n", section, key, value);
					sprintf(key2, "%s-%s", section, key);
					camera_composer_parameters[key2] = value;
				}
			}
		}
	}

	if(camera_composer_ids.empty() == false) {
		parameters.merge(camera_composer_parameters);
	}

	//oasis::dumpParameters("config", parameters);

#ifdef DEF_REAR_CAMERA_ONLY
	if((rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_720P){
		parameters["source1-capture-width"] = "1280";
		parameters["source1-capture-hight"] = "720";

		parameters["source2-capture-width"] = "1280";
		parameters["source2-capture-hight"] = "720";
	}
	else if((rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_1080P){
		parameters["source1-capture-width"] = "1920";
		parameters["source1-capture-hight"] = "1080";
		
		parameters["source2-capture-width"] = "1280";
		parameters["source2-capture-hight"] = "720";
	}
	else{
		parameters["source1-capture-width"] = "1920"; //tp2825-sepcific
		parameters["source2-capture-width"] = "1920"; //tp2825-sepcific
		
		parameters["source1-capture-height"] = "480";
		parameters["source2-capture-height"] = "480";
		
		parameters["source1-capture-interlaced"] = "1";
		parameters["source2-capture-interlaced"] = "1";
	}
#endif 

	if(rear_disabled_ == false) 	
	{
			if((rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_720P){
				parameters["source3-capture-width"] = "1280";
				parameters["source3-capture-hight"] = "720";

				parameters["source4-capture-width"] = "1280";
				parameters["source4-capture-hight"] = "720";
			}
			else if((rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_1080P){
				parameters["source3-capture-width"] = "1920";
				parameters["source3-capture-hight"] = "1080";

				parameters["source4-capture-width"] = "1920";
				parameters["source4-capture-hight"] = "1080";
			}
			else{
				parameters["source3-capture-width"] = "1920"; //tp2825-sepcific
				parameters["source4-capture-width"] = "1920"; //tp2825-sepcific

				parameters["source3-capture-height"] = "480";
				parameters["source4-capture-height"] = "480";
				
				parameters["source3-capture-interlaced"] = "1";
				parameters["source4-capture-interlaced"] = "1";
			}
	}

	return 0;
}

int preview_parameters_set(oasis::key_value_map_t &preview_parameters, int32_t camera1_id, int32_t camera2_id, int32_t camera3_id, int32_t rotation, std::string &ext, REC_ENV &rec_env, std::string &res_front, std::string &res_rear, bool audio_enable, const char * ini_path)
{
	int32_t err, i, c;
	char key[1024], value[1024];
	struct parameter_info *param;

	struct parameter_info preview_parameter_list_[] = {
		{ "rotation", true, false },
		{ "channel-count", true, false },
		{ "primary_channel", true, false },
		{ nullptr, false, false }
	};

	struct parameter_info preview_channel_parameter_list_[] = {
		{ "channel%d-camera-id", false, false },
		{ "channel%d-ise-id", false, true },
		{ "channel%d-visible", false, true },
		{ "channel%d-rotation", false, true },
		{ "channel%d-x", false, true },
		{ "channel%d-y", false, true },
		{ "channel%d-width", false, true },
		{ "channel%d-height", false, true },
		{ "channel%d-horz-pos", false, true },
		{ "channel%d-vert-pos", false, true },
		{ nullptr, false, false }
	};

	preview_parameters.clear();
////////////////////////////////////////////////////////////////////////////////////////////
	//preview settings
	param = preview_parameter_list_;
	for(i = 1; param->name != nullptr; param++) {
		if(param->noprefix) {
			strcpy(key, param->name);
		} else {
			sprintf(key, param->name, i);
		}
		err = oasis::get_profile_string("preview", key, nullptr, value, sizeof(value), ini_path);
		if(err < 0 && param->optional == false) {
			DLOG0(DLOG_ERROR, "[preview] %s not found!\n", key);
			abort_main();
			return -1;
		} else if(err > 0) {
			DLOG(DLOG_DEBUG, "[preview]       %s=%s\n", key, value);
			preview_parameters[key] = value;
		}
	}

	ASSERT(preview_parameters.find("channel-count") != preview_parameters.end());
	c = atoi(preview_parameters["channel-count"].c_str());
	if(c == 0) {
		DLOG(DLOG_WARN, "no preview channels!\n");
	} else {
		l_has_preview = true;

		//verify primary channel
		ASSERT(preview_parameters.find("primary_channel") != preview_parameters.end());
		i = atoi(preview_parameters["primary_channel"].c_str());
		if(i <= 0 || i > c) {
			DLOG(DLOG_ERROR, "wrong primary channel!\n");
			abort_main();
			return -1;
		}

		for(i=1; i<=c; i++) {
			param = preview_channel_parameter_list_;
			for(; param->name != nullptr; param++) {
				if(param->noprefix) {
					strcpy(key, param->name);
				} else {
					sprintf(key, param->name, i);
				}
				err = oasis::get_profile_string("preview", key, nullptr, value, sizeof(value), ini_path);
				if(err < 0 && param->optional == false) {
					DLOG0(DLOG_ERROR, "[preview] %s not found!\n", key);
					abort_main();
					return -1;
				} else if(err > 0) {
					DLOG(DLOG_DEBUG, "[preview]       %s=%s\n", key, value);
					preview_parameters[key] = value;
				}
			}
		}

		if(camera_composer_parameters.empty() == false /*&& camera_composer_id != -1*/) {
			preview_parameters.merge(camera_composer_parameters);
		}
	}

	return 0;
}

int adas_parameters_set(oasis::key_value_map_t &adas_parameters, int32_t camera1_id, int32_t camera2_id, int32_t rotation, std::string &ext, REC_ENV &rec_env, std::string &res_front, std::string &res_rear, bool audio_enable, const char * ini_path)
{
//	int32_t fps;
	int32_t err, i, c;
	char key[1024], value[1024];
	struct parameter_info *param;

	 struct parameter_info adas_parameter_list_[] = {
		{ "channel-count", true, false },
		{ "library-path", true, false },
		{ nullptr, false, false }
	};

	struct prefixed_parameter_info adas_channel_parameter_list_[] = {
		{ "channel%d-camera-id", "camera-id", false },
		{ "channel%d-enable", "enable", true },
		{ "channel%d-buffer-count", "buffer-count", true },
		{ "channel%d-gps-enabled", "gps-enabled", true },
		{ "channel%d-gsensor-enabled", "gsensor-enabled", true },
		{ "channel%d-force-default-view-angle", "force-default-view-angle", true },
		{ "channel%d-default-vert-view-angle", "default-vert-view-angle", true },
		{ "channel%d-collision-avoidance-sensitivity", "collision-avoidance-sensitivity", true },
		{ "channel%d-lane-departure-sensitivity", "lane-departure-sensitivity", true },
		{ "channel%d-fps", "fps", true },
		{ "channel%d-use-g2d", "use-g2d", true },
		{ "channel%d-big-width", "big-width", true },
		{ "channel%d-big-height", "big-height", true },
		{ "channel%d-small-width", "small-width", true },
		{ "channel%d-small-height", "small-height", true },
		{ nullptr, nullptr, false }
	};

	std::shared_ptr<MyAdasObserver> adas_observer = std::make_shared<MyAdasObserver>();	
	
	adas_parameters.clear();
	err = oasis::get_profile_string("adas", "channel-count", nullptr, value, sizeof(value), ini_path);
	if(err < 0) {
		DLOG0(DLOG_ERROR, "[adas] channel-count not found!\n");
		return -1;
	}

	c = atoi(value);
	if(c > 0) {

		//only one.
		c = 1;

		std::string library_path;
		err = oasis::get_profile_string("adas", "library-path", nullptr, value, sizeof(value), ini_path);
		if(err <= 0) {
			DLOG0(DLOG_ERROR, "[adas] library-path not found!\n");
			return -1;
		}
		if(access(value, F_OK) != 0) {
			DLOG0(DLOG_ERROR, "[adas] library-path file not found: %s\n", value);
			return -1;
		}

		library_path = value;
		struct prefixed_parameter_info *param;

		for(i=1; i<=c; i++) {

			adas_parameters.clear();
			adas_parameters["library-path"] = library_path;

			param = adas_channel_parameter_list_;

			for(; param->prefixed_name != nullptr; param++) {
				sprintf(key, param->prefixed_name, i);
				err = oasis::get_profile_string("adas", key, nullptr, value, sizeof(value), ini_path);
				if(err < 0 && param->optional == false) {
					DLOG0(DLOG_ERROR, "[adas] %s not found!\n", key);
					return -1;
				} else if(err > 0) {		
					DLOG(DLOG_DEBUG, "[adas]       %s=%s\n", key, value);
					adas_parameters[param->parameter_name] = value;
				}
			}
#if ENABLE_ADAS
			adas = createAdas(adas_parameters);
#endif
		}

	} else {
		DLOG(DLOG_WARN, "no adas channels!\n");
	}

	if(adas) {
		adasStart(adas, adas_observer);
	}
}

int motion_parameters_set(oasis::key_value_map_t &md_parameters, int32_t camera1_id, int32_t camera2_id, int32_t rotation, std::string &ext, REC_ENV &rec_env, std::string &res_front, std::string &res_rear, bool audio_enable, const char * ini_path)
{
//	int32_t fps;
	int32_t err, i, c;
	char key[1024], value[1024];
	struct parameter_info *param;

	struct parameter_info motion_parameter_list_[] = {
		{ "channel-count", true, false },
		{ nullptr, false, false }
	};

	struct prefixed_parameter_info motion_channel_parameter_list_[] = {
		{ "channel%d-camera-id", "camera-id", false },
		{ "channel%d-enable", "enable", true },
		{ "channel%d-sensitivity-level", "sensitivity-level", true },
		{ "channel%d-buffer-count", "buffer-count", true },
		{ "channel%d-fps", "fps", true },
		{ "channel%d-use-g2d", "use-g2d", true },
		{ "channel%d-version", "version", true },
		{ "channel%d-height-max", "height-max", true },
		{ nullptr, nullptr, false }
	};

	std::shared_ptr<MyMotionDetectObserver> motion_detect_observer = std::make_shared<MyMotionDetectObserver>();	
	DLOG0(DLOG_ERROR, "================> 1\n");
	md_parameters.clear();

	//preview settings
	//# not used
#if ENABLE_MD	
	if(rec_env.bMotionRecEnable)
		md_parameters["channel-count"] = std::to_string(3);
	else
		md_parameters["channel-count"] = "0";
#else
	md_parameters["channel-count"] = "0";
#endif

	err = oasis::get_profile_string("motion", "channel-count", nullptr, value, sizeof(value), ini_path);
	if(err < 0) {
		DLOG0(DLOG_ERROR, "[motion] channel-count not found!\n");
		return -1;
	}

	c = atoi(value);
	if(c > 0) {

		struct prefixed_parameter_info *param;

		for(i=1; i<=c; i++) {

			md_parameters.clear();

			param = motion_channel_parameter_list_;

			for(; param->prefixed_name != nullptr; param++) {
				sprintf(key, param->prefixed_name, i);
				err = oasis::get_profile_string("motion", key, nullptr, value, sizeof(value), ini_path);
				if(err < 0 && param->optional == false) {
					DLOG0(DLOG_ERROR, "[motion] %s not found!\n", key);
					return -1;
				} else if(err > 0) {		
					DLOG(DLOG_DEBUG, "[motion]       %s=%s\n", key, value);
					md_parameters[param->parameter_name] = value;
				}
			}
			//oasis::dumpParameters("config", parameters);
#if ENABLE_MD	
			if(rec_env.bMotionRecEnable){
				motion_detector = createMotionDetector(md_parameters);
				if(motion_detector) {
					motion_detectors.push_back(motion_detector);
				}
			}
#endif			
		}

	} else {
		DLOG(DLOG_WARN, "no motion detector channels!\n");
	}
DLOG0(DLOG_ERROR, "================> 2\n");
	for(const MotionDetectorRef &motion_detector : motion_detectors) {
		err = motionDetectorStart(motion_detector, motion_detect_observer);
		if(err < 0) {
			DLOG0(DLOG_ERROR, "MotionDetectgor start failed\n");
		} else {
			TRACE0("MotionDetector<%d> started\n", motionDetectorId(motion_detector));
		}
	}
	DLOG0(DLOG_ERROR, "================> 3\n");
}

int recorder_parameters_set(oasis::key_value_map_t &parameters, int32_t camera1_id, int32_t camera2_id, int32_t camera3_id, int32_t rotation, std::string &ext, REC_ENV &rec_env, std::string &res_front, std::string &res_rear, bool audio_enable, const char * ini_path)
{
	int32_t fps = rec_env.front_fps;
	int32_t err, i, c;
	char key[1024], value[1024];
	struct parameter_info *param;

	parameters.clear();

	////////////////////////////////////////////////////////////////////////////////////////////
	//recorer settings

	param = recorder_parameter_list_;
	for(i = 1; param->name != nullptr; param++) {
		if(param->noprefix) {
			strcpy(key, param->name);
		} else {
			sprintf(key, param->name, i);
		}
		err = oasis::get_profile_string("recorder", key, nullptr, value, sizeof(value), ini_path);
		if(err < 0 && param->optional == false) {
			DLOG0(DLOG_ERROR, "[recorder] %s not found!\n", key);
			abort_main();
			return -1;
		} else if(err > 0) {
			DLOG(DLOG_DEBUG, "[recorder]       %s=%s\n", key, value);
			parameters[key] = value;
		}
	}

	err = oasis::get_profile_string("recorder", "channel-count", nullptr, value, sizeof(value), ini_path);
	if(err < 0) {
		DLOG0(DLOG_ERROR, "[recorder] channel-count not found!\n");
		abort_main();
		return -1;
	}

	sprintf(value, "%d", l_recorder_camera_counts);//

	parameters["channel-count"] = value;
	
	c = atoi(value);
	if(c == 0) {
		DLOG(DLOG_ERROR, "no recorder channels!\n");
		abort_main();
		return -1;
	}

	for(i=1; i<=c; i++) {
		param = recorder_channel_parameter_list_;
		for(; param->name != nullptr; param++) {
			if(param->noprefix) {
				strcpy(key, param->name);
			} else {
				sprintf(key, param->name, i);
			}
			err = oasis::get_profile_string("recorder", key, nullptr, value, sizeof(value), ini_path);
			if(err < 0 && param->optional == false) {
				DLOG0(DLOG_ERROR, "[recorder] %s not found!\n", key);
				abort_main();
				return -1;
			} else if(err > 0) {		
				DLOG(DLOG_DEBUG, "[recorder]       %s=%s\n", key, value);
				parameters[key] = value;
			}
		}
	}

	ext = parameters["file-extension"];
	if(strcasecmp(ext.c_str(), "avi") && strcasecmp(ext.c_str(), "mp4")) {
		DLOG(DLOG_ERROR, "file extension avi or mp4 supported only\n");
		ext = "avi";
	}

	DLOG(DLOG_WARN, "file-prefix [%s] \n", parameters["file-prefix"].c_str());
	DLOG(DLOG_WARN, "file-extension [%s] \n", parameters["file-extension"].c_str());
	
	if(camera_composer_parameters.empty() == false /*&& camera_composer_id != -1*/) {
		parameters.merge(camera_composer_parameters);
	}
////////////////////////////////////////////////////////////////////////////////////////////////

	//*******************************************************************************

	//recorer settings
	//parameters["filedir"] = "/mnt/extsd/Videos";
#if 1	//if 0 => Track 4, 5 
	//++ 2020/06/14
	parameters["save-stream-data-in-header"] = "1"; 

	//++ 2020/09/21
	parameters["skip-v1-stream-data-in-movi-chunk"] = "1";
#endif

	//++ 2020/12/14
#if ENABLE_AVI_SCRAMBLING	
	parameters["enable-scramble"] = std::to_string(ENABLE_AVI_SCRAMBLING);
	parameters["scramble-codes"] = AVI_SCRAMBLE_CODES;
#endif

	//parameters["disable-offs-recording"] = std::to_string(DISABLE_OFFS_RECORDING);

	//parameters["recording-file-header-write-interval-secs"] = std::to_string(HEADER_WRITE_INTERVAL);
	
	//meta data size: 100-msec interval for 60-second media file, 9600 bytes
	if(strcasecmp(ext.c_str(), "avi") == 0) {
		parameters["avi-strd-size-max"] = std::to_string(calcMetaDataSize(20, rec_env.rec_secs, 16));
	} else if(strcasecmp(ext.c_str(), "mp4") == 0) {
		parameters["mp4-udta-size-max"] = std::to_string(calcMetaDataSize(20, rec_env.rec_secs, 16));
	}

	//*****************************************************************************************
	//*****************************************************************************************
	//*****************************************************************************************
#if 1
	parameters["file-duration-secs"] = std::to_string(rec_env.rec_secs);

	//parameters["enable-persistent-cache"] = "1"; //enable

	//front camera settings

#ifndef DEF_REAR_CAMERA_ONLY
	if(rotation == 0 && G_sensorGetControl()->rotate == DX_ROTATE_180){
		parameters["channel1-hflip"] = "1";
		parameters["channel1-vflip"] = "1";
	}
#endif

	if(rec_env.front_resolution == CFG_RESOLUTION_480I){
		parameters["channel1-out-width"] = "640";
		parameters["channel1-out-height"] = VGA_HEIGH_RESOLUTION;
	}
	else
		parameters["channel1-resolution"] = res_front.c_str();

	parameters["channel1-bitrate"] = std::to_string(rec_env.front_bitrate);

	fps = rec_env.front_fps;
	parameters["channel1-fps"] = std::to_string(CAMERA_FPS);
	
	if(fps != CAMERA_FPS){
		parameters["channel1-h264-framerate"] = std::to_string(fps);

		if(fps == TIMELAPSE_FPS)
			fps = AVI_FPS;	//timelapse mode
		
		parameters["channel1-avi-framerate"] = std::to_string(fps);
	}
	//if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH)		////////////////////////test_화질 X
	parameters["channel1-h264-keyframe-interval"] = std::to_string(fps); //"30";
	//else 
	//	parameters["channel1-h264-keyframe-interval"] = std::to_string(5);

#if 0	//test
	if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
		parameters["channel1-h264-min-qp"] = "27";
		parameters["channel1-h264-max-qp"] = "35";	//oasis 4.2.8 31=>42
		parameters["channel1-h264-enable-fixqp"] = "0";
		parameters["channel1-h264-fix-iqp"] = "20";
		parameters["channel1-h264-fix-pqp"] = "30";
	}
#endif
#ifndef TARGET_CPU_V536
 #if DEF_VIDEO_FIX_BITRATE
		parameters["channel1-h264-enable-fixqp"] = "1";
	//parameters["channel1-h264-fix-iqp"] = "20";
	//parameters["channel1-h264-fix-pqp"] = "35";
	parameters["channel1-h264-fix-iqp"] = std::to_string(rec_env.fix_iqp);	//"20";
	parameters["channel1-h264-fix-pqp"] = std::to_string(rec_env.fix_pqp);	//"38";
 #else
	parameters["channel1-h264-min-qp"] = "27";
	parameters["channel1-h264-max-qp"] = "35";	//oasis 4.2.8 31=>42
	parameters["channel1-h264-enable-fixqp"] = "0";
	parameters["channel1-h264-fix-iqp"] = "10";
	parameters["channel1-h264-fix-pqp"] = "20";
 #endif
#endif

	if(camera2_id >= 0 || camera3_id >= 0) {
		i = 2;
		if(camera2_id == -1)
			parameters["channel2-camera-id"] = std::to_string(camera3_id);
		
		for(; i<=c; i++) {
			//rear camera settings
			//parameters["rear-hflip"] = "1";
			//parameters["rear-vflip"] = "1";

			//DLOG0(DLOG_WARN, "rear_resolution : %d (hw_config:0x%x)\r\n", rec_env.rear_resolution, rec_env.hw_config);
			
			if(rec_env.rear_resolution == CFG_RESOLUTION_480I){
				parameters[format("channel%d-out-width", i)] = "640";
				parameters[format("channel%d-out-height", i)] = VGA_HEIGH_RESOLUTION;
				
				DLOG0(DLOG_WARN, "channel%d-out : 640x%s\r\n", i, VGA_HEIGH_RESOLUTION);
			}
			else {
				parameters[format("channel%d-resolution", i)] = res_rear.c_str();
				DLOG0(DLOG_WARN, "channel%d-resolution : %s\r\n", i, res_rear.c_str());
			}

			parameters[format("channel%d-bitrate", i)] = std::to_string(rec_env.rear_bitrate);

			parameters[format("channel%d-fps", i)] = std::to_string(CAMERA_FPS);

			fps = rec_env.rear_fps;
			
			if(fps != CAMERA_FPS)
			{
				parameters[format("channel%d-h264-framerate", i)] = std::to_string(fps);

				if(fps == TIMELAPSE_FPS)
					fps = AVI_FPS;	//timelapse mode
					
				parameters[format("channel%d-avi-framerate", i)] = std::to_string(fps);
			}

			//if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH)		////////////////////////test_화질 X
			parameters[format("channel%d-h264-keyframe-interval", i)] = std::to_string(fps); //"30";
			//else if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_MIDDLE)
			//	parameters[format("channel%d-h264-keyframe-interval", i)] = std::to_string(5);
			//else
			//	parameters[format("channel%d-h264-keyframe-interval", i)] = std::to_string(3);

#if 0	//test
			if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
				parameters[format("channel%d-h264-min-qp", i)] = "27";
				parameters[format("channel%d-h264-max-qp", i)] = "35";	//oasis 4.2.8 31=>42
				parameters[format("channel%d-h264-enable-fixqp", i)] = "0";
				parameters[format("channel%d-h264-fix-iqp", i)] = "25";
				parameters[format("channel%d-h264-fix-pqp", i)] = "30";
			}
#endif
			
#ifndef TARGET_CPU_V536			
 #if DEF_VIDEO_FIX_BITRATE		
			parameters[format("channel%d-h264-enable-fixqp", i)] = "1";
			//parameters[format("channel%d-h264-fix-iqp", i)] = "20";
			//parameters[format("channel%d-h264-fix-pqp", i)] = "35";  //20 File

			parameters[format("channel%d-h264-fix-iqp", i)] = std::to_string(rec_env.fix_iqp + 1); //"25";
			parameters[format("channel%d-h264-fix-pqp", i)] = std::to_string(rec_env.fix_pqp + 2); // "40";  //16M File
 #else		
			parameters[format("channel%d-h264-min-qp", i)] = "27";
			parameters[format("channel%d-h264-max-qp", i)] = "35";	//oasis 4.2.8 31=>42
			parameters[format("channel%d-h264-enable-fixqp", i)] = "0";
			parameters[format("channel%d-h264-fix-iqp", i)] = "10";
			parameters[format("channel%d-h264-fix-pqp", i)] = "20";
 #endif
#endif 
		}
	}

#if 0	//test
	if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
		parameters["channel1-h264-min-qp"] = "25";
		parameters["channel2-h264-min-qp"] = "25";
		parameters["channel3-h264-min-qp"] = "25";
	}
#endif
#ifndef TARGET_CPU_V536		
	//front+rear common settings
	if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH){
		if(rec_env.rear_resolution == CFG_RESOLUTION_480I){
			parameters["channel1-h264-min-qp"] = "15";
			parameters["channel2-h264-min-qp"] = "15";
			parameters["channel3-h264-min-qp"] = "15";
		}
		else {
			parameters["channel1-h264-min-qp"] = "25";
			parameters["channel2-h264-min-qp"] = "25";
			parameters["channel3-h264-min-qp"] = "25";
		}
		parameters["enable-dynamic-qp-range"] = std::to_string(1);
	}
	if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_MIDDLE){
		parameters["channel1-h264-min-qp"] = "35";
		parameters["channel2-h264-min-qp"] = "35";
		parameters["channel3-h264-min-qp"] = "35";	
		
		if(rec_env.rear_resolution == CFG_RESOLUTION_480I){
			parameters["channel2-h264-min-qp"] = "27";	
			parameters["channel3-h264-min-qp"] = "27";	
		}
		
		parameters["enable-dynamic-qp-range"] = std::to_string(0);

	}
	else{//LOW
		parameters["channel1-h264-min-qp"] = "34";  //35?? ????? ???ζ? 34?? Fix
		parameters["channel2-h264-min-qp"] = "32";
		parameters["channel3-h264-min-qp"] = "32";
		parameters["enable-dynamic-qp-range"] = std::to_string(0);
	}


  #ifdef DEF_REAR_CAMERA_ONLY	//test
	parameters["channel1-h264-min-qp"] = "25"; //"27"; //"23";
	parameters["channel2-h264-min-qp"] = "25";
	parameters["channel3-h264-min-qp"] = "25";
	
	parameters["enable-dynamic-qp-range"] = std::to_string(1);
  #endif


 #if 0
	parameters["dynamic-qp-increase-factor"] = std::to_string(2);
	parameters["dynamic-qp-decrease-factor"] = std::to_string(1);
	parameters["dynamic-qp-underflow-threshold"] = std::to_string(6);
 #else
	parameters["dynamic-qp-increase-factor"] = std::to_string(3);
	parameters["dynamic-qp-decrease-factor"] = std::to_string(1);
	parameters["dynamic-qp-underflow-threshold"] = std::to_string(1);
 #endif
#endif

	//audio settings
	if(audio_enable){
		parameters["snd-card"] = "0";
		parameters["snd-dev"] = "0";
		parameters["snd-path"] = "default";		//change to use ALSA
	}
	else {
		parameters["snd-card"] = "-1";
		parameters["snd-dev"] = "-1";
		//parameters["snd-path"] = "default";
	}
	parameters["snd-input-channels"] = "1";
	parameters["snd-input-sample-size"] = "16";

#if 1
#if MIC_16KHZ
	parameters["snd-input-sampling-rate"] = "16000";
#elif MIC_24KHZ
	parameters["snd-input-sampling-rate"] = "24000";
#else
	parameters["snd-input-sampling-rate"] = "22050";
#endif
#else
	parameters["snd-input-sampling-rate"] = "48000";
#endif

	//osd settings
 #ifdef DEF_REAR_CAMERA_ONLY
 	if((l_rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_1080P)
		parameters["channel1-osd-font-size"] = "36"; //in point size
	else if((l_rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK) == CFG_RESOLUTION_720P)
		parameters["channel1-osd-font-size"] = "26"; //in point size
	else
		parameters["channel1-osd-font-size"] = "26"; //in point size
 #else
  #ifdef TARGET_CPU_V536
		parameters["channel1-osd-font-size"] = "26";
  #else
		parameters["channel1-osd-font-size"] = "36";  //"18"; //in point size
  #endif		
 #endif

	if(strcasecmp(res_rear.c_str(), "1080p")  == 0){
		parameters["channel2-osd-font-size"] = "18"; //in point size
		parameters["channel3-osd-font-size"] = "18"; //in point size
	}
	else{
		parameters["channel2-osd-font-size"] = "12"; //in point size
		parameters["channel3-osd-font-size"] = "12"; //in point size
	}
#endif

	return 0;
}

//return value true is signal detected.
bool recorder_takePicture(int32_t camera_id, bool file_save)
{
	bool detected = false;
	
	oasis::key_value_map_t parameters;
	parameters.clear();
	parameters["width"] = "0"; //full-width
	parameters["height"] = "0"; //full-height
	parameters["timeout"] = "5000"; //wait timeout max in msec

	std::vector<char> image_data;
	struct timeval timestamp;
	if(oasis::takePicture(camera_id, parameters, image_data, &timestamp) == 0 && image_data.size() > 0) {
		detected = true;
		
		if(file_save){
			time_t t = (time_t)timestamp.tv_sec;
			struct tm tm_t;
			localtime_r(&t, &tm_t);
			char path[512];
			sprintf(path, "/tmp/mnt/front-snapshot-%4d%02d%02d-%02d%02d%02d.jpg", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
			FILE *fp = fopen(path, "wb");
			if(fp) {
				fwrite(image_data.data(), 1, image_data.size(), fp);
				fclose(fp);
				DLOG(DLOG_INFO, "%s saved %d bytes.\n", path, image_data.size());
			} else {
				DLOG(DLOG_ERROR, "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
			}
		}
	}

	DLOG(DLOG_INFO, "%s() : %d\n", __func__, (int)detected);
	return detected;
}

ui::LabelRef recorder_create_label(int32_t width, int32_t height, Color clr, const char *text)
{
	ui::LabelRef label = ui::createLabelWithText(text);
	if(label){
		int font_size = 16;
		ui::ScreenRef screen = ui::getDefaultScreen();
		
		if(ui::screenWidth(screen) < 400)
			font_size = 8;
			
		ui::setSize(label, width, height);
		
		ui::setLabelHorzJustification(label, ui::kJustificationLeft);
		ui::setLabelVertJustification(label, ui::kJustificationTop);
		ui::setLabelTextColor(label, clr); //argb
#if FIXED_FONT_USED
		ui::setLabelFont(label, font_size, DEF_FONT_DIR "fonts/consola.ttf");
#else		
		ui::setLabelFont(label, font_size, DEF_FONT_DIR "fonts/GenBkBasB.ttf");
#endif
		}
	//

	return label;
}

void recorder_test_mode_set_text(ui::LabelRef &label, const char *text, bool pass)
{
	ui::setLabelTextColor(label, pass ? Color(128, 0, 255, 0) : Color(128, 255, 0, 0));
	ui::setLabelText(label, text);
}

void recorder_test_mode_processer(void)
{
	static int pre_rtc_time;
	bool is_ok = false;
	int fd, retval;

	std::string title;

    struct rtc_time rtc_tm;

	fd = open("/dev/rtc0", O_RDONLY);
    if (fd == -1) {
        perror("Requested device cannot be opened!");
    }
 	else {
	    /* Reading Current RTC Date/Time */
	     retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
		 close(fd);
		 
	    if (retval == -1) {
	        perror("ioctl");
	    }

		if(pre_rtc_time != rtc_tm.tm_sec)
			is_ok = true;
		else
			is_ok = false;

		pre_rtc_time = rtc_tm.tm_sec;
		
			
	    title =  oasis::format(" RTC %s : %d/%d/%d %02d:%02d:%02d ",
			 is_ok ? "OK" : "NG",
	        rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday,
	        rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

		recorder_test_mode_set_text(l_test_osd_label.rtc, title.c_str(), is_ok);
		l_test_result.b.rtc = is_ok;
 	}

	//gsensor
   #define fabs(x) ((x) < 0 ?(-(x)) : (x))

	PST_GSENSOR_CONTROL gctl = G_sensorGetControl();

	float x, y, z;
	x = fabs(gctl->X);
	y = fabs(gctl->Y);
	z = fabs(gctl->Z);

	if(x + y + z > 0.8)
		is_ok = true;
	else
		is_ok = false;
	
	 title =  oasis::format(" x : %.3f  y: %.3f  z : %.3f ",
			 gctl->X,
			 gctl->Y,
			 gctl->Z);

	 recorder_test_mode_set_text(l_test_osd_label.gsensor, title.c_str(), is_ok);
	 l_test_result.b.gsensor = is_ok; 

#if DEF_SUB_MICOM_USE
	// SUB MCU
	title =  oasis::format(" Mcu Ver : %d  (%0.1fV  %0.1fC) ", \
			l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER, \
			 l_sub_mcu.m_volt, l_sub_mcu.getTemp());

	if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER < 1 ||\
		l_sub_mcu.m_volt > 12.3 || l_sub_mcu.m_volt < 11.3 || \
		l_sub_mcu.m_temp > 60.0 || l_sub_mcu.m_temp < 10.0)
		is_ok = false;
	else
		is_ok = true;
	
	recorder_test_mode_set_text(l_test_osd_label.submcu, title.c_str(), is_ok);
	l_test_result.b.submcu = is_ok;
#endif

	 //gps
	CGPSData gps = evdev_input_get_gpsdata();
	CPulseData pulse = evdev_input_get_pulsedata();

	if(pulse.m_bGpsWasConnection){
		is_ok = true;

		 title =  oasis::format(" GPS OK : %d/%d/%d %02d:%02d:%02d ",
	        gps.m_nYear + 2000, gps.m_nMonth + 1, gps.m_nDay,
	        gps.m_nHour, gps.m_nMinute, gps.m_nSecond);
	}
	else {
		is_ok = false;
		title =  oasis::format(" No GPS ");
	}

	 recorder_test_mode_set_text(l_test_osd_label.gps, title.c_str(), is_ok);
	l_test_result.b.gps = is_ok;
	
	 //extcam
	if(rear_camera_detected)
		title =  oasis::format(" ExtCam OK ");
	else 
		title =  oasis::format(" ExtCam not Detected ");

	recorder_test_mode_set_text(l_test_osd_label.ext_cam, title.c_str(), rear_camera_detected);
	//if((l_recorder_camera2_id >= 0) && (l_recorder_camera3_id >= 0))	//check the rear 2ch
	if(l_recorder_camera_counts == 3)	//check the CH Num because capture size error
		l_test_result.b.ext_cam = rear_camera_detected;

	// pulse
	title = oasis::format(" PC:%d",	(int)pulse.m_iPulseSec);
	l_test_result.b.pulse = pulse.m_iPulseSec ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse, title.c_str(), pulse.m_iPulseSec ? true : false);

	title = oasis::format("RC:%d",	(int)pulse.m_iRpmPulseSec);
	l_test_result.b.pulse_Rpm = pulse.m_iRpmPulseSec ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_Rpm, title.c_str(), pulse.m_iRpmPulseSec ? true : false);

	title = oasis::format("Br:%d",	(int)pulse.m_bBrk);
	l_test_result.b.pulse_Br = pulse.m_bBrk ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_Br, title.c_str(), pulse.m_bBrk ? true : false);

	title = oasis::format("R:%d",	(int)pulse.m_bSR);
	l_test_result.b.pulse_R = pulse.m_bSR ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_R, title.c_str(), pulse.m_bSR ? true : false);

	title = oasis::format("L:%d",	(int)pulse.m_bSL);	
	l_test_result.b.pulse_L = pulse.m_bSL ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_L, title.c_str(), pulse.m_bSL ? true : false);

	title = oasis::format("Bg:%d",	(int)pulse.m_bBgr);
	l_test_result.b.pulse_Bg = pulse.m_bBgr ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_Bg, title.c_str(), pulse.m_bBgr ? true : false);

	title = oasis::format("T:%d",	(int)pulse.m_bTR);
	l_test_result.b.pulse_T = pulse.m_bTR ? true : false;
	recorder_test_mode_set_text(l_test_osd_label.pulse_T, title.c_str(), pulse.m_bTR ? true : false);


	if(access("/sys/class/net/wlan0", F_OK) == 0)
	{
		// wifi
		char ip_addr[128] = {0,};
		if(datool_getIPAddress(ip_addr))
			is_ok = true;
		else
			is_ok = false;
		
		title = oasis::format(" IP : %s ", ip_addr);
			
		recorder_test_mode_set_text(l_test_osd_label.wifi, title.c_str(), is_ok);
		l_test_result.b.wifi = is_ok;
	}
}


void recorder_set_pip_size(ui::PreviewRef preview, int size, bool hv_flip) // 2 = 1/4, 
{
		ui::ScreenRef screen = ui::getDefaultScreen();
		//ASSERT(screen != nullptr);

		int32_t screen_width = ui::screenWidth(screen);
		int32_t screen_height = ui::screenHeight(screen);
		
		ui::setPos(preview, 0, 0);
		ui::setSize(preview, screen_width, screen_height);

		if(l_recorder_camera2_id >= 0){
			int32_t pip_width = screen_width/size;
			int32_t pip_height = screen_height/size;
		
			ui::previewSetPIPSize(preview, pip_width, pip_height);
			ui::previewSetPIPPos(preview, screen_width - pip_width, screen_height - pip_height);
			//ui::previewAlignPIPSurface(preview, kPositionLeft, kPositionTop);
		
			ui::previewShowPIPSurface(preview, true);
		}
}

ui::PreviewRef recorder_create_osd(oasis::key_value_map_t &parameters)
{
	int position = 0;
	int division = 1;
	ui::ScreenRef screen = ui::getDefaultScreen();
	//ASSERT(screen != nullptr);

	int32_t screen_width = ui::screenWidth(screen);
	int32_t screen_height = ui::screenHeight(screen);
	

	if(screen_width < 400)
		division = 2;
	
	//live1
	ui::WindowRef live1 = ui::createWindow("Live1");
	//ASSERT(live1 != nullptr);

	ui::FixedRef fixed1 = ui::createFixed();
	ui::containerAdd(live1, fixed1);

	static ui::PreviewRef preview1 = ui::createPreview(0, parameters);
	ui::fixedPut(fixed1, preview1, 0, 0);
	
	ui::FixedRef fixed2 = ui::createFixed();
	ui::containerAdd(live1, fixed2);
		
	//printf("preview 1 = %d, width = %d, height = %d \r\n", preview1, screen_width, screen_height);//

// date osd create code
	l_osd_date_label = recorder_create_label(screen_width-60, 30 , Color(0x80ffffff), "");

	if(l_osd_date_label){
		ui::fixedPut(fixed2, l_osd_date_label, 30, screen_height - (++position * 30));
	}
//
#if 1
	if(l_sys_setup.cfg.bTestMode){
   // Setup
   		char szText[64];
		Color color;
		if(l_sys_setup.cfg.iVideoQuality == DEF_DEFAULT_VIDEOQUALITY && l_sys_setup.cfg.iEventMode == DEF_DEFAULT_EVENT_SPACE_MODE && l_sys_setup.cfg.iGsensorSensi == DEF_DEFAULT_GSENSOR_SENS){
			color = Color(128, 0, 255, 0);
			l_test_result.b.setup = true;
		}
		else {
			color = Color(128, 255, 0, 0);
			l_test_result.b.setup = false;
		}

		sprintf(szText, "V.Q : %d   S.M : %d   G.S : %d ", l_sys_setup.cfg.iVideoQuality, l_sys_setup.cfg.iEventMode, l_sys_setup.cfg.iGsensorSensi);

		l_test_osd_label.setup = recorder_create_label(screen_width-100, 30 , color, szText);
		ui::fixedPut(fixed2, l_test_osd_label.setup , 50, screen_height - (++position * 30));

		// WiFi
		l_test_osd_label.wifi = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " IP:0.0.0.0 ");
		ui::fixedPut(fixed2, l_test_osd_label.wifi , 50, screen_height - (++position * 30));
			
		// PULSE
		l_test_osd_label.pulse = recorder_create_label(100 / division, 30 , Color(200, 255, 0, 0), " PC:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse , 50, screen_height - (++position * 30));
		l_test_osd_label.pulse_Rpm = recorder_create_label(100 / division, 30 , Color(200, 255, 0, 0), "RC:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_Rpm, 50 + (100 /division), screen_height - (position * 30));
		l_test_osd_label.pulse_Br = recorder_create_label(60 / division, 30 , Color(200, 255, 0, 0), "Br:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_Br , 50 + (200 /division), screen_height - (position * 30));
		l_test_osd_label.pulse_R = recorder_create_label(60 / division, 30 , Color(200, 255, 0, 0), "R:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_R , 50 + (260 /division), screen_height - (position * 30));
		l_test_osd_label.pulse_L = recorder_create_label(60 / division, 30 , Color(200, 255, 0, 0), "L:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_L , 50 + (320 /division), screen_height - (position * 30));
		l_test_osd_label.pulse_Bg = recorder_create_label(60 / division, 30 , Color(200, 255, 0, 0), "Bg:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_Bg , 50 + (380 /division), screen_height - (position * 30));
		l_test_osd_label.pulse_T = recorder_create_label(60 / division, 30 , Color(200, 255, 0, 0), "T:0");
		ui::fixedPut(fixed2, l_test_osd_label.pulse_T , 50 + (440 /division), screen_height - (position * 30));
		
		// ExtCam
		l_test_osd_label.ext_cam = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " ExtCam not Detected ");
		ui::fixedPut(fixed2, l_test_osd_label.ext_cam , 50, screen_height - (++position * 30));
		
		// RTC
		l_test_osd_label.rtc = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " RTC NG ");
		ui::fixedPut(fixed2, l_test_osd_label.rtc , 50, screen_height - (++position * 30));
		
		// GPS
		l_test_osd_label.gps = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " No GPS ");
		ui::fixedPut(fixed2, l_test_osd_label.gps , 50, screen_height - (++position * 30));
		
		// G-Sensor
		l_test_osd_label.gsensor = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " x : 0.000  y: 0.000  z : 0.000 ");
		ui::fixedPut(fixed2, l_test_osd_label.gsensor , 50, screen_height - (++position * 30));

		// SUB MCU 
		l_test_osd_label.submcu = recorder_create_label(screen_width-100, 30 , Color(200, 255, 0, 0), " Mcu Ver : (0  0.0V  0.0C) ");
		ui::fixedPut(fixed2, l_test_osd_label.submcu , 50, screen_height - (++position * 30));

		// SN
#if DEF_SUB_MICOM_USE	
		if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
			sprintf(szText, " SN : %u", l_rec_env.serial_no);
			ui::LabelRef label_sn = recorder_create_label(screen_width-100, 30 , Color(0x80ffffff), szText);
			ui::fixedPut(fixed2, label_sn, 50, screen_height - (++position * 30));
		}
#endif

		// FW Ver
		ui::LabelRef label_ver = recorder_create_label(screen_width-100, 30 , Color(0x80ffffff), " FW Ver : " DA_MODEL_NAME "_" DA_FIRMWARE_VERSION);
		ui::fixedPut(fixed2, label_ver, 50, screen_height - (++position * 30));

	//
	
	}
#else //listbox use
	//if(0){
	if(l_sys_setup.cfg.bTestMode){
		int position = 0;
		ui::ListBoxRef list_box = ui::createListBox();

/*		
		void listBoxRemove(ListBoxRef &listbox, WidgetRef widget);
		void listBoxRemoveAll(ListBoxRef &listbox);
		void listBoxSetPlaceHolder(ListBoxRef &listbox, WidgetRef placeholder);
		void listBoxGoUp(ListBoxRef &listbox);
		void listBoxGoDown(ListBoxRef &listbox);
		void listBoxGoPageUp(ListBoxRef &listbox);
		void listBoxGoPageDown(ListBoxRef &listbox);
*/		
	// FW Ver
		ui::LabelRef label_ver = recorder_create_label(screen_width-300, 30 , Color(0x80ffffff), " FW Ver. = " DA_MODEL_NAME "_" DA_FIRMWARE_VERSION);
		if(label_ver){
			ui::listBoxInsert(list_box, label_ver, position++);
		}
	//

	// G-Sensor
		l_test_osd_label.gsensor = recorder_create_label(screen_width-300, 30 , Color(200, 255, 0, 0), " x : 0.000  y: 0.000  z : 0.000 ");
		if(l_test_osd_label.gsensor){
			ui::listBoxInsert(list_box, l_test_osd_label.gsensor, position++);
		}
	//

	// GPS
		l_test_osd_label.gps = recorder_create_label(screen_width-300, 30 , Color(200, 255, 0, 0), " No GPS ");
		if(l_test_osd_label.gps){
			ui::listBoxInsert(list_box, l_test_osd_label.gps, position++);
		}
	//

	// RTC
		l_test_osd_label.rtc = recorder_create_label(screen_width-300, 30 , Color(200, 255, 0, 0), " RTC NG ");
		if(l_test_osd_label.rtc ){
			ui::listBoxInsert(list_box, l_test_osd_label.rtc , position++);
		}
	//
	
	// ExtCam
		l_test_osd_label.ext_cam = recorder_create_label(screen_width-300, 30 , Color(200, 255, 0, 0), " ExtCam not Detected ");
		if(l_test_osd_label.ext_cam ){
			ui::listBoxInsert(list_box, l_test_osd_label.ext_cam , position++);
		}
	//

   // Setup
   		char szSetup[64];
		Color color;
		if(l_sys_setup.cfg.iVideoQuality == 0 && l_sys_setup.cfg.iEventMode == 1 && l_sys_setup.cfg.iGsensorSensi == 3)
			color = Color(200, 0, 255, 0);
		else
			color = Color(200, 255, 0, 0);

		sprintf(szSetup, "V.Q : %d   S.M : %d   G.S : %d ", l_sys_setup.cfg.iVideoQuality, l_sys_setup.cfg.iEventMode, l_sys_setup.cfg.iGsensorSensi);

		l_test_osd_label.setup = recorder_create_label(screen_width-300, 30 , color, szSetup);
		if(l_test_osd_label.setup){
			ui::listBoxInsert(list_box, l_test_osd_label.setup, position++);
		}
	//

		ui::FixedRef fixed3 = ui::createFixed();
		ui::containerAdd(live1, fixed3);
		ui::fixedPut(fixed3, list_box, 50, 270);
	}
#endif
	ui::screenAdd(screen, live1);
	
	return preview1;
}

bool recorder_camera_check(int camera_id, int resolution)
{
	if(camera_id >= 0) {
#if 1
		oasis::key_value_map_t parameters;
		
	//	int32_t width, height, camera;
		DLOG0(DLOG_WARN, "rear camera check \r\n");
		snapshot_parameters_set(parameters, resolution);
					
		struct timeval timestamp;
		std::vector<char> image_data;

		if(oasis::takePicture(camera_id, parameters, image_data, &timestamp) < 0) {
			DLOG0(DLOG_INFO, "Take Picture Timeout\r\n");
			return false;
		}

		char *pData = image_data.data();
		u64 value = 0;
		for ( unsigned int i = 0; i < image_data.size(); i++) {
			value += (u64)pData[i];
		}

		DLOG0(DLOG_WARN, "Take Picture %lld \r\n", value);

		if(value > 500000)
			return true;
#endif
	}
	
	return false;
}

int recorder_rear_camera_type_check(void)
{
	int result = 0;
#if DEF_SUB_MICOM_USE
	if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
		int retry = 0;
		char cvstd[256] = {0,};
		
CAMERA_CHECK:	

#ifdef TARGET_CPU_V536
		#define VIDEO_CVSTD "cat /sys/devices/tp2863/cvstd"
#else
		#define VIDEO_CVSTD "cat /sys/devices/tp2825/cvstd"
#endif

		//	"1080p25"; "1080p30"; "720p25"; "720p25"; "720p30"; "720p50"; "720p60"; "SENSOR_SD"; "invalid"; "720p25v2"; "720p30v2"; "720p30v2"; "ntsc";
		if(SB_CmdScanf(VIDEO_CVSTD, "%s", cvstd)){
			u32 res = CFG_RESOLUTION_480I;

			DLOG(DLOG_TRACE, "Rear Camera Resolution %s\r\n", cvstd);
			
			if(!strcmp("720p30", cvstd) || !strcmp("720p30v2", cvstd) || !strcmp("720p60", cvstd)){
				res = CFG_RESOLUTION_720P;
			}
			else if(!strcmp("1080p30", cvstd) || !strcmp("1080p60", cvstd)){
				res = CFG_RESOLUTION_1080P;
			}

			if(strcmp("invalid", cvstd)){
				if((l_rec_env.hw_config & 0x3) != res){
					if(l_is_recording){
						postCommand(kStopRecording);
						postCommand(kStartRecording); // restart recording for camera type change
						result = 1;
					}
					else {
						if(retry++ < 2){
							sleep(1);
							goto CAMERA_CHECK;
						}
						
						DLOG(DLOG_TRACE, "Rear Camera Changed. Start rebooting. (%d => %d)\r\n", l_rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK, res);
						SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_SYSEND ,0 , "RCAM %d => %d", l_rec_env.hw_config & HW_CONFIG_REAR_CAM_TYPE_MASK, res);
							
						l_rec_env.hw_config = (res | (l_rec_env.hw_config & ~HW_CONFIG_REAR_CAM_TYPE_MASK));
						l_sub_mcu.SetEepromData(EEPROM_HW_CONFIG, l_rec_env.hw_config);
		
						if(l_sub_mcu.GetEepromData(EEPROM_HW_CONFIG) == l_rec_env.hw_config){
							recorder_reboot();
							result = 1;
						}
						else {
							DLOG(DLOG_ERROR, "EEPROM Save Error!! (0x%x : 0x%x)\r\n", l_sub_mcu.GetEepromData(EEPROM_HW_CONFIG), l_rec_env.hw_config);
						}
					}
				}
			}
		}	
		
	}
#endif
	return result;
}

int recorder_serialkey_precess(void)
{
	static int cmd_check = 0;
//	static int32_t pre_cmd = 0; 
	const char *key = "root\n";
	int32_t c;

	c = tty_.tty_getchar();
	
	if( c >= 0 ){
		if(cmd_check < 5){
			if(c  == key[cmd_check]){
				cmd_check++;
				if(cmd_check == 5)
					print_menu();
			}
			else {
				cmd_check = 0;
				DLOG(DLOG_INFO, "Please enter \"root\" + \"enter key\"\n");
			}
			return 0;
		}
		else
 #if 0			
			if(c == 0x5B || pre_cmd == 0x5B){
				if(pre_cmd == 0x5B){
					static u8 i2c_reg = 0;
					static u8 i2c_data = 0;
					u8 rw_flag = 1;
					switch(c)
					{
					case 0x43 : //right
						i2c_reg += 1;
						break;
					case 0x44 : //left
						i2c_reg -= 1;
						break;
					case 0x41 : //up
						rw_flag = 0;
						i2c_data += 1;
						break;						
					case 0x42 : //down
						rw_flag = 0;
						i2c_data -= 1;
					break;
					}

					if(rw_flag){
					}
					else {
					}
				}
				pre_cmd = c;
				return 0;
			}
#endif			
			print_menu();

			if(c == 'e' || c == 'E') {
				postCommand(kStartEventRecording);
#if 0			
				const char *s_argv[] ={ "/usr/bin/aplay", "/data/131.wav", NULL};

				// execl( "aplay", "aplay", "/data/131.wav", NULL);
				//execv("aplay", s_argv);
				//execlp("aplay", "aplay", "/data/131.wav", NULL);
				exec_prog(s_argv);  
				DLOG(DLOG_WARN, "aplay 131.wav\n");
#endif			
			} else if(c == 'x' || c == 'X') {
				led_set_normal_recording_end();
				kill_surveillant_app();
				cancel_handler(-9);
				return -1;
#if 0//ENABLE_ADAS
			} else if(c == 'a' || c == 'A') {
				if(adas_enabled_) {
					DLOG(DLOG_INFO, "disable adas\n");
					adas_enabled_ = false;
					ui::previewEnableADAS(preview1, false);
				} else {
					oasis::key_value_map_t parameters;
					DLOG(DLOG_INFO, "enable adas\n");
					parameters.clear();
					parameters["adas-gps-enabled"] = "0";
					parameters["adas-gsensor-enabled"] = "1";
					parameters["adas-force-default-view-angle"] = "1";
					parameters["adas-default-horz-view-angle"] = "105";
					parameters["adas-default-vert-view-angle"] = "63";
					adas_enabled_ = ui::previewEnableADASWithParameters(preview1, parameters) == 0;
				}
#endif
			} else if(c == 'w' || c == 'W') {
				postCommand(kSwapSurfaces);
			} else if(c == 'p' || c == 'P') {
				power_disconnected = false;
				l_normal_recording_type = ' ';
				DLOG(DLOG_INFO, "Recording Change\n");
				if(l_recorder_ref && oasis::recorderIsRunning(l_recorder_ref)) {
					postCommand(kStopRecording);
				}
				else {
					//rear_disabled_ = !rear_disabled_;
					postCommand(kStartRecording);
					
				}
			}else if(c == 'r' || c == 'R') {
				l_normal_recording_type = ' ';
				postCommand(kRestartRecording);
			}/* else if(c == 'f' || c == 'F') {
				int32_t normal, event;
				fs::getOffsSchedulingThresholds(normal, event);
				DLOG(DLOG_INFO, "OFFS thresholds: normal %d event %d\n", normal, event);
				fs::setOffsCurrentThresholds(normal, event, 0);
			} else if(c == 'h' || c == 'H') {
				int32_t normal, event;
				fs::getOffsSchedulingThresholds(normal, event);
				DLOG(DLOG_INFO, "OFFS thresholds: normal %d event %d\n", normal, event);
				fs::setOffsCurrentThresholds(normal/2, event/2, 0);
			} else if(c == 'r' || c == 'R') {
				fs::setOffsCurrentThresholds(0, 0, -1);
			} */else if(c == 'm' || c == 'M') {
				postCommand(kChangeRecordingMode);
			} else if(c == 's' || c == 'S') {
				postCommand(kTakeFrontSnapshot);
			} else if(c == 'd' || c == 'D') {
				if(rear_disabled_ == false) {
					postCommand(kTakeRearSnapshot);
				}
			} else if(c == 't' || c == 'T') {
				auto_recording_event_ = auto_recording_event_ == false;
				DLOG(DLOG_INFO, ">>>>>>>>>>>>>>>>>>>>> auto-start recording event %s\n", auto_recording_event_ ? "enabled":"disabled");
				DLOG(DLOG_INFO, "auto-start recording event %s\n", auto_recording_event_ ? "enabled":"disabled");
				if(auto_recording_event_) {
					postCommand(kStartEventRecording);
				}
			} else if(c == 'n' || c == 'N') {
				auto_stop_and_restart_normal_recording_ = auto_stop_and_restart_normal_recording_ == false;
				DLOG(DLOG_INFO, ">>>>>>>>>>>>>>>>>>>>> auto-stop and restart normal recording %s\n", auto_stop_and_restart_normal_recording_ ? "enabled":"disabled");
			}
	}

	return 0;
}

void recorder_tty_working(ui::PreviewRef &preview1)
{
	int32_t c;
	do{
		msleep(1);
		c = recorder_serialkey_precess();
		if(c < 0) {
			break;
		}
	} while (timer_running_ == true);
}

int main(int argc, char* argv[]) {
	int32_t err;
	int is_record_restart_good = 0;
	int is_record_start_good = 0;
	int is_record_stop_good = 0;
	
	std::thread rec_text_thread, rec_cmd_thread;
	std::thread recorder_thread;
	int main_surface_is_front_camera = 1;
	u32 snapshot_type_id  = 0;
	u32 snapshot_retry  = 0;
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)	
	u32 replace_offsbusy  = 0;
#endif

#if DEF_REAR_CAMERA_ONLY
	const char * ini_path = INI_REAR_2CH_FILE_PATH;
#else
	const char * ini_path = INI_3CH_FILE_PATH;
#endif

	char value[1024];
	struct parameter_info *param;
	
	if (argc >= 2) {
		if(access(argv[1], F_OK) == 0){
			if(oasis::get_profile_string("recorder", "file-duration-secs", nullptr, value, sizeof(value), argv[1]) > 0)
				ini_path = argv[1];
		}
		else
			DLOG(DLOG_ERROR, "\"%s\" not found\n", argv[1]);
	}
	
	l_rec_env.rec_secs = RECORDING_DURATION_SECS;
	l_rec_env.front_bitrate = BITRATE_1080P;
	l_rec_env.rear_bitrate = BITRATE_720P;
	
	if(datool_test())
		return 0;

	//Oasis set SIGINT
	signal(SIGINT, cancel_handler);

#if DEF_SUB_MICOM_USE
	l_sub_mcu.Initialize();	
	l_sub_mcu.SetWatchdogTme(30);
#endif

	if(oasis::get_profile_string("recorder", "file-duration-secs", nullptr, value, sizeof(value), ini_path) > 0) {
		l_rec_env.rec_secs = atoi(value);
	}
	else {
		DLOG0(DLOG_INFO, "%s : ERROR !!! %s  file not find.>\n", argv[0], ini_path);
		return -1;
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////

	std::thread t_watcher = std::thread([&]() {
		u32 l_watcher_working_ms = 0;
		led_process_start();
		led_process_postCommand(LED_BLUE, 0, LED_TIME_INFINITY);
		led_process_postCommand(LED_WIFI, 0, LED_TIME_INFINITY);
		led_process_postCommand(LED_WORK_MODE, LED_WORK_BOOTING, LED_TIME_INFINITY);
		do {
				if(is_time_out_ms(&l_watcher_working_ms, 1000)){
					
				}
				msleep(100);
				led_process_postCommand(LED_TIME_100MS, 0, 0);
		}while(timer_running_);
		
		led_process_stop();
	});

#if DEF_SUB_MICOM_USE
	if(l_sub_mcu.m_i2c_reg.st.FIRMWARE_VER >= 3){
		l_rec_env.serial_no = l_sub_mcu.GetEepromData(EEPROM_SERIAL_NO);
		l_rec_env.hw_config = l_sub_mcu.GetEepromData(EEPROM_HW_CONFIG);

		
		sysfs_printf(DEF_DEFAULT_SSID_TMP_PATH, "%u", l_rec_env.serial_no);

		// test code
		//DLOG0(DLOG_INFO,  " EEPROM 0 0x%08x 0x%08x 0x%08x\r\n", l_sub_mcu.GetEepromData(0), l_sub_mcu.GetEepromData(1), l_sub_mcu.GetEepromData(2));
		//l_sub_mcu.SetEepromData(0, 0x12345678);
		//l_sub_mcu.SetEepromData(1, 0x23456789);
		//l_sub_mcu.SetEepromData(2, 0x34567890);
		//DLOG0(DLOG_INFO,  " EEPROM 0 0x%08x 0x%08x 0x%08x\r\n", l_sub_mcu.GetEepromData(0), l_sub_mcu.GetEepromData(1), l_sub_mcu.GetEepromData(2));
	}
#endif

#ifdef DEF_PAI_R_DATA_SAVE
	recorder_pai_r_local_autoid_init();
#endif // end of DEF_PAI_R_DATA_SAVE

	////////////////////////////////////////////////////////////////////////////////////////////
	recorder_thread = std::thread(recorder_env_process);
	do {
		if(recorder_env_setup(&l_rec_env, l_is_parking_mode))
			break;

	 	if(recorder_serialkey_precess() < 0){
			DLOG0(DLOG_INFO, "exit!!!\n");
			timer_running_ = false;
	 	}
	 	else	
		{
#if DEF_SUB_MICOM_USE			
			if((int)(l_sub_mcu.getBatteryVoltage() * 10) < LOW_BATTERY_MIN_VOLT || (get_tick_count() > 120000 && recorder_is_sd_exist())){
				sleep(1);
				if((int)(l_sub_mcu.getBatteryVoltage() * 10) < LOW_BATTERY_MIN_VOLT){
					sleep(2);
					l_sub_mcu.SetPowerMode(BB_MICOM_POWER_OFF_FALL);
					sleep(1);
				}
			}
#endif	
		}
		printf(".");
		msleep(100);
	} while (timer_running_ == true);

	if(timer_running_ == false) {
		sleep(1);
		if(t_watcher.joinable()){
			t_watcher.join();
		}
		return -1;
	}

	////////////////////////////////////////////////////////////////////////////////////////////
	
	//system("echo 3 > /proc/sys/vm/drop_caches");

	DLOG0(DLOG_INFO, "header write interval: %d secs\n", HEADER_WRITE_INTERVAL);

	
	
	oasis::key_value_map_t parameters;
	oasis::key_value_map_t preview_parameters;
	oasis::key_value_map_t recorder_parameters;
	oasis::key_value_map_t adas_parameters;
	oasis::key_value_map_t md_parameters;

	srand(time(NULL));
	
	bool offs_disabled = false;
	err = oasis::get_profile_string("general", "offs-enable", nullptr, value, sizeof(value), ini_path);
	if(err > 0) {
		offs_disabled = atoi(value) == 0;
	}

	if(!offs_disabled) {
		parameters["offs-qsize-max"] = std::to_string(OFFS_QUEUE_SIZE_KBYTES);
		parameters["offs-overwrite-if-exist"] = "1";
		parameters["offs-cache-size"] = std::to_string(OFFS_CACHE_SIZE_KBYTES);
	} else {
		parameters["offs-disable"] = "1";			
	}

	err = oasis::get_profile_string("general", "media-cache-size", nullptr, value, sizeof(value), ini_path);
	if(err > 0) {
		parameters["media-cache-size"] = value;
	} else {
		parameters["media-cache-size"] = std::to_string(MEDIA_CACHE_SIZE_KBYTES);
	}


	// parameters["offs-cmd25-size-max"] = std::to_string(512*1024);
	// parameters["offs-cmd18-size-max"] = std::to_string(512*1024);

	//parameters["offs-log-flags"] = std::to_string(OFFS_LOG_IO_WRITE_BEGIN|OFFS_LOG_IO_WRITE_END|OFFS_LOG_IO_READ_BEGIN|OFFS_LOG_IO_READ_END|OFFS_LOG_IO_DEBUG);
	//parameters["offs-log-flags"] = std::to_string(OFFS_LOG_IO_DEBUG);	//
	parameters["oasis-log-flags"] = std::to_string(OASIS_LOG_DEBUG/*|OASIS_LOG_ENCODE_BITRATE*/);
	
#if defined(TARGET_CPU_V3) || defined(TARGET_CPU_I3)
#if ENABLE_ADAS	
	//++ oasis 4.1.29
	parameters["adas-library-path"]	= "/lib/libAdas.so";
#else
	parameters["adas-library-path"]	= "/adas_not_used";
#endif
#endif

	if(oasis::initialize(parameters) < 0) {
		DLOG(DLOG_ERROR, "Oasis init failed\n");
		return -1;
	}

#ifdef TARGET_CPU_V536
	oasis::setLicense("/data/datech-v536.lic");
#else
	oasis::setLicense("/data/oasis.lic");
#endif

	////////////////////////////////////////////////////////////////////////////////////////////
	// display setup (skip if not used)

	param = display_parameter_list_;
	for(; param->name != nullptr; param++) {
		err = oasis::get_profile_string("display", param->name, nullptr, value, sizeof(value), ini_path);
		if(err > 0) {
			parameters[param->name] = value;
		}
	}

	display::setup(parameters);

	////////////////////////////////////////////////////////////////////////////////////////////
	// ui setup
	
#if 0
	if(argc > 5) {
		auto_recording_event_ = true;
	}
#endif

	std::string ext = "avi";

	err = oasis::get_profile_string("recorder", "file-extension", nullptr, value, sizeof(value), ini_path);
	if(err > 0) {
		ext = value;
	}
	
	is_avi_format_ = strcasecmp(ext.c_str(), "avi") == 0;

#ifdef DEF_REAR_CAMERA_ONLY
	l_recorder_camera1_id = 2;
	l_recorder_camera2_id = 3;
#else
	if(oasis::get_profile_string("recorder", "channel1-camera-id", nullptr, value, sizeof(value), ini_path) > 0)
		l_recorder_camera1_id = atoi(value);
	if(oasis::get_profile_string("recorder", "channel2-camera-id", nullptr, value, sizeof(value), ini_path) > 0)
		l_recorder_camera2_id = atoi(value);
	if(oasis::get_profile_string("recorder", "channel3-camera-id", nullptr, value, sizeof(value), ini_path) > 0)
		l_recorder_camera3_id = atoi(value);
	if(oasis::get_profile_string("recorder", "channel4-camera-id", nullptr, value, sizeof(value), ini_path) > 0)
		l_recorder_camera4_id = atoi(value);
#endif

#ifdef TARGET_CPU_V536	
	l_recorder_camera_counts = 0;

	if(l_recorder_camera1_id >= 0)
		l_recorder_camera_counts ++;
	
	rear_disabled_ = false;
	rear_camera_detected = false; // true
#else
	if(l_recorder_camera2_id < 0) {
		rear_disabled_ = true;
	}
#endif

#if 0
	if(access("/dev/video1", F_OK) != 0) {
		DLOG(DLOG_ERROR, "/dev/video1 not found!\r\n");
#if DEF_VIDEO_QUALITY_ONLY		
		l_rec_env.front_resolution = CFG_RESOLUTION_720P;
#endif
		l_recorder_camera1_id = 0;
		l_recorder_camera2_id = -1;
	}
	
	if (l_recorder_camera1_id == l_recorder_camera2_id) {
		DLOG(DLOG_ERROR, "Enter the different cameras.\r\n");
		return -1;
	}
#endif

	int32_t rotation= 0;

#if FRONT_720P
	std::string res_front = "720p";
#else
	std::string res_front = "1080p";
#endif

	if(l_rec_env.front_resolution)
		res_front = "1080p";
	else if(l_rec_env.front_resolution == CFG_RESOLUTION_360P)
		res_front = "360p";
	else
		res_front = "720p";

#if REAR_1080P
	std::string res_rear = "1080p";
#else
	std::string res_rear = "7220p";
#endif	

	if(l_rec_env.rear_resolution == CFG_RESOLUTION_1080P)
		res_rear = "1080p";
	else if(l_rec_env.rear_resolution == CFG_RESOLUTION_360P)
		res_rear = "360p";
	else
		res_rear = "720p";
	
	DLOG0(DLOG_INFO, "front is %s\r\n", res_front.c_str());
	DLOG0(DLOG_INFO, "rear is %s\r\n", res_rear.c_str());

	parameters.clear();
#if defined(TARGET_CPU_V3) || defined(TARGET_CPU_I3)	
	parameters["isp-camera-id"] = "0";
#endif	
/*
	note: dpi calculation (not used since 4.1.13):
		written in lcd module spec: 48.96*73.44 mm, 320 x 480 pixels
		x dpi = 320 x 25.4 / 48.96 = 166.013071071895
		y dpi = 480 x 25.4 / 73.44 = 166.013071071895
	parameters["lcd-dpi-x"] = "166.013071";
	parameters["lcd-dpi-y"] = "166.013071";
*/
	parameters["system-font-size"] = "9";
#if FIXED_FONT_USED
	parameters["system-font-path"] = DEF_FONT_DIR "fonts/consola.ttf";
#else
	parameters["system-font-path"] = DEF_FONT_DIR "fonts/GenBasR.ttf";
#endif

#if defined(TARGET_CPU_V536) && defined(PRODUCT_GWLINDENIS)
	//parameters["display-rotation"] = oasis::format("%d", rotation);  //oasis 2.2.29 added
#else	
	parameters["display-rotation"] = oasis::format("%d", rotation);  //oasis 2.2.29 added
#endif

	parameters["enable-touch"] = "0";
#if defined(TARGET_CPU_V3) || defined(TARGET_CPU_I3)
	parameters["hide-unmanaged-ui-layer"] = std::to_string(ui::Qt_HLAYER());
#else
	if(oasis::get_profile_string("display", "hide-unmanaged-ui-layers", nullptr, value, sizeof(value), ini_path) > 0) {
		parameters["hide-unmanaged-ui-layers"] = value;
	}
#endif

	////////////////////////////////////////////////////////////////////////////////////////////
	err = ui::setup(parameters);

	if (err < 0) {
		oasis::finalize();
		return -1;
	}

#if 0
	std::thread t = std::thread([&]() {
		ui::run();
	});
#endif

	////////////////////////////////////////////////////////////////////////////////////////////
	
	////////////////////////////////////////////////////////////////////////////////////////////
	
#if 1
	if( camera_parameters_set(parameters, l_recorder_camera1_id, l_recorder_camera2_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0){
		abort_main();
		return -1;
	}
	
	configCameras(parameters);
	//sleep(1);
#endif	
	
	////////////////////////////////////////////////////////////////////////////////////////////
	if(preview_parameters_set(preview_parameters, l_recorder_camera1_id, l_recorder_camera2_id, l_recorder_camera3_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0){
		abort_main();
		return -1;
	}
	/////////////////////////////////////////////////////////////////////////
	
	ui::PreviewRef preview1 = nullptr;

	//if(l_has_preview){
		preview1 = recorder_create_osd(preview_parameters);

	//////////////////////////////////////////////////////////////////////////////////////////
#if 1
		std::shared_ptr<MyFrameObserver> frame_observer = std::make_shared<MyFrameObserver>();
		ui::setPreviewFrameObserver(preview1, frame_observer);
#endif
	//}
	
#if ENABLE_MD
#endif

#if ENABLE_ADAS

#endif

#if DISPLAY_PIP_CONTROL_USE	
	if(preview1)
		recorder_set_pip_size( preview1, DISPLAY_PIP_FRONT_CAMERA_ONLY, G_sensorGetControl()->rotate == DX_ROTATE_180);
#endif

#if ENABLE_ADAS
//	ui::previewShowPIPSurface(preview1, false);
//	ui::previewShowCameraStream(preview1, 1, false);
#endif

	if(preview1) {
		ui::previewStart(preview1);
		//ui::previewSwapSurfaces(preview1, 0, 1);
	}
	sleep(1);


	//VIDIOC_QBUF failed: 22(Invalid argument)
	////++{** rear camera resolution check **********************************

	recorder_rear_camera_type_check();
	
	////++}*************************************************************
#if 1//ndef TARGET_CPU_V536		//??? Segmentation fault	//??? ????? ???
	if(l_recorder_camera2_id >= 0){
		if(recorder_camera_check(l_recorder_camera2_id, l_rec_env.rear_resolution)){
			rear_camera_detected = true;
			l_recorder_camera_counts++;
		}
		else{
			l_recorder_camera2_id = -1;
		}
	}
	if(l_recorder_camera3_id >= 0){
		if(recorder_camera_check(l_recorder_camera3_id, l_rec_env.rear_resolution)){
			rear_camera_detected = true;
			l_recorder_camera_counts++;
		}
		else{
			l_recorder_camera3_id = -1;
		}
	}
#endif

	if(preview1) {
#if !DISPLAY_PIP_CONTROL_USE
	//if(!rear_camera_detected)
#endif
		recorder_set_pip_size( preview1, rear_camera_detected ? DISPLAY_PIP_SIZE : DISPLAY_PIP_FRONT_CAMERA_ONLY, G_sensorGetControl()->rotate == DX_ROTATE_180);
	}
	////////////////////////////////////////////////////////////////////////////////////////////
	std::shared_ptr<MyRecordingObserver> recording_observer = std::make_shared<MyRecordingObserver>();
	
	////////////////////////////////////////////////////////////////////////////////////////////
	if(rear_camera_detected){
		rear_disabled_ = false;
		if(recorder_parameters_set(recorder_parameters, l_recorder_camera1_id, l_recorder_camera2_id, l_recorder_camera3_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0)
			goto config_failed;
	}
	else {
		rear_disabled_ = true;
		if( recorder_parameters_set(recorder_parameters, l_recorder_camera1_id, -1, -1, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0)
			goto config_failed;
	}

	////////////////////////////////////////////////////////////////////////////////////////////
	//adas settings
	//if(adas_parameters_set(adas_parameters, l_recorder_camera1_id, l_recorder_camera2_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0){
	//	DLOG0(DLOG_ERROR, "================> 0\r\n");
	//	goto config_failed;
	//}
	////////////////////////////////////////////////////////////////////////////////////////////
	//motion detector settings
	//if(motion_parameters_set(md_parameters, l_recorder_camera1_id, l_recorder_camera2_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path) < 0){
	//	DLOG0(DLOG_ERROR, "================> 4\n");
	//	goto config_failed;
	//}
#if 1
	l_recorder_ref = createRecorder(VIDEO_NORMAL_DIR, recorder_parameters);
	DLOG(DLOG_TRACE, "l_recorder_ref created %p\r\n", l_recorder_ref.get());
	if(l_recorder_ref == nullptr) {
		DLOG(DLOG_ERROR, "l_recorder_ref creation failed!\r\n");
		goto recorder_creation_failed;
	}

//

//
 #if 1//ndef TARGET_CPU_V536 // v536 rear recording error
	if(rotation == 0 && G_sensorGetControl()->rotate == DX_ROTATE_180)
	{
		err = startRecording(l_recorder_ref, recording_observer, nullptr, true, &strd_data_header_, sizeof(strd_data_header_));
		if(err < 0) {
			l_is_recording = false;
			//mixwav_play(kMixwaveRecordStop);
			DLOG(DLOG_ERROR, "start recording failed\r\n");
			timer_running_ = false;
			tty_.tty_cancel();
		}
		sleep(1);

		if(stopRecording(l_recorder_ref) == 0) {
							
		}
		else if(l_recorder_ref){
			destroyRecorder(l_recorder_ref);
			l_recorder_ref = nullptr;
		}
	}
 #endif	
#endif

	//*****************************************************************************************
	//*****************************************************************************************
	//*****************************************************************************************

	memset(&ma_strd_data_header_, 0, sizeof(ma_strd_data_header_));
	ma_strd_data_header_.Separator = '$';
	ma_strd_data_header_.Header = 'H';
	memcpy(ma_strd_data_header_.ProductInfo, DA_FIRMWARE_VERSION, 4);
	strcpy(ma_strd_data_header_.VersionInfo, strchr(DA_FIRMWARE_VERSION, '/') + 1);
	sprintf(ma_strd_data_header_.ChannelInfo, "CH:%d", l_recorder_camera_counts);

#ifdef DEF_USB_LTE_MODEM_USE
	if(1)
#else
	if(access("/sys/class/net/wlan0", F_OK) == 0) 
#endif		
	{
#ifndef HTTPD_EMBEDDED
		if(access(DA_HTTPD_R3_PATH_USER, F_OK) == 0 || access(DA_HTTPD_R3_PATH_SD, F_OK) == 0 || access(DA_HTTPD_R3_PATH_SYSTEM, F_OK) == 0)
#endif
		{

			l_net_exist = true;
#ifdef DEF_PAI_R_DATA_SAVE		
			pai_r_datasaver_start();
#endif
		}
	}
	else
		l_net_exist = false;
	
#if ENABLE_NETWORKING
	if(l_net_exist)
		streaming_start(0, 0);
#endif

		rec_cmd_thread = std::thread([&]() {
		RecorderCommand cmd;
		int camera_counts = 1;
//		bool is_sd_ok = false;
		
		do {
			{
				std::unique_lock<std::mutex> _l(cmdq_lock_);
				while(cmdq_.empty()) {
					cmdq_cond_.wait(_l);
				}
				cmd = cmdq_.front();
				cmdq_.pop_front();
			}

			if(cmd.cmd_ == kRecorderExitNow) {
				DLOG(DLOG_TRACE, "got kRecorderExitNow\r\n");
				break;
			}

#ifdef TARGET_CPU_V536	
			camera_counts = l_recorder_camera_counts;
#else
			camera_counts = rear_disabled_ ? 1 : 2;
#endif

			switch(cmd.cmd_) {
				case kStartRecording: {
					int err;

					if(is_record_start_good < 0 || power_disconnected || power_low_voltage){
						if(power_disconnected){
							DLOG(DLOG_WARN, "external power disconnected!\n");
						}
						else if(power_low_voltage){
							DLOG(DLOG_WARN, "external power low voltage!\n");
						}
						
						DLOG(DLOG_ERROR, "recording do not start!\n");
						is_record_start_good = 0;
						break;
					}
					else if(is_record_start_good++ < 10 || get_tick_count() < SAFETY_START_RECORDING_SECONDS) {//for cmd queue clear
						msleep(100);
						RecorderPostCommand(cmd.cmd_);
						break;
					}
#ifdef DEF_PAI_R_DATA_SAVE
					if(l_net_exist && !pai_r_datasaver_is_ready()){
						msleep(100);
						RecorderPostCommand(cmd.cmd_);
						break;
					}
#endif
						
					is_record_start_good = 0;
					
					if(l_is_recording) {
						DLOG(DLOG_ERROR, "already recording!\n");
						std::string currentRecFilePath;
						if( getCurrentRecordingFilePath(l_recorder_ref, currentRecFilePath) == 0 ){
							DLOG(DLOG_INFO, "Current Recording File Path : \"%s\"\r\n", currentRecFilePath.c_str());
						}
						
						break;
					}
					////++{** rear camera resolution check **********************************
					if(recorder_rear_camera_type_check())		
						break;
					////++}*************************************************************
					
					l_is_recording = true;
					
					if(rear_camera_detected == rear_disabled_ || l_recorder_ref == nullptr) {
						if(l_recorder_ref){
							destroyRecorder(l_recorder_ref);
							l_recorder_ref = nullptr;
							sleep(1);
						}
						
						if( rear_camera_detected ){				
							rear_disabled_ = false;		
							recorder_parameters_set(recorder_parameters, l_recorder_camera1_id, l_recorder_camera2_id, l_recorder_camera3_id, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path);
						}
						else {							
							rear_disabled_ = true;
							recorder_parameters_set(recorder_parameters, l_recorder_camera1_id, -1, -1, rotation, ext, l_rec_env, res_front, res_rear, l_rec_env.bAudioRecEnable, ini_path);
						}

						if(l_recorder_ref == nullptr){
							l_recorder_ref = createRecorder(VIDEO_NORMAL_DIR, recorder_parameters);
							DLOG(DLOG_TRACE, "recorder created %p\r\n", l_recorder_ref.get());
							if(l_recorder_ref == nullptr) {
								l_is_recording = false;
								cancel_handler(0);
								break;
							}
							sleep(1);
						}

#ifdef TARGET_CPU_V536	
						camera_counts = l_recorder_camera_counts;
#else
						camera_counts = rear_disabled_ ? 1 : 2;
#endif

						if(preview1) {
#if REAR_CAMERA_CHECK_ONFRAME
							recorder_set_pip_size( preview1, rear_camera_detected ? DISPLAY_PIP_SIZE : DISPLAY_PIP_FRONT_CAMERA_ONLY, G_sensorGetControl()->rotate == DX_ROTATE_180);
#else
							if( rear_camera_detected ){
								main_surface_is_front_camera = 1;
								ui::previewSwapSurfaces(preview1, l_recorder_camera1_id, l_recorder_camera2_id);
							}
							else {
								main_surface_is_front_camera = 2;
								ui::previewSwapSurfaces(preview1, l_recorder_camera1_id, -1);
							}
#endif
						}			
					}
				
					bool sniffing = false;
#if 0//ENABLE_MD
					ui::previewEnableMotionDetect(preview1, l_recorder_camera1_id, l_rec_env.bMotionRecEnable);

					if(l_recorder_camera2_id >= 0)
						ui::previewEnableMotionDetect(preview1, l_recorder_camera2_id, l_rec_env.bMotionRecEnable);

					if(l_recorder_camera3_id >= 0)
						ui::previewEnableMotionDetect(preview1, l_recorder_camera3_id, l_rec_env.bMotionRecEnable);
						
					if(l_rec_env.bMotionRecEnable){
						ui::previewSetMotionDetectSensitivityLevel(preview1, l_rec_env.motionSensi);
	
						sniffing = true;
					}
#endif

					snapshot_id_ = 0;

					sprintf(ma_strd_data_header_.ChannelInfo, "CH:%d", camera_counts);
					
					err = startRecording(l_recorder_ref, recording_observer, nullptr, sniffing, &ma_strd_data_header_, sizeof(ma_strd_data_header_));

					if(err < 0) {
						l_is_recording = false;
						mixwav_play(kMixwaveRecord_wa);
						mixwav_play(kMixwaveRecordStop);
						DLOG(DLOG_ERROR, "start recording failed\r\n");
						timer_running_ = false;
						tty_.tty_cancel();
					} else {
						if(l_rec_env.bMotionRecEnable)
							led_set_motion_recording_wait();
						else
							led_set_normal_recording_start();
						
						led_process_postCommand(LED_WORK_MODE, LED_WORK_NORMAL, LED_TIME_INFINITY);

						SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_REC_BEGIN ,0 , "M:%d CH:%d", l_rec_env.RecordingMode, camera_counts);
						DLOG(DLOG_TRACE, "recording started\n");

						if(l_net_exist)
							pai_r_datasaver_send_msg_to_httpd(QMSG_CHANNEL, camera_counts, 0, 0);

						mixwav_play(kMixwaveRecordStart);
					}
					break;
				}
				case kRestartRecording: {
#if 0
					if(l_is_recording) {
					
						if(l_normal_recording_type == Recorder_get_recording_file_type(false))
							break;

						stopRecording(l_recorder_ref);
						l_is_recording = false;

						postCommand(kStartRecording);
					}
#else //oasis-4.1.14 restartRecording api added

					if(l_is_recording) {						
						if(isRecorderRecording(l_recorder_ref) && l_normal_recording_type == Recorder_get_recording_file_type(l_rec_env.bMotionRecEnable)){
							is_record_restart_good = 0;
							break;
						}

						if(is_record_restart_good++ < 10) { //evdev???? 500 delay ???? ??, (5 + 10) * 100ms
							msleep(100);
							RecorderPostCommand(cmd.cmd_);
							break;
						}

						if(l_rec_env.bMotionRecEnable){
							postCommand(kStopMotionRecording);
							postCommand(kStartMotionRecording);
						}
						else	 {
							err = restartRecording(l_recorder_ref);
							if(err < 0) {
								DLOG(DLOG_ERROR, "restart recording failed\n");
							}
						}
					}

					is_record_restart_good = 0;
#endif					
					break;
				}
				case kStopRecording: {
					if(is_record_stop_good == 0 && is_record_start_good){
						is_record_start_good = -1;
						DLOG(DLOG_ERROR, "skip recording!\n");
					}
				
					if(l_is_recording){						
						if(is_record_stop_good++ < 3) {//for cmd queue clear
							if(l_net_exist && (snapshot_type_id || snapshot_id_ == 0))
								msleep(100);
							
							RecorderPostCommand(cmd.cmd_);
							break;
						}
						is_record_stop_good = 0;						

						
						//datool_led_red_set(l_is_recording);
						led_set_normal_recording_end();

						struct timespec ts;
						clock_gettime(CLOCK_MONOTONIC, &ts);
						SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_REC_END ,0 , "%d:%02d:%02d", ts.tv_sec / 3600, (ts.tv_sec % 3600) / 60, ts.tv_sec % 60 );
					
						if(stopRecording(l_recorder_ref) == 0) {
							
						}
						else if(l_recorder_ref){
							destroyRecorder(l_recorder_ref);
							l_recorder_ref = nullptr;
						}
						l_is_recording = false;
					}
						
					DLOG(DLOG_TRACE, "got kStopRecording, stopped? %d\r\n", l_is_recording);
					break;
				}
				case kChangeRecordingMode: {
					if(l_is_recording) {
						stopRecording(l_recorder_ref);
						l_is_recording = false;
					}

					l_is_parking_mode = !l_is_parking_mode;
					postCommand(kStartRecording);
					break;
				}
					break;
					
				case kStartEventInput1:
				case kStartEventInput2:
				case kStartEventRecording: 
				case kStartEventSuddenAcceleration:
				case kStartEventSuddenStart:
				case kStartEventSuddenDeacceleration:
				case kStartEventRapidLeft:
				case kStartEventRapidRight:
				case kStartEventOverSpeed:	
				case kStartEventFastSpeed:	
				case kStartEventGSensorLevelA:
				case kStartEventGSensorLevelB:
				case kStartEventGSensorLevelC:
				{
					if(power_disconnected){
						DLOG(DLOG_ERROR, "power off...\r\n");
						break;
					}
					if(l_is_recording == false || l_sd_exist == false) {
						DLOG(DLOG_ERROR, "not recording\r\n");
						break;
					}

					if(isEventRunning(l_recorder_ref)){
						DLOG(DLOG_INFO, "already event recording!\n");
						break;
					}
						
					if(l_sys_setup.cfg.iEventMode == 0) { // ??o?? ????
						DLOG(DLOG_WARN, "Continuance Recording Mode.\r\n");
						break;
					}

					time_t t;
					struct tm tm_t;
					//u16 time_ms = (get_tick_count()%1000);
					time(&t);
					localtime_r(&t, &tm_t);
					std::string file_path;

					char event_type = REC_FILE_TYPE_EVENT;
					
					if(cmd.cmd_ == kStartEventInput1)
						event_type = REC_FILE_TYPE_EVENT_INPUT1;
					else if(cmd.cmd_ == kStartEventInput2)
						event_type = REC_FILE_TYPE_EVENT_INPUT2;
#if ENABLE_DATA_LOG
					else
						l_datalog.m_event_recording = true;

					//l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_NO_EVENT;
#endif
					if(cmd.cmd_ >= kStartEventSuddenAcceleration && cmd.cmd_ <= kStartEventGSensorLevelC){
						//if(cmd.cmd_ < kStartEventGSensorLevelA)
						//	event_type = REC_FILE_TYPE_EVENT_WARNING;
#if defined(DEF_SAFE_DRIVING_MONITORING) && ENABLE_DATA_LOG
						switch (cmd.cmd_) {
							case 	kStartEventSuddenAcceleration:
							case 	kStartEventSuddenStart:	
								l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_RAPID_ACCELERATION;
								l_datalog.m_event_cause_difference = safe_driving_get_diff_speed();
								break;
							case 	kStartEventSuddenDeacceleration:	
								l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_SUDDEN_DECELERATION;	
								l_datalog.m_event_cause_difference = safe_driving_get_diff_speed();
								break;
							case 	kStartEventRapidLeft:
							case 	kStartEventRapidRight:	
								l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_SUDDEN_HANDLE;
								l_datalog.m_event_cause_difference = safe_driving_get_rotate_value();
								break;
							case 	kStartEventOverSpeed:	
								l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_OVERSPEED;	
								l_datalog.m_event_cause_difference = safe_driving_get_overspeed_count();
								break;
							case 	kStartEventFastSpeed:	
								l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_FASTSPEED;	
								l_datalog.m_event_cause_difference = safe_driving_get_overspeed_count();
								break;
							//default:	l_datalog.m_event_cause_category = EVENT_CAUSE_CATEGORY_OTHERS;	break;
						}						
#endif
						file_path = Recorder_get_recording_file_name_pai_r(VIDEO_EVENT_DIR, tm_t, event_type, camera_counts, ext, cmd.cmd_);
						//file_path = Recorder_get_recording_file_name_pai_r(VIDEO_EVENT_DIR, tm_t, event_type, rear_disabled_ ? 1 : 2, ext, cmd.cmd_);
					}
					else {
						//file_path = Recorder_get_recording_file_name(VIDEO_EVENT_DIR, tm_t, event_type, rear_disabled_ ? 1 : 2, ext);
						file_path = Recorder_get_recording_file_name(VIDEO_EVENT_DIR, tm_t, event_type, camera_counts, ext);
					}
					
					if(startEventRecording(l_recorder_ref, file_path.c_str(), &ma_strd_data_header_, sizeof(ma_strd_data_header_), auto_stop_and_restart_normal_recording_) < 0) {
						DLOG(DLOG_ERROR, "start event recording failed!\r\n");
					}else {
						u32 start_tick = get_tick_count();
						datool_ext_trigger_out_set(1); // 1??? High
						mixwav_play(kMixwaveEventRecording);

#ifdef DEF_PAI_R_DATA_SAVE
						if(l_net_exist){
							struct timeval timestamp;
							std::vector<char> image_data;
							
							snapshot_parameters_set(parameters, l_rec_env.front_resolution);
							parameters["timeout"] = "950";
							
							if(oasis::takePicture(l_recorder_camera1_id, parameters, image_data, &timestamp) < 0) {
								//"Request Timeout"
							} else {
								FILE *fp = fopen("/tmp/snapshot-event.jpg", "wb");
								if(fp) {
									fwrite(image_data.data(), 1, image_data.size(), fp);
									fclose(fp);
								} else {
									DLOG(DLOG_ERROR, "%s file creation failed: %d(%s)\n", __func__, errno, strerror(errno));
								}
							}
		
							pai_r_datasaver_addLocation_new(file_path, true, PRE_RECORDING_SECONDS);

							postCommand(kTakeFrontEventSnapshot);
						}
#endif

						while(get_tick_count() < start_tick + 1000){
							msleep(1);
						}
						
						datool_ext_trigger_out_set(0);
						//test only
						//postCommand(kStopEventRecording);
					}
					break;
				}
				case kStopEventRecording: {
					DLOG(DLOG_INFO, "event is running?%d, recording?%d\n", isEventRunning(l_recorder_ref), isEventRecording(l_recorder_ref));
					if(l_recorder_ref && /*isEventRecording(l_recorder_ref)*/ isEventRunning(l_recorder_ref)) {
						stopEventRecording(l_recorder_ref);
					}
					break;
				}
				case kStartMotionRecording: {
					if(l_is_recording == false || l_sd_exist == false) {
						DLOG(DLOG_ERROR, "not recording\n");
						break;
					}
					else if(isMotionRecording(l_recorder_ref)){
						DLOG(DLOG_INFO, "already motion recording!\n");
						break;
					}
					std::string file_path;
					
					time_t t;
					struct tm tm_t;
					time(&t);
					localtime_r(&t, &tm_t);

					l_normal_recording_type = Recorder_get_recording_file_type(true);
					
					file_path = Recorder_get_recording_file_name(VIDEO_MOTION_DIR, tm_t, l_normal_recording_type, camera_counts, ext);
					
					if(startMotionRecording(l_recorder_ref, file_path.c_str(), &ma_strd_data_header_, sizeof(ma_strd_data_header_)) < 0) {
						DLOG(DLOG_WARN, "start motion recording failed!\n");
					}else {
#ifdef DEF_PAI_R_DATA_SAVE
						if(l_net_exist){
							pai_r_datasaver_addLocation_new(file_path, false, PRE_RECORDING_SECONDS);

							postCommand(kTakeFrontSnapshot);
						}
#endif
					}
					break;
				}
				case kStopMotionRecording: {
					if(isMotionRecording(l_recorder_ref)) {
						stopMotionRecording(l_recorder_ref);
					}
					break;
				}

				case kExternalPowerGood : {
					DLOG(DLOG_WARN, "external power connected!\n");

					if(power_disconnected){
#if 0 // ???? Off/On?? ????? ?????? ??? ??????? ????
#if DEF_SUB_MICOM_USE		
						double volt = l_sub_mcu.getBatteryVoltage();
						if(volt < (double)LOW_BATTERY_MIN_VOLT / 10.0)
							break;
						
						SYSTEMLOG(LOG_TYPE_BATTERY, LOG_EVENT_BATTCHANGE, eBB_BATT_STATE_NORMAL, "%0.1f V", volt);
#endif
						power_disconnected = false;
						sleep(3);
						postCommand(kStartRecording);
#endif						
					}
					break;
				}
				case kExternalPowerFall : {
					if(power_disconnected == false){
						DLOG(DLOG_WARN, "external power disconnected!\n");

#if DEF_SUB_MICOM_USE					
						SYSTEMLOG(LOG_TYPE_BATTERY, LOG_EVENT_BATTCHANGE, eBB_BATT_STATE_FALL, "%0.1f V", l_sub_mcu.getBatteryVoltage());
#endif
						power_disconnected = true;
						if(l_is_recording){
							mixwav_play(kMixwaveRecordStop);
							mixwav_play(kMixwave10SecBlocking);
							if(l_sys_setup.cfg.iVideoQuality == CFG_VIDEO_QUALITY_HIGH)		//add_250306
								sleep(1);	
							else			
								sleep(2);
							
							postCommand(kStopRecording);

							pai_r_datasaver_emergency_backup();
						}			
					}
					break;
				}
				case kSwapSurfaces: {
					if(main_surface_is_front_camera) {
						if(main_surface_is_front_camera == 1){
							main_surface_is_front_camera = 2;
							ui::previewSwapSurfaces(preview1, 0, -1);
						}else if(main_surface_is_front_camera == 2){
							main_surface_is_front_camera = 3;
							ui::previewSwapSurfaces(preview1, 1, -1);
						}else {
							main_surface_is_front_camera = false;
							ui::previewSwapSurfaces(preview1, 1, 0);
						}
					} else {
						main_surface_is_front_camera = true;
						ui::previewSwapSurfaces(preview1, 0, 1);
					}
					break;
				}
				case kTakeFrontSnapshot:
				case kTakeFrontEventSnapshot : {
					snapshot_parameters_set(parameters, l_rec_env.front_resolution);
					
#if 0
					std::vector<char> image_data;
					struct timeval timestamp;
					if(oasis::takePicture(l_recorder_camera1_id, parameters, image_data, &timestamp) == 0 && image_data.size() > 0) {
						time_t t = (time_t)timestamp.tv_sec;
						struct tm tm_t;
						localtime_r(&t, &tm_t);
						char path[512];
						sprintf(path, "/tmp/mnt/front-snapshot-%4d%02d%02d-%02d%02d%02d.jpg", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
						FILE *fp = fopen(path, "wb");
						if(fp) {
							fwrite(image_data.data(), 1, image_data.size(), fp);
							fclose(fp);
							DLOG(DLOG_INFO, "%s saved %d bytes.\n", path, image_data.size());
						} else {
							DLOG(DLOG_ERROR, "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
						}
					}
#else
					if(l_is_recording == false || timer_running_ == false) {
						DLOG(DLOG_ERROR, "not recording\r\n");
						break;
					}
 #ifdef DEF_PAI_R_DATA_SAVE
 					if(l_net_exist){
	 					if(!recorderIsRunning(l_recorder_ref)){
							DLOG(DLOG_ERROR, "not recording %d %d\r\n", snapshot_id_, isRecorderRecording(l_recorder_ref));
							//sleep(1);
							break;
	 					}

						if(recorderIsRunning(l_recorder_ref)){
							if(snapshot_type_id){
								if(snapshot_retry++ < 2){
									sleep(1);
									postCommand(cmd.cmd_);
								}
								break;
							}
	
							snapshot_retry = 0;

							snapshot_id_++;
							
	  						snapshot_type_id = snapshot_id_ * 2;

							if(cmd.cmd_ == kTakeFrontEventSnapshot)
								snapshot_type_id |= DEF_SNAPSHOT_TYPE_MASK;
							
	 						takeRecordingShapshot(l_recorder_ref, snapshot_type_id, l_recorder_camera1_id, parameters);
 #if 0							
							if(rear_disabled_ == false){
								sleep(1);
								takeRecordingShapshot(l_recorder_ref, snapshot_type_id + 1, l_recorder_camera2_id, parameters);
							}
 #endif							
						}
 					}
 #else
					takeRecordingShapshot(l_recorder_ref, snapshot_id_++, l_recorder_camera1_id, parameters);
 #endif
#endif
					break;
				}
			case kTakeRearSnapshot: {
				if(rear_disabled_) {
					if(snapshot_type_id)
						snapshot_type_id = 0;
					else
						DLOG(DLOG_ERROR, "rear camera disabled\n");
					break;
				}
				snapshot_parameters_set(parameters, l_rec_env.rear_resolution);
				
				if(is_record_stop_good || l_is_recording == false || timer_running_ == false) {
					snapshot_type_id = 0;
					DLOG(DLOG_ERROR, "not recording\r\n");
					break;
				}
				takeRecordingShapshot(l_recorder_ref, snapshot_type_id + 1, l_recorder_camera2_id, parameters);
				snapshot_type_id = 0;
				break;
			}
			
#if (defined(DEF_VRSE_SECURITY_FILE_EXT) && ENABLE_AVI_SCRAMBLING == 0)
			case kRecordingCompleted : 
			case kRecordingCompleted2 : 
			case kRecordingCompletedMotion :
			case kRecordingCompletedMotion2 : 
			case kRecordingCompletedNormal :
			case kRecordingCompletedNormal2 : {
				//const char * riff_hd = { 0x52, 0x49, 0x46, 0x46, 0xf8, 0xff, 0xbf, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4c, 0x49, 0x53, 0x54 };
				const char * fvfs_hd = DEF_VRSE_FVFS_HEADER;
				u32 idx = 0, retry = 0;
				char buf[32] = {0, };
				char path[128] = { 0, };

				if( cmd.cmd_ >= kRecordingCompletedMotion) {
					if(cmd.cmd_ >= kRecordingCompletedNormal) {
						idx = cmd.cmd_ - kRecordingCompletedNormal;
					}
					else {
						idx = cmd.cmd_ - kRecordingCompletedMotion;
					}

					if(oasis::fs::offsIsBusy()) {
						msleep(500);
						replace_offsbusy++;
						if(replace_offsbusy++ > 20){
							DLOG(DLOG_WARN, "OFFS Busy!! (%s)\r\n", l_st_motion_recording_file_path.path[idx]);
						}
						else {
							postCommand(cmd.cmd_);
							break;
						}
					}
					replace_offsbusy = 0;
					
					strcpy(path, (const char *)l_st_motion_recording_file_path.path[idx]);
					strcpy(l_st_motion_recording_file_path.path[idx], "");
				}else {
					idx = cmd.cmd_ - kRecordingCompleted;
					strcpy(path, (const char *)l_st_recording_file_path.path[idx]);
					strcpy(l_st_recording_file_path.path[idx], "");
				}

				do{
					sysfs_replace((const char *)path, fvfs_hd, 16, 0); //RIFF => FVFS
					sysfs_read((const char *)path, buf, 16);
					
					if(strcmp(fvfs_hd, buf) == 0)
						break;

					msleep(100);

					if(retry)
						DLOG(DLOG_WARN, "file replace %s [%s], retry %d \r\n", path, buf, retry);
					
				}while(retry++ < 10);
				
				break;
			}
#endif			
			case kSDCardOk: {
				//is_sd_ok = true;
				break;
			}
			case kSDCardError : {
				//is_sd_ok = false;
				break;
			}
				default:
					break;
			}

		} while(timer_running_);

		{
			std::lock_guard<std::mutex> _l(cmdq_lock_);
			cmdq_.clear();
		}

		if(l_is_recording) {
			if(isEventRecording(l_recorder_ref)) {
				stopEventRecording(l_recorder_ref);
			}
			stopRecording(l_recorder_ref);
		}
	});


	////////////////////////////////////////////////////////////////////////////////////////////
	rec_text_thread = std::thread([&]() {
		time_t t;
		struct tm tm_t;
		uint64_t usec;
		uint64_t preview_usec = 0, rec_restart_preview_usec = 0;
		std::string title;
		int rear1_error = 0;
		int rear1_okcnt = 0;
		int rear2_error = 0;
		int rear2_okcnt = 0;
		
#if ENABLE_DATA_LOG
		time_t pre_log_update_time;

		const char * driver_code_path = DEF_DRIVER_CODE_FILE_PATH;
		char driver_code[64];
		strcpy(driver_code, "0");
		
		if(access(driver_code_path, R_OK ) == 0){
			if(sysfs_scanf(driver_code_path, "%s", driver_code)){
				l_datalog.set_driver_code(driver_code );
			}
		}
		
		l_datalog.set_serial_number(l_rec_env.serial_no);
		
#endif
//		char stream_data[512] = { 0, };

		preview_usec = systemTime();
		do {
			CPulseData pulse = evdev_input_get_pulsedata();
			
			usec = systemTime();

			time(&t);
			localtime_r(&t, &tm_t);
			
#if ENABLE_DATA_LOG
			if(l_sys_setup.isInit() && pre_log_update_time != t){
				pre_log_update_time = t;

				Recorder_datalog_add(t);		//Use to the OnFrame_250304
				
				if(tm_t.tm_sec % 10 == 9){ // 10??? ?? ???? ????
					if(!power_low_voltage && !power_disconnected)
						l_datalog.save_log();
				}
			}
#endif

#if 0
			if (recorderIsRunning(l_recorder_ref)) {
				sprintf(stream_data, "Main %4d/%02d/%02d %02d:%02d:%02d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
				addRecordingVideoStreamData(l_recorder_ref, (void*)stream_data, strlen(stream_data) + 1);
			}
#endif

			if(l_is_recording && recorderIsRunning(l_recorder_ref)) {
#if 0 //DEF_PULSE_DEBUG				

				title = oasis::format("[1] %4d/%02d/%02d %02d:%02d:%02d G:%03d, PS:%03d, PC:%d [%d %d %d %d %d]", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec, \
					(int)pulse.m_fGpsSpeed, (int)pulse.m_fPulseSpeed, (int)pulse.m_iPulseSec, \
					(int)pulse.m_bBrk, (int)pulse.m_bSR, (int)pulse.m_bSL, (int)pulse.m_bBgr, (int)pulse.m_bTR);
#else	
				title = oasis::format(" %4d/%02d/%02d %02d:%02d:%02d",  tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);

				if(l_sys_setup.cfg.bOsdSpeed) {
					if(pulse.m_bPulseState && pulse.m_pulse_count_accrue)
				 		title.append(oasis::format("                                                         %03dkm/h", (int)pulse.m_fPulseSpeed));//OSD_changed the position
					else if(pulse.m_bGpsSignalState)
						title.append(oasis::format("                                                         %03dkm/h", (int)pulse.m_fGpsSpeed));//OSD_changed the position
				}

				if(power_disconnected){
					title.append(oasis::format("              POWER OFF", (int)pulse.m_fGpsSpeed));
				}
				
				if(l_sys_setup.cfg.bOsdRpm && pulse.m_rpm_count_accrue)
					title.append(oasis::format("  %04drpm", pulse.m_iRpm));

				evdev_input_pulse_data_add_stream();
#endif

				if(streaming_is_rtsp_play()){
					setRecordingText(l_recorder_ref, l_recorder_camera1_id, kTextTrackOsd, std::string(""), usec);
					//setRecordingText(l_recorder_ref, l_recorder_camera2_id, kTextTrackOsd, std::string(""), usec);
				}
				else {
					setRecordingText(l_recorder_ref, l_recorder_camera1_id, kTextTrackOsd, title, usec);
#if 0 // 0.0.20 deleted
					if(rear_disabled_ == false) {
						title.replace(1, 1, "2");
						//title = oasis::format("[2] %4d/%02d/%02d %02d:%02d:%02d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
						setRecordingText(l_recorder_ref, l_recorder_camera2_id, kTextTrackOsd, title, usec);
					}
#endif		
				}
			}


			if(systemTime() - preview_usec >= 1000000) {
#if !REAR_CAMERA_CHECK_ONFRAME			
/////////////////////////////////////////////////////////////////////////////////////////////////				
				//printf("RearCAM 2,3 = detect (%s)\r\n", rear_camera_detected ? "true" : "false");
				//printf("rear_disable (%s)\r\n", rear_disabled_ ? "true" : "false");
				
					if(tm_t.tm_sec % 2 == 0){
						if(recorder_camera_check(2, l_rec_env.rear_resolution)){
							
							if(l_recorder_camera2_id == -1 && rear1_okcnt++ >= 4){
								
								rear_camera_detected = true;
								rear_disabled_ = true;
								l_recorder_camera2_id = 2;
								l_recorder_camera_counts++;
								rear1_okcnt = 0;				
							}
						}
						else if(l_recorder_camera2_id != -1 && rear1_error++ >= 4){
							
							if(l_recorder_camera_counts == 2)
								rear_camera_detected = false;
							
							rear_disabled_ = rear_camera_detected; // restart recording
							l_recorder_camera2_id = -1;
							l_recorder_camera_counts--;
							rear1_error = 0;
						}
					}

					else{	
						if(recorder_camera_check(3, l_rec_env.rear_resolution)){
							
							if(l_recorder_camera3_id == -1 && rear2_okcnt++ >= 5){
								
								rear_camera_detected = true;
								rear_disabled_ = true;
								l_recorder_camera3_id = 3;
								l_recorder_camera_counts++;
								rear2_okcnt = 0;
							}
						}
						else if(l_recorder_camera3_id != -1 && rear2_error++ >= 5){
							
							if(l_recorder_camera_counts == 2)
								rear_camera_detected = false;
							
							rear_disabled_ = rear_camera_detected; // restart recording
							l_recorder_camera3_id = -1;
							l_recorder_camera_counts--;
							rear2_error = 0;
						}
					}
				
				
/////////////////////////////////////////////////////////////////////////////////////////////////				
#endif		
				//printf("lowbattery = %s, disconnection = %s\r\n", power_low_voltage, power_disconnected);
 #ifdef DEF_REAR_CAMERA_ONLY
				if((tm_t.tm_sec % 10) == 0){
					recorder_rear_camera_type_check();
				}
#endif
				if(tm_t.tm_hour == 3 && tm_t.tm_min== 0 && tm_t.tm_sec == 0){
					//??翡 ?? ?? RTC?ð????? ????
					system("hwclock --hctosys");
				}
				
				preview_usec = systemTime();

				if(l_is_recording){
	 #if DEF_PULSE_DEBUG
					CPulseData pulse = evdev_input_get_pulsedata();

					title = oasis::format("%4d/%02d/%02d %02d:%02d:%02d %d %d %d %d %d G:%03d P:%03d R:%d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec, \
						(int)pulse.m_bBrk, (int)pulse.m_bSR, (int)pulse.m_bSL, (int)pulse.m_bBgr, (int)pulse.m_bTR, \
						(int)pulse.m_fGpsSpeed, (int)pulse.m_fPulseSpeed, pulse.m_iRpm);

					if(preview1){
	 					ui::setLabelText(l_osd_date_label, title.c_str());
					}
					
	 				title = oasis::format("%4d/%02d/%02d %02d:%02d:%02d [Br:%d R:%d L:%d Bg:%d T:%d] G:%03d, PS:%03d, PC:%03d, RPM:%04d, RC:%03d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec, \
						(int)pulse.m_bBrk, (int)pulse.m_bSR, (int)pulse.m_bSL, (int)pulse.m_bBgr, (int)pulse.m_bTR, \
						(int)pulse.m_fGpsSpeed, (int)pulse.m_fPulseSpeed, (int)pulse.m_iPulseSec, pulse.m_iRpm, pulse.m_iRpmPulseSec);
	#if DEF_SUB_MICOM_USE
					title.append(oasis::format(" VOLT:%0.1f", l_sub_mcu.m_volt));
	#endif
					DLOG(DLOG_WARN | DLOG_RECORD, "%s \r\n", title.c_str());
	 #else
	 				if(preview1){
						title = oasis::format("%4d/%02d/%02d %02d:%02d:%02d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
		 				ui::setLabelText(l_osd_date_label, title.c_str());
	 				}
 	#endif

					if(rear_camera_detected == rear_disabled_ && systemTime() - rec_restart_preview_usec >= 5000000){ //rear camera status changed
						postCommand(kStopRecording);
						postCommand(kStartRecording);
						rec_restart_preview_usec = systemTime();
						//recorder_reboot();
					}
				}
 
#if ENABLE_ADAS
				if(preview1)
					ui::previewAdasSetGsensorSpeed(preview1, false, 0, 4.0);
#endif
				if(l_sys_setup.cfg.bTestMode)
					recorder_test_mode_processer();
			}

			//40 msec
			//usleep(40000);
			usleep(500000);

		} while (timer_running_);
		
	});

#if DEF_DEBUG_SERIAL_LOG_FILE_SAVE
	while (timer_running_) usleep(500000);
#else
	recorder_tty_working(preview1);
#endif

//////////////////////////////////////////////////////
#ifdef DEF_PAI_R_DATA_SAVE
	if(l_net_exist){
		pai_r_datasaver_end();
	}
#endif

#if DEF_SUB_MICOM_USE
	SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_SYSEND ,0 , "%0.1f V", l_sub_mcu.getBatteryVoltage());
#else
	SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_SYSEND ,0 , "%u", get_tick_count());
#endif
	timer_running_ = false;
	msleep(100);

	if(adas){
		adasStop(adas);
	}

	if (rec_text_thread.joinable()) {
		rec_text_thread.join();
	}

	if (recorder_thread.joinable()) {
		recorder_thread.join();
 	}
	
	if (rec_cmd_thread.joinable()) {
		rec_cmd_thread.join();
	}
	
#if ENABLE_NETWORKING
	if(l_net_exist)
		streaming_end();
#endif

	if(l_recorder_ref)
		destroyRecorder(l_recorder_ref);

recorder_creation_failed:
	
	if(preview1) {
		ui::previewStop(preview1);
	}

	if(adas) {
		adasStop(adas);
		destroyAdas(adas);
	}

	if(l_rec_env.bMotionRecEnable){
		for(const MotionDetectorRef &motion_detector : motion_detectors) {
			motionDetectorStop(motion_detector);
		}

		while(motion_detectors.empty() == false) {
			MotionDetectorRef motion_detector = motion_detectors.front();
			motion_detectors.pop_front();
			destroyMotionDetector(motion_detector);
		}
	}

config_failed:
	
	ui::cleanup();

//	if (t.joinable()) {
//		t.join();
//	}
	oasis::finalize();

	if(t_watcher.joinable()){
		t_watcher.join();
	}

	printf("goodbye.\n");
	return 0;
}

