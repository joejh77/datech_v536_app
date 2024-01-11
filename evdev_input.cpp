
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
 #include <unistd.h> //include for sleep function
 
#include <libevdev-1.0/libevdev/libevdev.h>
#include <sysfs.h>

//
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "daversion.h"
#include "datools.h"
#include "dagpio.h"
#include "sysfs.h"
#include "SB_System.h"
#include "gsensor.h"
#include "serial.h"
#include "bb_api.h"
#include "system_log.h"
#include "pai_r_datasaver.h"
#include "pai_r_data.h"
#include "led_process.h"
#include "mixwav.h"
#include "evdev_input.h"
#include "recorder.h"

#define BLUE_LED_CONTROL 		0 // 0 is msg queue use

#define ZONE_EVDEV_FUNC			1
#define ZONE_EVDEV_ERROR		1

#define USE_KEY 				1

#define USE_GSENSOR				1
#define USE_GPS 				1

#define DEF_DC_DET_CHATTERING_TIME			750	//1250 //2500

#define DEF_TURNON_CHATTERING_TIME			200
#define DEF_INDICATORS_CHATTERING_TIME		500
#define DEF_TURN_SIGNAL_CHATTERING_TIME		(in_param.iBlink * 500 + 500)

#define GPS_BAUDRATE 						9600
  /* COM1="/dev/ttyS1, COM2="/dev/ttyS2 */


//#define GPS_OPEN()		open_serial(GPS_DEVICE, GPS_BAUDRATE, 1, 0, 8, 1)

#define GSEND_STREAM_DATA_INTERVAL_TIME		100 //ms

#define KEY_CODE_TRG_I			0
#define KEY_CODE_BRK			1
#define KEY_CODE_SIG_R			2
#define KEY_CODE_SIG_L			3
#define KEY_CODE_GEAR_R			4
#define KEY_CODE_DC_DET			5

#define KEY_CODE_MAX			6

/*
EV_SW Event type
Event: 0(SW_LID)
Event: 1(SW_TABLET_MODE)
Event: 2(SW_HEADPHONE_INSERT)
Event: 3(SW_RFKILL_ALL)
Event: 4(SW_MICROPHONE_INSERT)
Event: 5(SW_DOCK)
Event: 6(SW_LINEOUT_INSERT)
Event: 7(SW_JACK_PHYSICAL_INSERT)
Event: 8(SW_VIDEOOUT_INSERT)
Event: 9(SW_CAMERA_LENS_COVER)
Event: 10(SW_KEYPAD_SLIDE)
Event: 11(SW_FRONT_PROXIMITY)
Event: 12(SW_ROTATE_LOCK)
Event: 13(SW_LINEIN_INSERT)
Event: 14(SW_MUTE_DEVICE)
Event: 15(SW_MAX)

//// V536
gpio_pin_1              =  port:PG02<0><3><3><1> //TRG_I
gpio_pin_2              =  port:PG03<0><3><3><1> //GEAR_R
gpio_pin_3              =  port:PG04<0><3><3><1> //BRK
gpio_pin_4              =  port:PG05<0><3><3><1> //SIG_R
gpio_pin_5              =  port:PG06<0><3><3><1> //SIG_L
gpio_pin_6              =  port:PH03<0><3><3><1> //DC_DET
gpio_pin_7              =  port:PH04<0><3><3><1> //GSENSOR

//// I3
[gpio_keys]
; keyN_type: 1 = EV_KEY, 3 = EV_ABS, 5 = EV_SW. Refer to linux/input.h for detail.
; keyN_code: input event code (KEY_*, SW_*)
; code 167:KEY_RECORD, 2:KEY_1

key0_type   = 5
; TRG_I
key0_code   = 0
key0_gpio   = port:PB02<6><default><default><default>
key0_debounce = 100

key1_type   = 5
; BRK
key1_code   = 1
key1_gpio   = port:PB03<6><default><default><default>
key1_debounce = 50

key2_type   = 5
; SIG_R
key2_code   = 2
key2_gpio   = port:PB05<6><default><default><default>
key2_debounce = 50

key3_type   = 5
; SIG_L
key3_code   = 3
key3_gpio   = port:PB011<6><default><default><default>
key3_debounce = 50

key4_type   = 5
; GEAR_R
key4_code   = 4
key4_gpio   = port:PB013<6><default><default><default>
key4_debounce = 50

key5_type   = 5
; DC_DET
key5_code   = 5
key5_gpio   = port:PG08<6><default><default><default>
key5_debounce = 200

key6_type   = 4
; SPD_P2
key6_code   = 0
key6_gpio   = port:PG00<6><default><default><default>
key6_debounce = 0

*/

enum {
	EVENT_SW_TRG_I = 0,
	EVENT_SW_BRK,
	EVENT_SW_SIG_R,
	EVENT_SW_SIG_L,
	EVENT_SW_GEAR_R,
	EVENT_SW_DC_DET,

	EVENT_SW_END
};


//
// libevdev를 써서 입력 장치로부터 이벤트를 받아 원하는 이벤트인 경우 `sysfs` API를 써서
// GPIO 및 LED를 제어하도록 하는 예제.
//

static std::thread input_thread;
static std::thread input_gsensor_thread;

static bool evdev_thread_run = false;
static bool evdev_gsensor_thread_run = false;

static INPUT_PARAM in_param = { 0, };
static CGPSData l_gps;
static CPulseData l_pulse_data;
static char l_pre_pulse_data[128] = { 0, };
static bool l_preKeyValue[KEY_CODE_MAX] = { 0,};

static u32 l_pulse_tmout_ms = 0;
static u32 l_rpm_tmout_ms = 0;
		 
/*
/dev/input/event1 axp20-supplier
/dev/input/event2 bma250
/dev/input/event3 ht-gpio-keys

/dev/input/event1 sunxi-keyboard
/dev/input/event2 axp20-supplier
/dev/input/event3 bma250
/dev/input/event4 ht-gpio-keys


new board
/dev/input/event0 pulse_count
/dev/input/event1 sunxi-keyboard
/dev/input/event2 bma250
/dev/input/event3 ht-gpio-keys

*/


int evdev_input_gps_time_sync_check(CGPSData &gps, int gmt);

int __evdev_get_pulse_count(int input_no, u32 *av)
{
	int fd = NULL;
	char cmd[128];
	char dev_name[128];
	char event_name[128];

	sprintf(cmd, "/sys/class/input/input%d/pulse_count", input_no);
	
	if(sysfs_scanf(cmd, "%u,%u,%u,%u", &av[0], &av[1], &av[2], &av[3])){
		return 1;
	}

	return 0;
}

// return fd
int __evdev_input_open(const char * name, int &default_open_no)
{
	int fd = NULL;
	int input_number = default_open_no;
	char cmd[128];
	char dev_name[128];
	char event_name[128];

	sprintf(cmd, "/sys/class/input/input%d/name", input_number);
	
	if(sysfs_scanf(cmd, "%s", event_name)){
		if(strcmp(name, event_name) == 0){

			sprintf(dev_name, "/dev/input/event%d", input_number);
			
			fd = open(dev_name, O_RDONLY|O_NONBLOCK);

			if (fd > 0)
				return fd;
		}
	}

	// search
	sprintf(cmd, "grep -r \"%s\" /sys/class/input/input*", name);
	
	if(SB_CmdScanf(cmd, "/sys/class/input/input%d/", &input_number)){
		sprintf(dev_name, "/dev/input/event%d", input_number);
		
	 	fd = open(dev_name, O_RDONLY|O_NONBLOCK);

		 if (fd > 0){
		 	default_open_no = input_number;
		 	dprintf(ZONE_EVDEV_FUNC, "%s:%s open OK\n", dev_name,name);
		 }
		 else{
		 	dprintf(ZONE_EVDEV_ERROR, "%s open failed: %d(%s)\n", dev_name, errno, strerror(errno));
		 }
	}
	else {
		dprintf(ZONE_EVDEV_ERROR, "%s do not find\n", name);
	}
	return fd;
}

//
bool __evdev_new_from_fd_has_event_code(int fd, struct libevdev **dev, u32 type, u32 code)
{
	int rc = -EAGAIN;
	
	 rc =  libevdev_new_from_fd(fd, dev);
    if (rc < 0) {
        dprintf(ZONE_EVDEV_ERROR, "Failed to init libevdev (%s)\n", strerror(-rc));
        return false;
    }
    dprintf(ZONE_EVDEV_FUNC, "Input device name: \"%s\"\n", libevdev_get_name(*dev));
    dprintf(ZONE_EVDEV_FUNC, "Input device ID: bus %#x vendor %#x product %#x\n",
            libevdev_get_id_bustype(*dev),
            libevdev_get_id_vendor(*dev),
            libevdev_get_id_product(*dev));

    if (!libevdev_has_event_code(*dev, type, code)) {
		libevdev_free(*dev);
       dprintf(ZONE_EVDEV_ERROR, "This device does not have type:%s, code:%s \n", libevdev_event_type_get_name(type), libevdev_event_code_get_name(type, code));
       return false;
    }

	return true;
}


void _evdev_input_key_data_set(int type , BOOL value, BOOL init)
{
	BOOL key_data;

	if(type <  KEY_CODE_MAX) {
		if(l_preKeyValue[type] == value && !init)
			return;
		
		l_preKeyValue[type] = value;
	}
	
	switch(type){
		
#ifdef DEF_DO_NOT_USE_DIGITAL_IO
	//DA-220S모델은 Digital IO와 Pulse Signal을 사용하지 않음

#else
		case KEY_CODE_TRG_I: 		
			l_pulse_data.m_current_input1 = (value) != in_param.bInput1Pu;
			
			if(l_pulse_data.m_current_input1){					
				l_pulse_data.m_chattering_tm_trg_signal = get_tick_count();
			}
			else {
				if(!l_pulse_data.m_bTR) { // 1초전이라면 Event
					if(is_time_out_ms(&l_pulse_data.m_chattering_tm_trg_signal, 100)){
						if(!init){
							RecorderPostCommand(kStartEventInput1);
							l_pulse_data.m_bInput1Evt = TRUE;
						}
					}
					l_pulse_data.m_chattering_tm_trg_signal = 0;
					
				}
				else{
					l_pulse_data.m_chattering_tm_trg_signal = get_tick_count();
				}
			}
			
			break;

		case KEY_CODE_GEAR_R:	

			l_pulse_data.m_current_input2 = (value) != in_param.bInput2Pu;
			
			if(l_pulse_data.m_current_input2){					
				l_pulse_data.m_chattering_tm_bgr_signal = get_tick_count();					
			}
			else {
				if(!l_pulse_data.m_bBgr){ // 1초전이라면 Event
					if(is_time_out_ms(&l_pulse_data.m_chattering_tm_bgr_signal, 100)){
						if(!init){
							RecorderPostCommand(kStartEventInput2);
							l_pulse_data.m_bInput2Evt = TRUE;
						}
					}
					l_pulse_data.m_chattering_tm_bgr_signal = 0;						
				}
				else {
					l_pulse_data.m_chattering_tm_bgr_signal = get_tick_count();
				}
			}
			break;
			
		case KEY_CODE_BRK: 			
			l_pulse_data.m_current_bBrk = (value) != in_param.bBrakePu;
			l_pulse_data.m_chattering_tm_brk_signal = get_tick_count();
			break;
			
		case KEY_CODE_SIG_R:		
			l_pulse_data.m_current_bSR = value;
			l_pulse_data.m_chattering_tm_turn_signal_right = get_tick_count();
			break;
			
		case KEY_CODE_SIG_L:		
			l_pulse_data.m_current_bSL = value;
			l_pulse_data.m_chattering_tm_turn_signal_left = get_tick_count();			
			break;
#endif

		case KEY_CODE_DC_DET: 	
			l_pulse_data.m_bDcDet = value;
			if(value){
				u32 tick = get_tick_count();

				if(tick < 35000 || (tick - l_pulse_data.m_chattering_tm_DC_Det) < 35000) //// 20sec no charged supper capacitor
					tick -= (DEF_DC_DET_CHATTERING_TIME - 250);
				else if(tick < 40000 || (tick - l_pulse_data.m_chattering_tm_DC_Det) < 40000)
					tick -= (DEF_DC_DET_CHATTERING_TIME - 250) / 2;
				
				l_pulse_data.m_chattering_tm_DC_Det = tick;
				
				//RecorderPostCommand(kExternalPowerFall);
			}
			else{
				//l_pulse_data.m_chattering_tm_DC_Det = 0;
				RecorderPostCommand(kExternalPowerGood);
			}
			break;
		}
}

void _evdev_input_pulse_data_set(u32 type , u32 value, BOOL init)
{
#if 0
		printf("EV_REL : %d(%s) %d\n", type, libevdev_event_code_get_name(EV_REL, type), value);
#endif

	 switch (type) {
		case REL_X:
#if 0			// for test					
			l_pulse_data.m_bGpsSignalState = true;
			l_pulse_data.m_fGpsSpeed = 45.0;
#endif
			l_pulse_data.m_iPulseSec = (int)(value * 0.965);//pulse count 보정

		if(l_pulse_data.SetPulseSec(l_pulse_data.m_iPulseSec)) {
			if((in_param.pulse_init == 0 && l_pulse_data.m_bPulseState) || l_pulse_data.m_bPulseReinit){
				in_param.pulse_init ++;
				in_param.pulse_param = l_pulse_data.m_speed_parameter;
				printf("pulse parametter : %.02f\n", in_param.pulse_param);
				mixwav_play(kMixwaveDingdong);
			}
		}
		l_pulse_tmout_ms = get_tick_count();
		break;
		
		case REL_Y:
			if(l_pulse_data.m_pulse_count_accrue == value){
				l_pulse_data.m_fPulseSpeed = 0.0;
				l_pulse_data.m_iPulseSec = 0;
			}
			else
				l_pulse_data.m_pulse_count_accrue = value;

			evdev_input_pulse_data_add_stream();
		break;
		
		case REL_RX:
			l_pulse_data.m_iRpmPulseSec = (int)(value * 0.965);//rpm count 보정

		if((int)l_pulse_data.m_rpm_parameter > 0)
				l_pulse_data.m_iRpm = (int)(l_pulse_data.m_iRpmPulseSec * 60 * 2 / l_pulse_data.m_rpm_parameter);
		else
			l_pulse_data.m_iRpm = 0;
		
		break;
		
		case REL_RY:
			l_rpm_tmout_ms = get_tick_count();
			
			if(l_pulse_data.m_rpm_count_accrue == value){
				l_pulse_data.m_iRpm = 0;
				l_pulse_data.m_iRpmPulseSec = 0;
			}
			else
				l_pulse_data.m_rpm_count_accrue = value;

		break;
	 }
}

static int event_run(void)
{
	nmeaPOS lastNmeaPos = {0.0, 0.0};
	u32			gpsDataInvalidCount = 0;
	u32 			value;
#if USE_KEY	
	bool l_rdk_board = false;
    struct libevdev *dev_key = NULL;
	int fd_key = NULL;
#endif

#if USE_GPS
	//int l_fd_gps = 0;
//	bool gps_led_on = false;
	int gps_reopen = 3;

	int l_fd_gps = 0;
	CBbApi l_bbApi;
	
	l_bbApi.SerialHandleSet(&l_fd_gps);
#endif

#if USE_DA_PULSE		
	struct libevdev *dev_da = NULL;
	int fd_da = NULL;
#ifdef TARGET_CPU_V536		
	int da_pulse_input_no = 3;
#else
	int da_pulse_input_no = 0;
#endif

	l_pulse_data.m_rpm_parameter = in_param.engine_cylinders;
	l_pulse_data.m_speed_limit = in_param.speed_limit_value;

	if(in_param.pulse_init){
		l_pulse_data.m_bPulseState = in_param.pulse_init;
		l_pulse_data.m_speed_parameter = in_param.pulse_param;

		if(in_param.pulse_init)
			l_pulse_data.m_iSpdPulse = pulse_count_to_speed( 1 , in_param.pulse_param);
		
		printf("pulse parametter : %.02f\n", in_param.pulse_param);
	}
	
#endif


#if USE_GPS
	l_bbApi.SerialOpen(GPS_DEVICE, GPS_BAUDRATE);
#endif

    int rc = -EAGAIN;
	int i;


#if 0
	dagpio_pin_read(34); //TRG_I		PB02
	dagpio_pin_read(35); //BRK			PB03
	dagpio_pin_read(37); //SIG_R		PB05
	dagpio_pin_read(43); //SIG_L 		PB011
	dagpio_pin_read(45); //GEAR_R 	PB013
	dagpio_pin_read(200); //DC_DET	PG08
#endif

    printf("%s(): start.\n", __func__);

#if USE_GPS
	//l_fd_gps = GPS_OPEN();
	if(l_fd_gps < 0){
		goto exit;
	}
#endif

#if USE_KEY	
	

	if(fd_key == NULL){
#ifdef TARGET_CPU_V536		
		const char * name = "gpiokey";
		i = 4;
#else
		const char * name = "ht-gpio-keys";
		i= 3;
#endif
		
    	fd_key = __evdev_input_open(name, i); //new board is 3
	}

    if ( fd_key && !__evdev_new_from_fd_has_event_code(fd_key, &dev_key, EV_SW, SW_DOCK)) {
		//error
		dev_key = NULL;
//		goto exit;
    }
		
#endif

#if USE_DA_PULSE
	fd_da = __evdev_input_open("pulse_count", da_pulse_input_no); //new board is 0

    if ( fd_da && !__evdev_new_from_fd_has_event_code(fd_da, &dev_da, EV_REL, REL_X)) {
#if USE_KEY				
		l_rdk_board = true;
#endif
		//error
		dev_da = NULL;
    }
#endif		

	sleep(1);

#if USE_KEY	
	if(dev_key){
		for( i = 0; i < KEY_CODE_MAX; i++){
			value = libevdev_get_event_value(dev_key, EV_SW, i);
			if(value == 0)
				printf("%d is low\n", i);
			else
				printf("%d is high\n", i);

			_evdev_input_key_data_set(i, (BOOL)value, 1);
		}
	}
#endif

#if BLUE_LED_CONTROL
	datool_led_blue_set(0);
#else
	led_process_postCommand(LED_BLUE, 0, LED_TIME_INFINITY);
#endif

    // 루프 시작
    do {
        struct input_event ev;
		 //bool gps_led_on = 0;
		 u32 l_gps_tmout_ms = 0;
		 u32 l_speed_tmout_ms = 0;
		 char gps_data[256] = { 0, };
		 u32 gps_data_index = 0;

#if 0
		while ((rc = libevdev_next_event(dev_key, LIBEVDEV_READ_FLAG_SYNC , &ev)) == 1){
	     		/* this is a synced event */
					printf("Event Sync : %d(%s) %d(%s) %d (rc:%d)\n",
                  ev.type, libevdev_event_type_get_name(ev.type),
                  ev.code, libevdev_event_code_get_name(ev.type, ev.code),
                  ev.value, rc);
		}
#endif

		while(evdev_thread_run){
			char data[512];
			int len = 0;
			
#if USE_GPS
			//u64 stime = systemTime();
			len = read_serial(l_fd_gps, data, sizeof(data), 0);
			//printf("%lld (%d)\n",  systemTime() - stime, len);
#if 0//test code {++++++++++++++++++++++++++++++++++++++++++
			l_pulse_data.m_bPulseState = 1;
			l_pulse_data.m_fPulseSpeed = 100;

{
		static u32 nmea_interval_time = 0;
		static u32 nmea_cnt = 0;
		const char nmea_sample[][512] = {
			{"$GPZDA,194633.656,28,09,2020,00,00*5A\r\n" \
			"$GPGGA,194633.656,3459.000000,N,13551.000000,E,1,0,1,82,M,0,M,,*7E\r\n" \
			"$GPGLL,3459.000000,N,13551.000000,E,194633.656,A,A*5E\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194633.656,A,3459.000000,N,13551.000000,E,10.79,0,280920,90,E,A*35\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194634.656,28,09,2020,00,00*5D\r\n" \
			"$GPGGA,194634.656,3459.016667,N,13551.016667,E,1,0,1,82,M,0,M,,*79\r\n" \
			"$GPGLL,3459.016667,N,13551.016667,E,194634.656,A,A*59\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194634.656,A,3459.016667,N,13551.016667,E,10.79,0,280920,90,E,A*32\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194635.656,28,09,2020,00,00*5C\r\n" \
			"$GPGGA,194635.656,3459.033333,N,13551.033333,E,1,0,1,82,M,0,M,,*78\r\n" \
			"$GPGLL,3459.033333,N,13551.033333,E,194635.656,A,A*58\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194635.656,A,3459.033333,N,13551.033333,E,10.79,0,280920,90,E,A*33\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194636.656,28,09,2020,00,00*5F\r\n" \
			"$GPGGA,194636.656,3459.050000,N,13551.050000,E,1,0,1,82,M,0,M,,*7B\r\n" \
			"$GPGLL,3459.050000,N,13551.050000,E,194636.656,A,A*5B\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194636.656,A,3459.050000,N,13551.050000,E,10.79,0,280920,90,E,A*30\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194637.656,28,09,2020,00,00*5E\r\n" \
			"$GPGGA,194637.656,3459.066667,N,13551.066667,E,1,0,1,82,M,0,M,,*7A\r\n" \
			"$GPGLL,3459.066667,N,13551.066667,E,194637.656,A,A*5A\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194637.656,A,3459.066667,N,13551.066667,E,10.79,0,280920,90,E,A*31\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194638.656,28,09,2020,00,00*51\r\n" \
			"$GPGGA,194638.656,3459.083333,N,13551.083333,E,1,0,1,82,M,0,M,,*75\r\n" \
			"$GPGLL,3459.083333,N,13551.083333,E,194638.656,A,A*55\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194638.656,A,3459.083333,N,13551.083333,E,10.79,0,280920,90,E,A*3E\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194639.656,28,09,2020,00,00*50\r\n" \
			"$GPGGA,194639.656,3459.100000,N,13551.100000,E,1,0,1,82,M,0,M,,*74\r\n" \
			"$GPGLL,3459.100000,N,13551.100000,E,194639.656,A,A*54\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194639.656,A,3459.100000,N,13551.100000,E,10.79,0,280920,90,E,A*3F\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194640.656,28,09,2020,00,00*5E\r\n" \
			"$GPGGA,194640.656,3459.116667,N,13551.116667,E,1,0,1,82,M,0,M,,*7A\r\n" \
			"$GPGLL,3459.116667,N,13551.116667,E,194640.656,A,A*5A\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194640.656,A,3459.116667,N,13551.116667,E,10.79,0,280920,90,E,A*31\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194641.656,28,09,2020,00,00*5F\r\n" \
			"$GPGGA,194641.656,3459.133333,N,13551.133333,E,1,0,1,82,M,0,M,,*7B\r\n" \
			"$GPGLL,3459.133333,N,13551.133333,E,194641.656,A,A*5B\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194641.656,A,3459.133333,N,13551.133333,E,10.79,0,280920,90,E,A*30\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194642.656,28,09,2020,00,00*5C\r\n" \
			"$GPGGA,194642.656,3459.150000,N,13551.150000,E,1,0,1,82,M,0,M,,*78\r\n" \
			"$GPGLL,3459.150000,N,13551.150000,E,194642.656,A,A*58\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194642.656,A,3459.150000,N,13551.150000,E,10.79,0,280920,90,E,A*33\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194643.656,28,09,2020,00,00*5D\r\n" \
			"$GPGGA,194643.656,3459.166667,N,13551.166667,E,1,0,1,82,M,0,M,,*79\r\n" \
			"$GPGLL,3459.166667,N,13551.166667,E,194643.656,A,A*59\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194643.656,A,3459.166667,N,13551.166667,E,10.79,0,280920,90,E,A*32\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194644.656,28,09,2020,00,00*5A\r\n" \
			"$GPGGA,194644.656,3459.183333,N,13551.183333,E,1,0,1,82,M,0,M,,*7E\r\n" \
			"$GPGLL,3459.183333,N,13551.183333,E,194644.656,A,A*5E\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194644.656,A,3459.183333,N,13551.183333,E,10.79,0,280920,90,E,A*35\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194645.656,28,09,2020,00,00*5B\r\n" \
			"$GPGGA,194645.656,3459.200000,N,13551.200000,E,1,0,1,82,M,0,M,,*7F\r\n" \
			"$GPGLL,3459.200000,N,13551.200000,E,194645.656,A,A*5F\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194645.656,A,3459.200000,N,13551.200000,E,10.79,0,280920,90,E,A*34\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPGGA,194646.656,3459.216667,N,13551.216667,E,1,0,1,82,M,0,M,,*7C\r\n" \
			"$GPGLL,3459.216667,N,13551.216667,E,194646.656,A,A*5C\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194646.656,A,3459.216667,N,13551.216667,E,10.79,0,280920,90,E,A*37\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194647.656,28,09,2020,00,00*59\r\n" \
			"$GPGGA,194647.656,3459.233333,N,13551.233333,E,1,0,1,82,M,0,M,,*7D\r\n" \
			"$GPGLL,3459.233333,N,13551.233333,E,194647.656,A,A*5D\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194647.656,A,3459.233333,N,13551.233333,E,10.79,0,280920,90,E,A*36\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194648.656,28,09,2020,00,00*56\r\n" \
			"$GPGGA,194648.656,3459.250000,N,13551.250000,E,1,0,1,82,M,0,M,,*72\r\n" \
			"$GPGLL,3459.250000,N,13551.250000,E,194648.656,A,A*52\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194648.656,A,3459.250000,N,13551.250000,E,10.79,0,280920,90,E,A*39\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194649.656,28,09,2020,00,00*57\r\n" \
			"$GPGGA,194649.656,3459.266667,N,13551.266667,E,1,0,1,82,M,0,M,,*73\r\n" \
			"$GPGLL,3459.266667,N,13551.266667,E,194649.656,A,A*53\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194649.656,A,3459.266667,N,13551.266667,E,10.79,0,280920,90,E,A*38\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194650.656,28,09,2020,00,00*5F\r\n" \
			"$GPGGA,194650.656,3459.283333,N,13551.283333,E,1,0,1,82,M,0,M,,*7B\r\n" \
			"$GPGLL,3459.283333,N,13551.283333,E,194650.656,A,A*5B\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194650.656,A,3459.283333,N,13551.283333,E,10.79,0,280920,90,E,A*30\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194651.656,28,09,2020,00,00*5E\r\n" \
			"$GPGGA,194651.656,3459.300000,N,13551.300000,E,1,0,1,82,M,0,M,,*7A\r\n" \
			"$GPGLL,3459.300000,N,13551.300000,E,194651.656,A,A*5A\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194651.656,A,3459.300000,N,13551.300000,E,10.79,0,280920,90,E,A*31\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{"$GPZDA,194652.656,28,09,2020,00,00*5D\r\n" \
			"$GPGGA,194652.656,3459.316667,N,13551.316667,E,1,0,1,82,M,0,M,,*79\r\n" \
			"$GPGLL,3459.316667,N,13551.316667,E,194652.656,A,A*59\r\n" \
			"$GPVTG,0,T,180,M,10.79,N,19.9925884750787,K,A*14\r\n" \
			"$GPRMC,194652.656,A,3459.316667,N,13551.316667,E,10.79,0,280920,90,E,A*32\r\n" \
			"$GPGSA,A,3,,,,,,,,,,,,,1,1,1*2D\r\n" \
			"$GPRTE,1,1,C,0,*0B\r\n"}, \
			{0x00, 0x00}
			};

		if(is_time_out_ms(&nmea_interval_time, 1000)){
			
			len = sprintf(data, "%s", nmea_sample[nmea_cnt]);

			nmea_cnt++;

			if(strlen(nmea_sample[nmea_cnt]) == 0)
				nmea_cnt = 0;
		}
}
#endif//}+++++++++++++++++++++++++++++++++++++++++++++++
			if(len){
				
				for(int i = 0; i < len; i++){
					if(l_bbApi.SerialParser(data[i])){
						l_gps_tmout_ms = get_tick_count();
						continue;
					}
					
					gps_data[gps_data_index] = data[i];
					if(gps_data[gps_data_index] == '\n' || gps_data_index >= sizeof(gps_data)-1){
						//printf("%s", gps_data);
						gps_data_index++;						
						gps_data[gps_data_index++] = 0;
						if(strncmp( (const char *)gps_data, "$GPGGA", 6) == 0){
							if(l_gps.ParsingGPGGA(gps_data, 0)){
							}
						}
						else if(strncmp( (const char *)gps_data, "$GPRMC", 6) == 0){
							
							if(l_gps.ParsingGPRMC(gps_data, 0)){
								l_gps_tmout_ms = get_tick_count();

#if 0// test code
								printf("%s():  Valid %d (gmt %d)\n %s\r\n", __func__, (int)l_gps.m_bValid, in_param.gmt, gps_data);
								//l_gps.AdjustDateTime(in_param.gmt);
#endif								
								
								if(l_gps.m_bValid){
									char gps_data_buffer[256] = { 0, };
									gps_data_buffer[0] = 'G';
									strcpy(&gps_data_buffer[1], gps_data);
									RecorderAddStreamData((void *)&gps_data_buffer, gps_data_index+1);
									
									evdev_input_gps_time_sync_check(l_gps, in_param.gmt);


									l_pulse_data.m_fLatitude = l_gps.GetLat();
									l_pulse_data.m_fLongitude = l_gps.GetLng();
									l_pulse_data.m_fAltitude = l_gps.m_fAltitude;
									l_pulse_data.m_fGpsSpeed = l_gps.m_fSpeed * 1.853184;
									l_pulse_data.m_direction = l_gps.m_nCog / 100;
									l_pulse_data.m_directionCounts = l_gps.m_nCogCounts;

									//printf("%s():  speed %.02f (lat %.06f, lng %0.6f)\r\n", __func__, in_param.gps_speed, l_gps.GetLat(), l_gps.GetLng());
									
#if BLUE_LED_CONTROL
									if(!gps_led_on){
										gps_led_on = true;
										datool_led_blue_set(gps_led_on);
									}
#else
									if(l_gps.m_eGpsState != CGPSData::eGPS_STAUUS_VALID){
										l_gps.m_eGpsState = CGPSData::eGPS_STAUUS_VALID;
										led_process_postCommand( LED_BLUE, LED_TIME_INFINITY, 0);
									}
#endif
								}
								else {
#if BLUE_LED_CONTROL			
									gps_led_on = !gps_led_on;
									datool_led_blue_set(gps_led_on);
#else
									if(l_gps.m_eGpsState != CGPSData::eGPS_STATUS_INVALID){
										l_gps.m_eGpsState = CGPSData::eGPS_STATUS_INVALID;
										l_pulse_data.m_fGpsSpeed = 0;//test
										led_process_postCommand( LED_BLUE, 1000, 1000);
									}
#endif
								}

								l_pulse_data.m_bGpsSignalState = l_gps.m_bValid;
								l_pulse_data.SetSpeedLimit();

							}

							l_pulse_data.m_bGpsConnectionState = true;
							l_pulse_data.m_bGpsWasConnection = true;

						}
						gps_data_index = 0;
					}
					else
						gps_data_index++;
				}
			}

#if USE_KEY
			if(l_pulse_data.m_chattering_tm_turn_signal_left) {
				u32 chattering_tm = DEF_TURNON_CHATTERING_TIME;
				
				if(!l_pulse_data.m_current_bSL)
					chattering_tm = DEF_TURN_SIGNAL_CHATTERING_TIME;

				if(is_time_out_ms(&l_pulse_data.m_chattering_tm_turn_signal_left, chattering_tm)){
					l_pulse_data.m_chattering_tm_turn_signal_left = 0;
					l_pulse_data.m_bSL = l_pulse_data.m_current_bSL;
				}
			}
			if(l_pulse_data.m_chattering_tm_turn_signal_right) {
				u32 chattering_tm = DEF_TURNON_CHATTERING_TIME;
				
				if(!l_pulse_data.m_current_bSR)
					chattering_tm = DEF_TURN_SIGNAL_CHATTERING_TIME;
				
				if(is_time_out_ms(&l_pulse_data.m_chattering_tm_turn_signal_right, chattering_tm)) {
					l_pulse_data.m_chattering_tm_turn_signal_right = 0;
					l_pulse_data.m_bSR = l_pulse_data.m_current_bSR;
				}
			}

			if(l_pulse_data.m_chattering_tm_brk_signal) {
				u32 chattering_tm = DEF_TURNON_CHATTERING_TIME;
				
				if(!l_pulse_data.m_current_bBrk)
					chattering_tm = DEF_INDICATORS_CHATTERING_TIME;
				
				if(is_time_out_ms(&l_pulse_data.m_chattering_tm_brk_signal, chattering_tm)){
					l_pulse_data.m_chattering_tm_brk_signal = 0;

					if(l_pulse_data.m_bDcDet == 0) // power off시 변경되지 않도록
						l_pulse_data.m_bBrk = l_pulse_data.m_current_bBrk;
				}
			}
			
			if(l_pulse_data.m_chattering_tm_trg_signal && is_time_out_ms(&l_pulse_data.m_chattering_tm_trg_signal, 1000)) {
				l_pulse_data.m_chattering_tm_trg_signal = 0;
				
				if(l_pulse_data.m_bTR != l_pulse_data.m_current_input1) {
					if(l_pulse_data.m_bDcDet == 0){ // power off시 변경되지 않도록
						l_pulse_data.m_bTR = l_pulse_data.m_current_input1;
						RecorderPostCommand(kRestartRecording);
					}
				}
			}
			if(l_pulse_data.m_chattering_tm_bgr_signal && is_time_out_ms(&l_pulse_data.m_chattering_tm_bgr_signal, 1000)) {
				l_pulse_data.m_chattering_tm_bgr_signal = 0;
				
				if(l_pulse_data.m_bBgr != l_pulse_data.m_current_input2) {
					if(l_pulse_data.m_bDcDet == 0){ // power off시 변경되지 않도록
						l_pulse_data.m_bBgr = l_pulse_data.m_current_input2;

						if(l_pulse_data.m_bTR) 
							RecorderPostCommand(kRestartRecording);
					}
				}
			}

			if(l_pulse_data.m_bDcDet && l_rdk_board == false) {
				if(is_time_out_ms(&l_pulse_data.m_chattering_tm_DC_Det, DEF_DC_DET_CHATTERING_TIME)) {
						RecorderPostCommand(kExternalPowerFall);

					//l_pulse_data.m_chattering_tm_DC_Det = 0;
				}
			}
#endif

			if(is_time_out_ms(&l_gps_tmout_ms, 3000)){

				if(l_pulse_data.m_bGpsConnectionState){
					printf("%s():  GPS Disconnected...\n", __func__);
					
#if BLUE_LED_CONTROL					
					gps_led_on = false;
					datool_led_blue_set(gps_led_on);
#else
					l_gps.m_eGpsState = CGPSData::eGPS_STATUS_NC;
					led_process_postCommand(LED_BLUE, 0, LED_TIME_INFINITY);
#endif
				}
				
				l_pulse_data.m_bGpsConnectionState = false;
				l_pulse_data.m_bGpsSignalState = false;
//add_230628
				l_pulse_data.m_fLatitude = 0;
				l_pulse_data.m_fLongitude = 0;
				l_pulse_data.m_fGpsSpeed = 0;
//
				l_pulse_data.SetSpeedLimit();
				

				if(gps_reopen){
					gps_reopen--;
					//close_serial(l_fd_gps);
					//l_fd_gps = GPS_OPEN();
					l_bbApi.SerialOpen(GPS_DEVICE, GPS_BAUDRATE);
				}
			}
#endif

#if USE_DA_PULSE
			if(is_time_out_ms(&l_pulse_tmout_ms, 2000)){
				l_pulse_data.SetSpeedLimit();
				
				evdev_input_pulse_data_add_stream();
			}
#endif


			if(is_time_out_ms(&l_speed_tmout_ms, DEF_PAI_R_SPD_INTERVAL)){
				double speed = 0.0; //km/h
				double distance = 0.0; //m
				
				if(l_pulse_data.m_bPulseState)
					speed = l_pulse_data.m_fPulseSpeed;
				else if(l_pulse_data.m_bGpsSignalState)
					speed = l_pulse_data.m_fGpsSpeed;
//++{**********************
//relic : 20220217
// Speed 값으로 거리를 환산하는 방식은 100km 이상 누적 시 오차가 심하여  좌표 값으로 거리를 계산하도록 수정

				gpsDataInvalidCount++;
				if(l_pulse_data.m_bGpsSignalState){
					if(speed > 2.0 && l_pulse_data.m_fLatitude != lastNmeaPos.lat && l_pulse_data.m_fLongitude != lastNmeaPos.lon){
						nmeaPOS gpsPos;
						gpsPos.lat = l_pulse_data.m_fLatitude;
						gpsPos.lon = l_pulse_data.m_fLongitude;
						gpsPos.alt = l_pulse_data.m_fAltitude;
						
						if(lastNmeaPos.lat != 0.0 && lastNmeaPos.lon != 0.0){
							distance = datool_nmea_distance(&lastNmeaPos, &gpsPos);
							//printf(" %f %f	DISTANCE %0.1fm.  (Altitude %f  %0.1fm)\n", gpsPos.lat, gpsPos.lon, distance, gpsPos.alt, datool_nmea_distance_with_altitude(&lastNmeaPos, &gpsPos));
							
							if(gpsDataInvalidCount * 350 < distance){
								printf("ERROR : The distance %0.1fm is too big\n", distance);
								distance = 0.0;
							}
						}

						lastNmeaPos = gpsPos;
						gpsDataInvalidCount=0;
					}
					
				}


				if(distance == 0.0 && gpsDataInvalidCount >= 2){
					if(l_pulse_data.m_bPulseState) {
						distance = speed * 0.277778 / (1000 / DEF_PAI_R_SPD_INTERVAL) ;

						if(!l_pulse_data.m_bGpsSignalState)
							lastNmeaPos.lat = lastNmeaPos.lon = 0.0;
					}
				}

				l_pulse_data.m_fDistance += distance;
				//printf(" TOTAL DISTANCE %0.1fm.\n", l_pulse_data.m_fDistance);
//++}**********************
					

#ifdef DEF_PAI_R_DATA_SAVE				
				if(Recorder_isNetExist()){
					PAI_R_SPEED_CSV spd = {0,};
					time_t t = time(0);
					
					tick_count_sync(t);				
					l_speed_tmout_ms = l_speed_tmout_ms - (l_speed_tmout_ms % DEF_PAI_R_SPD_INTERVAL);
					
					spd.create_time = t;
					spd.create_time_ms = (get_tick_count_sync() % 1000);

					spd.speed = (u16)speed;
					spd.distance = l_pulse_data.m_fDistance;

					pai_r_datasaver_addSpeed(spd, Recorder_isEventRunning());
				}
#endif				
			}

#if USE_KEY	
			if(dev_key){
	       	rc = libevdev_next_event(dev_key, LIBEVDEV_READ_FLAG_NORMAL, &ev);
#if defined(TARGET_CPU_V536)
				while(libevdev_next_event(dev_key, LIBEVDEV_READ_FLAG_NORMAL, &ev) == 0); // clear, ??? EV_SYN data만 호출됨

				for( i = 0; i < KEY_CODE_MAX; i++){
					value = libevdev_get_event_value(dev_key, EV_SW, i);
					_evdev_input_key_data_set(i, (BOOL)value, 0);
				}
#endif
			}
			
			if(rc == -EAGAIN)
#endif				
			{
#if USE_DA_PULSE		
				if(dev_da){
					rc = libevdev_next_event(dev_da, LIBEVDEV_READ_FLAG_NORMAL, &ev);
 #if defined(TARGET_CPU_V536)
	 				if(rc == 0){
						u32  l_pulse_value[4] = {0,};
 #if 0						
	 					printf("Event: %d(%s) %d(%s) %d (rc:%d)\n",
			                    ev.type, libevdev_event_type_get_name(ev.type),
			                    ev.code, libevdev_event_code_get_name(ev.type, ev.code),
			                    ev.value, rc);
#endif	 
						while(libevdev_next_event(dev_da, LIBEVDEV_READ_FLAG_NORMAL, &ev) == 0); // clear, ??? EV_SYN data만 호출됨
#if 1
						if(__evdev_get_pulse_count(da_pulse_input_no, l_pulse_value)){
							_evdev_input_pulse_data_set(REL_X, l_pulse_value[0], 0);
							_evdev_input_pulse_data_set(REL_Y, l_pulse_value[1], 0);
							_evdev_input_pulse_data_set(REL_RX, l_pulse_value[2], 0);
							_evdev_input_pulse_data_set(REL_RY, l_pulse_value[3], 0);
						}
#else
						for( i = 0; i < 2; i++){
							value = libevdev_get_event_value(dev_da, EV_REL, REL_X + i);
							_evdev_input_pulse_data_set(REL_X + i, value, 0);
						}

						for( i = 0; i < 2; i++){
							value = libevdev_get_event_value(dev_da, EV_REL, REL_RX + i);
							_evdev_input_pulse_data_set(REL_RX + i, value, 0);
						}
 #endif
					}
#endif
				}
				
				if(rc == -EAGAIN)
#endif
				{
					usleep(1000);
					continue;
				}
			}

			if(rc != 1 && rc != 0){
				printf("libevdev_next_event(): error (%d)\n", rc);
				break;
			}

		 	if(ev.type == EV_SYN && ev.code == SYN_REPORT){
					continue;
		 	}
		
	       if (rc == 0) {
				if (ev.type == EV_KEY && ev.code == KEY_1) { //speed
					//event_inc_speed_count(&ev);
				}
				else {
#if 0					
					printf("Event: %d(%s) %d(%s) %d (rc:%d)\n",
		                    ev.type, libevdev_event_type_get_name(ev.type),
		                    ev.code, libevdev_event_code_get_name(ev.type, ev.code),
		                    ev.value, rc);
#endif

					if (ev.type == EV_REL) {
						_evdev_input_pulse_data_set(ev.code, ev.value, 0);
					}
					else if (ev.type == EV_SW) {
						
					 	_evdev_input_key_data_set((int)ev.code, (BOOL)ev.value, 0);
						
						evdev_input_pulse_data_add_stream();
					}
	          }
	       }
		}
    } while (evdev_thread_run);

exit:
	
#if USE_KEY
	if(dev_key)
  		libevdev_free(dev_key);
#endif

#if USE_DA_PULSE	 
	if(dev_da)
   		libevdev_free(dev_da);
#endif




#if USE_KEY
	if(fd_key)
		close(fd_key);
#endif

#if USE_DA_PULSE	
	if(fd_da)
		close(fd_da);
#endif

#if USE_GPS
	if(l_fd_gps)
		close_serial(l_fd_gps);
#endif

	printf("%s(): exit(%d, %d).\n", __func__, evdev_thread_run, rc);
	
 return 0;
}

#if USE_GSENSOR
static int gsensor_event_run(void)
{
	int g_no = 2;

	GVALUE gv = { 0, };
	
	struct libevdev *dev_gsensor = NULL;
	int fd_gsensor;
	
	u32 g_interval_time = 0;
	u32 g_interval_ms = 20;
	const char* class_path = "/sys/class/input/input";

    int rc = -EAGAIN;
//	int i;

    printf("%s(): start.\n", __func__);

	{
		#define BUF_MAX 4
		char buf[BUF_MAX];
	    char read_buf[BUF_MAX] = {0,};
	    size_t bytes_written;
		const char * class_name = "bma250";
#if defined(TARGET_CPU_V536)	
		g_no = 0;
#else
		g_no = 2;
#endif
		fd_gsensor = __evdev_input_open(class_name, g_no);
		    //가속도 센서를 켠다.
	    sysfs_putchar(format_string("%s%d/enable", class_path, g_no).c_str(), '1');
	    printf("g-sensor power on\n");

	    //가속도 센서의 주기를 설정한다. (10 ~ 200 ms)
	    bytes_written = sprintf(buf, "%d", g_interval_ms);
	    sysfs_write(format_string("%s%d/delay", class_path, g_no).c_str(), buf, bytes_written);
	    

	    //설정된 센서의 주기 값을 읽어본다.ls 
	    sysfs_read(format_string("%s%d/delay", class_path, g_no).c_str(), read_buf, sizeof(read_buf));
	    printf("g-sensor delay %s \n", read_buf);


		if( !__evdev_new_from_fd_has_event_code(fd_gsensor, &dev_gsensor, EV_ABS, ABS_X)){
			dev_gsensor = NULL;
			goto exit;
		}
	}
	sleep(1);


	gv.x = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_X);
	gv.y = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_Y);
	gv.z = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_Z);

	printf("gsensor : %d, %d, %d\n", gv.x, gv.y, gv.z);
	
    // 루프 시작
    do {
        struct input_event ev;


		while(evdev_gsensor_thread_run){

			rc = libevdev_next_event(dev_gsensor, LIBEVDEV_READ_FLAG_NORMAL, &ev);
			if(rc == -EAGAIN)
			{
				usleep(1000);
				continue;
			}
			else if(rc != 1 && rc != 0){
				printf("libevdev_next_event(): error (%d)\n", rc);
				break;
			}
			
			if(ev.type == EV_SYN){
#if defined(TARGET_CPU_V536)
				while(libevdev_next_event(dev_gsensor, LIBEVDEV_READ_FLAG_NORMAL, &ev) == 0); // clear, ??? EV_SYN data만 호출됨

				gv.x = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_X);
				gv.y = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_Y);
				gv.z = libevdev_get_event_value(dev_gsensor, EV_ABS, ABS_Z);
#endif				
				G_sensor_XYZ(gv);
				if(is_time_out_ms(&g_interval_time, GSEND_STREAM_DATA_INTERVAL_TIME)){
					char data[256];
					int len = 0;

					g_interval_time -= g_interval_ms - 1; // for GSEND_STREAM_DATA_INTERVAL_TIME timeout sync
					
					len = CGSensorData::MakeGSensorData((signed char *)data, sizeof(data), &gv);
					if(len)
						RecorderAddStreamData((void *)data, len);
				}
			}

		 	if(ev.type == EV_SYN && ev.code == SYN_REPORT){
					continue;
		 	}
		
	       if (rc == 0) {
#if 0					
					printf("GSensor Input Event: %d(%s) %d(%s) %d (rc:%d)\n",
                    ev.type, libevdev_event_type_get_name(ev.type),
                    ev.code, libevdev_event_code_get_name(ev.type, ev.code),
                    ev.value, rc);
#endif

					if(ev.type == EV_ABS){
						switch (ev.code) {
							case ABS_X:{
								gv.x = ev.value;
								break;
							}
							case ABS_Y:{
								gv.y = ev.value;
								break;
							}
							case ABS_Z:{
								gv.z = ev.value;
								break;
							}
						}
	          }
	       }
		}
    } while (evdev_gsensor_thread_run);

exit:
	
	if(dev_gsensor)
  		libevdev_free(dev_gsensor);

	
	if(fd_gsensor)
  		close(fd_gsensor);
	
	sysfs_putchar(format_string("%s%c/enable", class_path, g_no).c_str(), '0');
	printf("g-sensor power off\n");


	printf("%s(): exit(%d, %d).\n", __func__, evdev_gsensor_thread_run, rc);
	
 return 0;
}
#endif

int evdev_input_gps_time_sync_check(CGPSData &gps, int gmt)
{
#if 0 // test
	gps.m_nYear = 19;
	gps.m_nMonth = 2;
	gps.m_nDay = 8;
	gps.m_nHour = 15;
	gps.m_nMinute = 05;
	gps.m_nSecond = 30;
	
	gps.AdjustDateTime(gmt);
#endif

	if(gps.m_nValid_time > 15 && gps.m_nYear >= 20) {
		if(gps.m_bTimeSynced == FALSE){
			gps.m_bTimeSynced = TRUE;

			//SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE , RTC_SRC_GPS, "Before");
			if(gps.AdjustDateTime(gmt))
				SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE , RTC_SRC_GPS, "After");
		}
#if 0		
		if(gps.m_nValid_time > 3600){
			gps.m_nValid_time = 0;
			SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE , RTC_SRC_GPS, "Before");
			gps.AdjustDateTime(gmt);
			SYSTEMLOG(LOG_TYPE_SYSTEM, LOG_EVENT_DATETIMECHANGE , RTC_SRC_GPS, "After");
		}
#endif		
	}

	return false;
}

void evdev_input_set_pulse_parameter(bool bBrake, bool bInput1, bool bInput2, int iBlink, bool init, double paramiter, int gmt, double engine_cylinders, int speed_limit)
{
	in_param.bBrakePu = bBrake;
	in_param.bInput1Pu = bInput1;
	in_param.bInput2Pu = bInput2;
	in_param.iBlink = iBlink;
	
	in_param.pulse_init = (int)init;
	in_param.pulse_param = paramiter;

	in_param.gmt = gmt;

	in_param.engine_cylinders = engine_cylinders;
	in_param.speed_limit_value = speed_limit;
}

INPUT_PARAM *evdev_input_get_pulse_parameter(void)
{
	return &in_param;
}

CGPSData evdev_input_get_gpsdata(void)
{
	//printf("valid:%d, lat:%.06f, lng:%.06f\n",l_gps.m_bValid, l_gps.GetLat(), l_gps.GetLng());
	return l_gps;
}

CPulseData evdev_input_get_pulsedata(void)
{
	return l_pulse_data;
}

bool evdev_input_get_input1Event(void)
{
	if(l_pulse_data.m_bInput1Evt){
		l_pulse_data.m_bInput1Evt = false;

		return true;
	}

	return false;
}

bool evdev_input_get_input2Event(void)
{
	if(l_pulse_data.m_bInput2Evt){
		l_pulse_data.m_bInput2Evt = false;

		return true;
	}

	return false;
}


int evdev_input_pulse_data_add_stream(void)
{
	char data[128] = { 0, };
	int len = l_pulse_data.MakeStringPulseData((char *)data, sizeof(data));
	if(len) {
		if(strcmp(l_pre_pulse_data, data) != 0){
			RecorderAddStreamData((void *)data, len);
			strcpy(l_pre_pulse_data, data);
			//printf("pulse : %s\n", data);

			return 1;
		}
	}

	return 0;
}

int evdev_gsensor_start(void)
{	
#if USE_GSENSOR
	if(evdev_gsensor_thread_run == false){
		evdev_gsensor_thread_run = true;
	   	input_gsensor_thread = std::thread(gsensor_event_run);
	}
#endif	
	return 0;
}

int evdev_input_start(bool debug_mode)
{	
	if(evdev_thread_run == false && !debug_mode){
		evdev_thread_run = true;
	   	input_thread = std::thread(event_run);
	}

#if 0 //USE_GSENSOR
	if(evdev_gsensor_thread_run == false){
		evdev_gsensor_thread_run = true;
	   	input_gsensor_thread = std::thread(gsensor_event_run);
	}
#endif	
	return 0;
}

int evdev_input_stop(void)
{

	if(evdev_thread_run){
		evdev_thread_run = false;
		msleep(200);
		 if (input_thread.joinable()) {
			input_thread.join();
		 }
	}
#if USE_GSENSOR
	if(evdev_gsensor_thread_run){
		evdev_gsensor_thread_run = false;
		msleep(200);
		 if (input_gsensor_thread.joinable()) {
			input_gsensor_thread.join();
		 }
	}
#endif	
	 return 0;
}

int evdev_input_test(void)
{
	return 0;
}

