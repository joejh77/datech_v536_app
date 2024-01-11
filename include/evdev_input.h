
#ifndef __EVDEV_INPUT_H__
#define __EVDEV_INPUT_H__

#include "datypes.h"
#include "recorder.h"
#include "user_data.h"

#ifdef TARGET_CPU_V536
#define GPS_DEVICE "/dev/ttyS4"
#else
#define GPS_DEVICE "/dev/ttyS2"
#endif

typedef struct {
	bool bBrakePu;		//pull-down false, pull-up true
	bool bInput1Pu;
	bool bInput2Pu;
	int	iBlink; //vaule 1 : 1초 / value 2 : 1.5초 / value 3 : 2초 선택 가능
	
	int  pulse_init;
	double pulse_param;
	
	int	gmt;

	double	engine_cylinders;
	int		speed_limit_value;
}INPUT_PARAM;

#ifdef __cplusplus
extern "C" {
#endif

/*
ID | GPS Status |GPS Speed |Pulse Set | Pulse Speed	Pulse/sec	Spd/Pulse	Brk	SR	SL	우대(HC)	승차(GI)	Separator	Year	Month	Day	Hr	Min	Sec
P$
*/


void evdev_input_set_pulse_parameter(bool bBreak, bool bInput1, bool bInput2, int iBlink, bool init, double paramiter, int gmt, double engine_cylinders, int speed_limit);
INPUT_PARAM *evdev_input_get_pulse_parameter(void);
CGPSData evdev_input_get_gpsdata(void);
CPulseData evdev_input_get_pulsedata(void);
bool evdev_input_get_input1Event(void);
bool evdev_input_get_input2Event(void);
int evdev_input_pulse_data_add_stream(void);

int evdev_gsensor_start(void);
int evdev_input_start(bool debug_mode);
int evdev_input_stop(void);
int evdev_input_test(void);

#ifdef __cplusplus
}
#endif

#endif
