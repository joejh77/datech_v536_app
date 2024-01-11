/**************************************************************************************************
 *
 *      File Name       :  safe_driving_monitoring.cpp
 *      Description     :   vehicle motion event library source
 *
 *      Creator         :   relic
 *      Create Date     :   2021/06/02
 *      Update History  :   
 *
 *************************************************************************************************/
 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "datools.h"
#include "evdev_input.h"
#include "recorder.h"
#include "safe_driving_monitoring.h"

/**********************************************************
  *  definitions
 ***********************************************************/
#define ZONE_SDM_ERROR		1
#define ZONE_SDM_INIT		1
#define ZONE_SDM_FUNC		0


 /*************************************
 * functions
 *************************************/
#ifdef DEF_SAFE_DRIVING_MONITORING

static ST_SDM_CONTROL l_SDM = { 30, 30, 20, 0, kStartEventNone, DX_ROTATE_0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,false};

int safe_driving_get_diff_speed(void)
{
	return l_SDM.FB;
}

int safe_driving_get_rotate_value(void)
{
	return l_SDM.LR;
}

u32 safe_driving_get_overspeed_count(void)
{
	return l_SDM.uOverspeedCount;
}

bool safe_driving_get_highway_mode(void)
{
	return l_SDM.bHighway;
}

bool safe_driving_get_overspeed_mode(void)//add
{
	return l_SDM.Overspeedmode;
}

bool safe_driving_get_fastspeed_mode(void)//add
{
	return l_SDM.Fastspeedmode;
}

void safe_driving_control_set(int trgLevel_Acceleration, int trgLevel_Deacceleration, int trgLevel_LR, DX_ROTATE rotate)
{
	l_SDM.trgLevel_Acceleration = trgLevel_Acceleration;
	l_SDM.trgLevel_Deacceleration = trgLevel_Deacceleration;
	l_SDM.trgLevel_LR = trgLevel_LR;
	l_SDM.rotate = rotate;
}

void safe_driving_speed_set(ST_CFG_OVSPEED over, ST_CFG_OVSPEED fast, ST_CFG_OVSPEED high)
{
	l_SDM.Overspeed = over;
	l_SDM.Fastspeed = fast;
	l_SDM.Highspeed = high;
}

void safe_driving_monitoring_overspeed(int speed)
{
	static u32 highspeed_check_time = 0;
	static u32 overspeed_check_time = 0;
	static u32 fastspeed_check_time = 0;
	
	int event = kStartEventNone;

	if(l_SDM.Highspeed.nTime && speed >= l_SDM.Highspeed.nSpeed){
		if(++highspeed_check_time >= l_SDM.Highspeed.nTime){
			l_SDM.bHighway = true;
			l_SDM.Overspeedmode = false;
		}
	}
	else{
		highspeed_check_time = 0;
		l_SDM.bHighway = false;
	}
	
	if(l_SDM.Fastspeed.nTime && speed >= l_SDM.Fastspeed.nSpeed){
		overspeed_check_time = 0;
		if(++fastspeed_check_time >= l_SDM.Fastspeed.nTime){
			l_SDM.Fastspeedmode = true;//add
			u32 time = fastspeed_check_time - l_SDM.Fastspeed.nTime;
			
			if((time % l_SDM.Fastspeed.nAlarm) == 0){
				if(time == 0)
					l_SDM.uOverspeedCount = 0;
				
				l_SDM.uOverspeedCount++;

				event = kStartEventFastSpeed;
				DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM Fast Overspeed EVENT detection. %d(count %d)\r\n", speed, l_SDM.uOverspeedCount));
			}
		}
	}
	else if(l_SDM.Fastspeedmode == true && speed < l_SDM.Fastspeed.nSpeed){
		fastspeed_check_time = 0;
		l_SDM.Fastspeedmode = false;
	}
	else if(l_SDM.Overspeed.nTime && speed >= l_SDM.Overspeed.nSpeed && l_SDM.bHighway == false){
		fastspeed_check_time = 0;
		if(++overspeed_check_time >= l_SDM.Overspeed.nTime){
			l_SDM.Overspeedmode = true;//add
			u32 time = overspeed_check_time - l_SDM.Overspeed.nTime;
			
			if((time % l_SDM.Overspeed.nAlarm) == 0){
				if(time == 0)
					l_SDM.uOverspeedCount = 0;
				
				l_SDM.uOverspeedCount++;

				event = kStartEventOverSpeed;
				DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM General Overspeed EVENT detection. %d(count %d)\r\n", speed, l_SDM.uOverspeedCount));
			}
		}
	}
	else {
		overspeed_check_time = 0;
		fastspeed_check_time = 0;
		l_SDM.uOverspeedCount = 0;
		l_SDM.Overspeedmode = false;
		l_SDM.Fastspeedmode = false;
	}	
	
	if(event != kStartEventNone) {
		RecorderPostCommand(event);
	}
}

void safe_driving_monitoring_osakagas(void)
{
	int event = kStartEventNone;
	double speed; //km/h
	static double pre_speed;
	int diff_speed;

	int direction;
	static int pre_direction;
	int diff_direction;
	int rotate_value;
	
	CPulseData pulse = evdev_input_get_pulsedata();

	if(pulse.m_bPulseState && pulse.m_pulse_count_accrue)
		speed = (u32)pulse.m_fPulseSpeed;	// Km/h
	else
		speed = (u32)pulse.m_fGpsSpeed;

	diff_speed = (int)(speed - pre_speed);

	safe_driving_monitoring_overspeed((int)speed);
	
	if(pulse.m_direction > pre_direction)
		diff_direction = pulse.m_direction - pre_direction;
	else
		diff_direction = pre_direction - pulse.m_direction;

	if(diff_direction > 180){
		diff_direction = 360 - diff_direction;
	}

	rotate_value = (speed *  diff_direction / 10);
	
	DEBUGMSG(ZONE_SDM_FUNC, ("%s():	%d		%d	%d\r\n", __func__, diff_speed, diff_direction, rotate_value));

	if(diff_speed >= l_SDM.trgLevel_Acceleration){
		l_SDM.FB = diff_speed;
		event = kStartEventSuddenAcceleration;
		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM Acceleration EVENT detection. %d (TrgLevel : %d)\r\n", diff_speed, l_SDM.trgLevel_Acceleration));
	}
	else if((-diff_speed) >= l_SDM.trgLevel_Deacceleration) { // 
		l_SDM.FB = -diff_speed;
		event = kStartEventSuddenDeacceleration;
		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM Deacceleration EVENT detection. %d (TrgLevel : %d)\r\n", -diff_speed, l_SDM.trgLevel_Deacceleration));
	}
	else if(rotate_value >= l_SDM.trgLevel_LR){
		if(speed > 10.0 && pulse.m_directionCounts > 2){ //filter
			l_SDM.LR = rotate_value;
			
			if(pulse.m_direction < pre_direction){
				event = kStartEventRapidLeft;
			}
			else {
				event = kStartEventRapidRight;
			}

			DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM TURN EVENT(%d) detection. %d (TrgLevel : %d)\r\n", event - kStartEventRapidLeft, rotate_value, l_SDM.trgLevel_LR));
		}
	}	
	
	pre_speed = speed;
	pre_direction = pulse.m_direction;

	if(event != kStartEventNone) {
		RecorderPostCommand(event);
	}
}
 
void safe_driving_monitoring(int motion_forward_backward_movement, int motion_left_right_turn, int motion_up_down, int without_gravity)
{
	int event = kStartEventNone;
	u32 tick = get_tick_count();


	DEBUGMSG(ZONE_SDM_FUNC, ("	%d		%d		%d 	%d\r\n", motion_forward_backward_movement, motion_left_right_turn, motion_up_down, without_gravity));

	if(l_SDM.uLastEventTime + 5000 > tick){

		if(l_SDM.uLastEventTime + 300 < tick && l_SDM.iLastEvent != kStartEventNone) {
			RecorderPostCommand(l_SDM.iLastEvent);
			l_SDM.iLastEvent = kStartEventNone;
		}
		return;
	}

	if((motion_up_down > 30 || motion_up_down < -30) || \
		(without_gravity < 15 && without_gravity > -15)) { //noise
		l_SDM.uLastNoiseTime = tick;
	}
	
	if(l_SDM.uLastNoiseTime + 200 > tick){
		l_SDM.iLastEvent = kStartEventNone;
		l_SDM.uLastEventTime = 0;
		l_SDM.uEventChatteringTime = tick;
		return;
	}

	if( motion_forward_backward_movement > l_SDM.trgLevel_Acceleration){
		CPulseData PD = evdev_input_get_pulsedata();
	
		if((PD.m_bPulseState && PD.m_fPulseSpeed < 5.0) || \
			((PD.m_bGpsConnectionState && PD.m_bGpsSignalState) && (PD.m_fGpsSpeed < 5.0))) {
			//SA = Sudden Acceleration
			event = kStartEventSuddenAcceleration;
		}
		else {
			event = kStartEventSuddenStart;
		}
		
		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM Acceleration EVENT detection. %d (TrgLevel : %d)\r\n", motion_forward_backward_movement, l_SDM.trgLevel_Acceleration));
	}
	else if(motion_forward_backward_movement < -l_SDM.trgLevel_Deacceleration){
		event = kStartEventSuddenDeacceleration;
		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM Deacceleration EVENT detection. %d (TrgLevel : %d)\r\n", motion_forward_backward_movement, -l_SDM.trgLevel_Deacceleration));
	}
	else if( motion_left_right_turn > l_SDM.trgLevel_LR){
		event = kStartEventRapidLeft;
		
		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM LEFT TURN EVENT detection. %d (TrgLevel : %d)\r\n", motion_left_right_turn, l_SDM.trgLevel_LR));
	}
	else if(motion_left_right_turn < -l_SDM.trgLevel_LR){
		event = kStartEventRapidRight;

		DEBUGMSG(ZONE_SDM_FUNC,(" ----> SDM RIGHT TURN EVENT detection. %d (TrgLevel : %d)\r\n", motion_left_right_turn, -l_SDM.trgLevel_LR));
	}

	if(event == kStartEventNone || event != l_SDM.iLastEvent){
		l_SDM.uEventChatteringTime = tick;
	}
	
	if(event != kStartEventNone){
		l_SDM.iLastEvent = event;
		
		if(is_time_out_ms(&l_SDM.uEventChatteringTime, 300)){
			// send event
			l_SDM.uLastEventTime = tick;
		}
	}
}

#endif
