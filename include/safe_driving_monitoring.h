/**************************************************************************************************
 *
 *      File Name       :  safe_driving_monitoring.h
 *      Description     :  vehicle motion event library source
 *
 *      Creator         :   relic
 *      Create Date     :   2021/06/02
 *      Update History  :   
 *
 *************************************************************************************************/
#ifndef __SAFE_DRIVING_MONITORING_H__
#define __SAFE_DRIVING_MONITORING_H__
#include "datypes.h"
#include "gsensor.h"
#include "ConfigTextFile.h"

/**********************************************************
 * definitions
***********************************************************/



 typedef struct tagSDMControl
 {
	 int	 				trgLevel_Acceleration;
	 int	 				trgLevel_Deacceleration;
	 int					trgLevel_LR;
	 u32					uLastEventTime;
	 int					iLastEvent;
	DX_ROTATE		rotate;

	 int					FB; // + forward, - backward movement
	 int					LR; // + left, - right turn

	u32					uLastNoiseTime;
	u32					uEventChatteringTime;

	////
	ST_CFG_OVSPEED		Overspeed;	//General overspeed
	ST_CFG_OVSPEED		Fastspeed;	//Fast overspeed
	ST_CFG_OVSPEED		Highspeed;	// High-Speed	
	u32					uOverspeedCount;
	bool					bHighway;
	bool				Overspeedmode;//add
	bool				Fastspeedmode;//add
 } ST_SDM_CONTROL, * PST_SDM_CONTROL;

 /*************************************
 * functions
 *************************************/
 #ifdef __cplusplus
extern "C" {
#endif
bool safe_driving_get_overspeed_mode(void);//add
bool safe_driving_get_fastspeed_mode(void);//add

int safe_driving_get_diff_speed(void);
int safe_driving_get_rotate_value(void);
u32 safe_driving_get_overspeed_count(void);
bool safe_driving_get_highway_mode(void);
void safe_driving_control_set(int trgLevel_Acceleration, int trgLevel_Deacceleration, int trgLevel_LR, DX_ROTATE rotate);
void safe_driving_speed_set(ST_CFG_OVSPEED over, ST_CFG_OVSPEED fast, ST_CFG_OVSPEED high);
void safe_driving_monitoring(int motion_forward_backward_movement, int motion_left_right_turn, int motion_up_down, int without_gravity);
void safe_driving_monitoring_osakagas(void);
#ifdef __cplusplus
 }
#endif
#endif /* end of __SAFE_DRIVING_MONITORING_H__ */
