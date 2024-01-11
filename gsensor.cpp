/**************************************************************************************************
 *
 *      File Name       :  dxgsensor.c
 *      Description     :   G-Sensor event library source
 *
 *      Creator         :   relic
 *      Create Date     :   2016/02/15
 *      Update History  :   
 *
 *************************************************************************************************/
 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "datools.h"
#include "safe_driving_monitoring.h"
#include "gsensor.h"
#include "recorder.h"


/**********************************************************
  *  definitions
 ***********************************************************/
#define ZONE_GSENS_ERROR	1
#define ZONE_GSENS_INIT		1
#define ZONE_GSENS_FUNC		1

#define G_SENSOR_DELTA_TIME_AND_GRAVITY     1
#define G_SENSOR_DELTA_GRAVITY              2
#define G_SENSOR_THRESHOLD                  3

#define ROTATION_CHECK_VALUE 0.15

#define Sleep(ms)		msleep(ms)
#define sys_clock_gettick	get_tick_count
#define util_IsTimerDiff is_time_out_ms
#define RtGetClockUtc get_sec_count

 /*--------------------------------------------------------------------------------

    [NOTICE] Choose a Mode.
    
    1)G_SENSOR_DELTA_TIME_AND_GRAVITY   => Compare Calculating Gravity Degree & User Setting Gravity Degree 
    2)G_SENSOR_DELTA_GRAVITY            => Compare Calculating Delta Gravity & User Setting Delta Gravity 
    2)G_SENSOR_THRESHOLD                => Compare Calculating Gravity & User Setting Threshold Gravity 

--------------------------------------------------------------------------------*/

//#define G_SENSOR_EVENT_MODE     G_SENSOR_DELTA_TIME_AND_GRAVITY
#define G_SENSOR_EVENT_MODE     G_SENSOR_DELTA_GRAVITY
//#define G_SENSOR_EVENT_MODE     G_SENSOR_THRESHOLD


#if (G_SENSOR_EVENT_MODE == G_SENSOR_DELTA_TIME_AND_GRAVITY)
#define PI              3.1416
#endif

#define fabs(x) ((x) < 0 ?(-(x)) : (x))
#define VM_VALUE(value, bias) ((int)((bias - value ) * 250))

#define G_SENSOR_BW_2HZ	500 
#define G_SENSOR_BW_4HZ	250 
#define G_SENSOR_BW_5HZ	200 
#define G_SENSOR_BW_10HZ 	100 
#define G_SENSOR_BW_20HZ 	50  
#define G_SENSOR_BW_25HZ 	40 // 40ms
#define G_SENSOR_BW_50HZ 	20 // 20 ms
#define G_SENSOR_BW_100HZ 	10 // 10 ms

/**********************************************************
*  Global Variable
***********************************************************/
static float G_sensorValueX = 0.0;
static float G_sensorValueY = 0.0;
static float G_sensorValueZ = 0.0;
static  float l_pre_g_sensorTrgLevel = 0.0;
static u8 G_sensorInterrupt = 0;

/**********************************************************
* local variables
***********************************************************/
static float G_sensorInitialValueX;
static float G_sensorInitialValueY;
static float G_sensorInitialValueZ;
static BOOL  l_bG_sensorEventEnable;
static u32 l_g_sensor_event_time;

static ST_GSENSOR_CONTROL 	l_st_gsensor_ctrl = {FALSE, TRUE, 0, 0.5, 1.0, 1.5, 10, G_SENSOR_BOTTOM_POSITION_X, DX_ROTATE_0, 0.0, 0.0, 0.0};

#define PARKING_G_SENSOR_THRESHOLD_STEP		(0.01563)
#define PARKING_CHECK_COUNT_MAX 		((int)(CONFIG_PARKING_CHECK_TIME * l_st_gsensor_ctrl.iAutoPmEnterTime * 30)) // +-30 = 60sec = 1min
#define PARKING_CHECK_VALUE 			(PARKING_CHECK_COUNT_MAX - 10) //
#define PARKING_G_SENSOR_THRESHOLD			(0.007812)	//(0.0045)
#define PARKING_G_SENSOR_ERROR_THRESHOLD	(0.008)
#define PARKING_G_SENOSR_ERROR_CHECK_COUNT_MAX 		25 // 약 10 sec
#define PARKING_G_SENOSR_ERROR_CHECK_VALUE 			20 // -20 ~ +20 = 약 8 Sec

////
/* Kalman user define */
#define KAL_ERROR_ESTIMATED			((float) 0.98)
#define KAL_ERROR_MEASUREMENT			((float) 0.98)
#define KAL_NOISE_PROCESS			((float) 5.0)
#define KAL_NOISE_PROCESS_BIAS	((float) 0.5)

#ifdef DEF_SAFE_DRIVING_MONITORING
static ST_FILTER_VALUE l_st_filter_value = {0.0, };
static ST_FILTER_VALUE l_st_filter_value_bias = {0.0, };

void KalmanFilter(float *p_gValue, PST_FILTER_VALUE pF, float noise_process)
{
	float current_estimate, kalman_gain;
	int i;
	
	for(i=0; i < G_DATA_COUNT; i++){
		float g_v;
		if(i < 3) {
			g_v = p_gValue[i];
		}
		else { 
			g_v = fabs(p_gValue[0]) + fabs(p_gValue[1]) + fabs(p_gValue[2]);
			//noise_process /= 2.0;
		}
		
		kalman_gain = pF->err_est[i] /(pF->err_est[i] + KAL_ERROR_MEASUREMENT) ;
		current_estimate = pF->last_estimate[i] + kalman_gain*(g_v - pF->last_estimate[i]);
		pF->err_est[i] = (1.0-kalman_gain)*pF->err_est[i] + fabs(pF->last_estimate[i]-current_estimate)*noise_process;
		pF->last_estimate[i] = current_estimate;
	}
	
	return;
}
#endif

/* 진동으로 Parking 상태 check */
void G_senosr_parking_check(float gx, float gy, float gz)
{
	//DEBUGMSG(ZONE_GSENS_FUNC,("%d : %f %f %f (%d : %d)\r\n", l_st_gsensor_ctrl.parking, gx, gy, gz, l_st_gsensor_ctrl.parkingCheckCnt, PARKING_CHECK_VALUE));
	
	if(l_st_gsensor_ctrl.initial == FALSE)
	{
		G_sensor_SetInitialValue();
	}
	else if(l_bG_sensorEventEnable == TRUE)
	{
		static float parking_g_sensor_error_threshold = PARKING_G_SENSOR_ERROR_THRESHOLD;
		static int gsensor_error_check = 0;
		static u32 event_count = 0;

		// parking 상태 check
		if(gx > PARKING_G_SENSOR_THRESHOLD || gy > PARKING_G_SENSOR_THRESHOLD || gz >PARKING_G_SENSOR_THRESHOLD)
		{
			 if(l_st_gsensor_ctrl.parkingCheckCnt < PARKING_CHECK_COUNT_MAX)
			 {
			 	if( gx >= 0.012 || gy >= 0.012 || gy >= 0.012)
				 	l_st_gsensor_ctrl.parkingCheckCnt += (PARKING_CHECK_VALUE/5);
				else
					l_st_gsensor_ctrl.parkingCheckCnt++;
			 }
			 if(l_st_gsensor_ctrl.parking == TRUE && l_st_gsensor_ctrl.parkingCheckCnt >= (PARKING_CHECK_VALUE))
			 {
				 // normal mode
				 l_st_gsensor_ctrl.parking = FALSE;
				 DEBUGMSG(ZONE_GSENS_FUNC,(">>>>> Parking Mode (FALSE)\r\n"));
			 }
		}
		else
		{
		  	if(l_st_gsensor_ctrl.parkingCheckCnt > (-PARKING_CHECK_COUNT_MAX))
			  	l_st_gsensor_ctrl.parkingCheckCnt--;
		  	if(l_st_gsensor_ctrl.parking == FALSE && l_st_gsensor_ctrl.parkingCheckCnt <= (-PARKING_CHECK_VALUE))
		  	{
				// parking mode
				l_st_gsensor_ctrl.parking = TRUE;
				DEBUGMSG(ZONE_GSENS_FUNC,(">>>>> Parking Mode (TRUE)\r\n"));
		  	}
		}	 	

		// G-Sensor 오작동 check
		if( l_st_gsensor_ctrl.parking == TRUE)
		{
			if(gx > parking_g_sensor_error_threshold || gy > parking_g_sensor_error_threshold || gz > parking_g_sensor_error_threshold)
			{
				 if(gsensor_error_check < PARKING_G_SENOSR_ERROR_CHECK_COUNT_MAX)
				 {
					 gsensor_error_check++;
					 
					 if(gx >= 0.5 && l_st_gsensor_ctrl.parkingCheckCnt < PARKING_G_SENOSR_ERROR_CHECK_VALUE)
					 {
					 	gsensor_error_check += 3;
						if(gsensor_error_check > PARKING_G_SENOSR_ERROR_CHECK_VALUE)
							gsensor_error_check = PARKING_G_SENOSR_ERROR_CHECK_VALUE;
					 }
				 }
				 
				 if(gsensor_error_check == PARKING_G_SENOSR_ERROR_CHECK_VALUE)
				 {
					 // g-sensor error
					 l_st_gsensor_ctrl.trgLevel += PARKING_G_SENSOR_THRESHOLD_STEP;
					 parking_g_sensor_error_threshold += PARKING_G_SENSOR_THRESHOLD_STEP;
					 DEBUGMSG(ZONE_GSENS_ERROR, (" g-sensor check [ERROR] : %0.2f => %0.2f\n", l_st_gsensor_ctrl.trgLevel - PARKING_G_SENSOR_THRESHOLD_STEP, l_st_gsensor_ctrl.trgLevel));
					 gsensor_error_check = 0;
					 event_count = 0;

					//dxlog_add_ex(LOG_EVENT_GSENSOR, NULL, "0.%02d => 0.%02d", (int)((l_st_gsensor_ctrl.trgLevel - PARKING_G_SENSOR_THRESHOLD_STEP) * 100) % 100, (int)(l_st_gsensor_ctrl.trgLevel*100) % 100);
				 }
			}
			else
			{
			  	if(gsensor_error_check > (-PARKING_G_SENOSR_ERROR_CHECK_COUNT_MAX))
				  	gsensor_error_check--;
			}	 

			
			if(gx >= 0.5)
			{
				static s32 time_5minute = 0;
				time_t seconds = RtGetClockUtc();

				if(time_5minute < (seconds - 300)) //  5 * 60 = 5 min
					event_count = 0;

				if(event_count == 0)
					time_5minute = seconds;
					
				event_count ++;

				if(event_count > 10)
				{
					 l_st_gsensor_ctrl.trgLevel += PARKING_G_SENSOR_THRESHOLD_STEP;
					 parking_g_sensor_error_threshold += PARKING_G_SENSOR_THRESHOLD_STEP;
					 DEBUGMSG(ZONE_GSENS_ERROR, (" event count %d/5min : %0.2f => %0.2f\n", event_count, l_st_gsensor_ctrl.trgLevel - PARKING_G_SENSOR_THRESHOLD_STEP, l_st_gsensor_ctrl.trgLevel));
					 l_st_gsensor_ctrl.parkingCheckCnt = 0;
					 event_count = 0;
					 time_5minute = seconds;

					//dxlog_add_ex(LOG_EVENT_GSENSOR, NULL, "0.%02d => 0.%02d", (int)((l_st_gsensor_ctrl.trgLevel - PARKING_G_SENSOR_THRESHOLD_STEP) * 100) % 100, (int)(l_st_gsensor_ctrl.trgLevel * 100) % 100);
				}
			}
		}

	}
}

/*------------------------------------------------------------------------------
	   Function name   : G_sensor_EventBlockingTimeSet
	   Return			   : Boolean
	   Argument 	   :  ulMsTime
	   Comments 	   :  
------------------------------------------------------------------------------*/
BOOL G_sensor_EventBlockingTimeSet(u32 ulMsTime) // max (EVENT_IGNORE_TIME * 1000)
{
	if(l_st_gsensor_ctrl.trgLevel < 0.6) 
	{
		if((EVENT_IGNORE_TIME * 1000) > ulMsTime)
		{
			u32 currentTick = sys_clock_gettick();
			
			if(currentTick > ((EVENT_IGNORE_TIME * 1000) - ulMsTime))
			{
				l_g_sensor_event_time = currentTick - ((EVENT_IGNORE_TIME * 1000) - ulMsTime);
				l_bG_sensorEventEnable = FALSE;
				return TRUE;
			}
		}
	}
	return FALSE;
}


 /*------------------------------------------------------------------------------
	  Function name   : G_SensorDataSend
	  Return			  :  void
	  Argument		  :  
	  Comments		  :  
  ------------------------------------------------------------------------------*/
void G_SensorDataSend(u8 event, float x, float y, float z, float m)
{
#if 0
	ST_GSENSOR_DATA data;
	float ratio = 2.0/128.0;
	data.event_flag = event;
	data.ax[0] = (s8)(x / ratio);
	data.ay[0] = (s8)(y / ratio);
	data.az[0] = (s8)(z / ratio);
	data.accm = (s8)(m / ratio);
#endif
	//
}

 /*------------------------------------------------------------------------------
	  Function name   : G_sensor_XYZ
	  Return			  :  Boolean
	  Argument		  :  void
	  Comments		  :  offset value initial.
  ------------------------------------------------------------------------------*/
BOOL G_sensor_XYZ(gsensor_acc accel)
{
	BOOL ret = FALSE;
	float			sensitivity = 0;
	static float	G_sensorX = 0.0, G_sensorY = 0.0, G_sensorZ = 0.0;
//	static u32	bandwidth_50hz_time = 0;
	static u32 	set_bandwidth_time = 0;
	static u32 	set_eventcheck_time = 0;
//	u32	bw_time = G_SENSOR_BW_50HZ;
//	u32	bw_time = G_SENSOR_BW_10HZ;

	float delta_X;
	float delta_Y;
	float delta_Z;

	sensitivity = GSAMPLING_SENSITIVITY;
	
	l_st_gsensor_ctrl.X = G_sensorValueX = (float)accel.x / sensitivity; 
	l_st_gsensor_ctrl.Y = G_sensorValueY = (float)accel.y / sensitivity;
	l_st_gsensor_ctrl.Z = G_sensorValueZ = (float)accel.z / sensitivity;
	
	if(l_st_gsensor_ctrl.initial == FALSE)
	{
		G_sensor_SetInitialValue();
		return ret;
	}

#ifdef DEF_SAFE_DRIVING_MONITORING
 #ifdef DEF_OSAKAGAS_DATALOG
 	//if(util_IsTimerDiff(&set_eventcheck_time, 1000 - 10)){
 	//	safe_driving_monitoring_osakagas();
 	//}
 #else
/*	X + :  상승, - : 하강
		Y + : 좌회전, - : 우회전
		Z + : 출발 , - : 정지
*/
	KalmanFilter(&l_st_gsensor_ctrl.X, &l_st_filter_value, KAL_NOISE_PROCESS);
	KalmanFilter(&l_st_gsensor_ctrl.X, &l_st_filter_value_bias, KAL_NOISE_PROCESS_BIAS);

	//printf("x:%0.2f, y: %0.2f, z: %0.2f, a: %0.2f		",G_sensorValueX, G_sensorValueY, G_sensorValueZ, (fabs(G_sensorValueX) + fabs(G_sensorValueY) + fabs(G_sensorValueZ)));
	
	safe_driving_monitoring(VM_VALUE(l_st_filter_value.last_estimate[2], l_st_filter_value_bias.last_estimate[2]), \
		VM_VALUE(l_st_filter_value.last_estimate[1], l_st_filter_value_bias.last_estimate[1]), \
		VM_VALUE(l_st_filter_value.last_estimate[0], l_st_filter_value_bias.last_estimate[0]), \
		VM_VALUE(l_st_filter_value.last_estimate[3], l_st_filter_value_bias.last_estimate[3]));
 #endif
#endif

	//if(util_IsTimerDiff(&bandwidth_50hz_time, bw_time)) // g-sensor band width is 50 Hz
	{
			
#if 0
		DEBUGMSG(ZONE_GSENS_FUNC,("T:%d	x:%.03f ", sys_clock_gettick(), G_sensorValueX));
		DEBUGMSG(ZONE_GSENS_FUNC,("y:%.03f ", G_sensorValueY));
		DEBUGMSG(ZONE_GSENS_FUNC,("z:%.03f\n", G_sensorValueZ));
#endif

		delta_X = fabs(G_sensorValueX - G_sensorInitialValueX);
		delta_Y = fabs(G_sensorValueY - G_sensorInitialValueY);
		delta_Z = fabs(G_sensorValueZ - G_sensorInitialValueZ);
		

		G_sensorX = MAX_VALUE(delta_X, G_sensorX);
		G_sensorY = MAX_VALUE(delta_Y, G_sensorY);
		G_sensorZ = MAX_VALUE(delta_Z, G_sensorZ);

		ret = TRUE;
	}


	#define GSENSOR_LOW_SPEED_BW			G_SENSOR_BW_5HZ
 	if(util_IsTimerDiff(&set_bandwidth_time, GSENSOR_LOW_SPEED_BW))
	{
		static u32 bandwidth_count = 0;
		static float g_sensorTrgLevelX, g_sensorTrgLevelY, g_sensorTrgLevelZ;
#if (G_SENSOR_EVENT_MODE != G_SENSOR_THRESHOLD)
		float G_delta_X;
		float G_delta_Y;
		float G_delta_Z;
	 
		static float G_old_sensorValueX;
		static float G_old_sensorValueY;
		static float G_old_sensorValueZ; 
#endif    
	  	if( l_st_gsensor_ctrl.trgLevel != l_pre_g_sensorTrgLevel )
		{
			float trgLevelOffset;
			DEBUGMSG(ZONE_GSENS_INIT, ("%s(): G_sensorTrgLevel %0.02f => %0.02f\n", __func__, l_pre_g_sensorTrgLevel, l_st_gsensor_ctrl.trgLevel));
			l_pre_g_sensorTrgLevel = l_st_gsensor_ctrl.trgLevel;
			//if(pre_g_sensorTrgLevel > (G_SENSOR_MIN_TRGLEVEL / 0.7)) // G_SENSOR_MIN_TRGLEVEL 오작동이 없는 가장 작은 값임 
			//	trgLevelOffset = 0.7;
			//else
				trgLevelOffset = 1;

#if 0 //hw interrupt는 무시
			drv_gsensor_set_any_motion_threshold(l_pre_g_sensorTrgLevel + 1.0); //hw interrupt는 거의  무시
			drv_gsensor_setInterrupt(1);
			drv_gsensor_resetInterrupt();		
#if (BUILD_CONFIG_HW_VER == BUILD_HW_BASIC)		
			SetNvicIrqGInt(1);
#endif
#endif
			g_sensorTrgLevelX = l_pre_g_sensorTrgLevel * trgLevelOffset;
			g_sensorTrgLevelY = l_pre_g_sensorTrgLevel * trgLevelOffset;
			g_sensorTrgLevelZ = l_pre_g_sensorTrgLevel * trgLevelOffset;

			switch(l_st_gsensor_ctrl.position) // Z축은 약간 더 둔감하게 설정
			{
			case G_SENSOR_BOTTOM_POSITION_X:
				g_sensorTrgLevelX = l_pre_g_sensorTrgLevel + 1.3;
				break;
			case G_SENSOR_BOTTOM_POSITION_Y:
				g_sensorTrgLevelY = l_pre_g_sensorTrgLevel + 1.3;
				break;
			case G_SENSOR_BOTTOM_POSITION_Z:
				g_sensorTrgLevelZ = l_pre_g_sensorTrgLevel + 1.3;
				break;	
				
			default:
				break;
			}
		}
				
		if(l_bG_sensorEventEnable == FALSE)// 이벤트가 너무 자주 발생하는 것을 막기 위함.
		{
			if(util_IsTimerDiff(&l_g_sensor_event_time, EVENT_IGNORE_TIME * 1000)) // 3sec 
			{
				l_bG_sensorEventEnable = TRUE;
				G_sensorInterrupt = FALSE;
			}
		}	 

 #if (G_SENSOR_EVENT_MODE == G_SENSOR_DELTA_TIME_AND_GRAVITY)
		G_delta_X = atan((G_sensorX - G_old_sensorValueX)/(0.33)) * (180/PI);
		G_delta_Y = atan((G_sensorY - G_old_sensorValueY)/(0.33)) * (180/PI);
		G_delta_Z = atan((G_sensorZ - G_old_sensorValueZ)/(0.33)) * (180/PI);
#elif (G_SENSOR_EVENT_MODE == G_SENSOR_DELTA_GRAVITY)
		G_delta_X = G_sensorX - G_old_sensorValueX;
		G_delta_Y = G_sensorY - G_old_sensorValueY;
		G_delta_Z = G_sensorZ - G_old_sensorValueZ;
#endif

#if (G_SENSOR_EVENT_MODE != G_SENSOR_THRESHOLD)
		G_delta_X = fabs(G_delta_X);
		G_delta_Y = fabs(G_delta_Y);
		G_delta_Z = fabs(G_delta_Z);	
#else
		G_delta_X = G_sensorX;
		G_delta_Y = G_sensorY;
		G_delta_Z = G_sensorZ;
 #endif
 		G_senosr_parking_check(G_delta_X, G_delta_Y, G_delta_Z);	//relic 160615
 
		if(l_bG_sensorEventEnable == TRUE)
		{
			if(G_sensorInterrupt ||(G_delta_X > g_sensorTrgLevelX) || (G_delta_Y > g_sensorTrgLevelY) || (G_delta_Z > g_sensorTrgLevelZ))			  
			{
				float fMax_value = MAX_VALUE(MAX_VALUE(G_delta_X, G_delta_Y), G_delta_Z);

				G_sensorEventDet(G_sensorInterrupt, fMax_value);
			}

#if 0
#if (G_SENSOR_EVENT_MODE != G_SENSOR_THRESHOLD)
			printf("X = %0.2f  /  Y = %0.2f	/  Z = %0.2f \r\n",G_delta_X, G_delta_Y, G_delta_Z);
#else                
			printf("X = %0.2f  /  Y = %0.2f	/  Z = %0.2f \r\n",G_sensorX, G_sensorY, G_sensorZ);
#endif
#endif
		}

		if((bandwidth_count++ % (1000 / GSENSOR_LOW_SPEED_BW)) == 0)// 1 sec
		{
			float fMax_value = MAX_VALUE(MAX_VALUE(G_delta_X, G_delta_Y), G_delta_Z);
			G_SensorDataSend(0, G_sensorValueX, G_sensorValueY, G_sensorValueZ, fMax_value);
		}
		
#if (G_SENSOR_EVENT_MODE != G_SENSOR_THRESHOLD)
		G_old_sensorValueX = G_sensorX;
		G_old_sensorValueY = G_sensorY;
		G_old_sensorValueZ = G_sensorZ;
#endif
#if 0	
	  {
		static u32 freq_check_time = 0;
		static u32 freq_cnt = 0;

		freq_cnt++;
		if(util_IsTimerDiff(&freq_check_time, 100)) // 1sec 
		{
			printf("x:%0.2f, y: %0.2f, z: %0.2f	[%d] \r\n",G_sensorX, G_sensorY, G_sensorZ, freq_cnt);
			freq_cnt = 0;
		}
	  }
#endif
		G_sensorX = 0;
		G_sensorY = 0;
		G_sensorZ = 0;

		
		ret = TRUE;
	}

	return ret;
}


 /*------------------------------------------------------------------------------
	 Function name	 : G_sensor_SetInitialValue
	 Return 			 :  void
	 Argument		 :  void
	 Comments		 :  offset value initial.
 ------------------------------------------------------------------------------*/

void G_sensor_SetInitialValue(void)
{   
	#define DEF_MAX_RETRY_COUNT 100
	
	if(l_st_gsensor_ctrl.initial == FALSE)
	{
		static int retry = 0;
		float G_sensorX, G_sensorY, G_sensorZ;

#ifdef DEF_SAFE_DRIVING_MONITORING
		for( int i = 0; i < G_DATA_COUNT; i++ ){
			l_st_filter_value_bias.err_est[i] = l_st_filter_value.err_est[i] = KAL_ERROR_ESTIMATED;
		}
#endif

		if(retry == 0){
			DEBUGMSG(ZONE_GSENS_FUNC,("%s(): START \r\n", __func__ ));
		
			l_bG_sensorEventEnable = FALSE;
			l_g_sensor_event_time = sys_clock_gettick();

			G_sensorInitialValueX = G_sensorValueX;
			G_sensorInitialValueY = G_sensorValueY;
			G_sensorInitialValueZ = G_sensorValueZ; 
			retry++;
			return;
		}
		
	    if (retry++ < DEF_MAX_RETRY_COUNT)
	    {
	        //if(G_sensor_XYZ())
	        {
	        	if( (fabs(G_sensorValueX) + fabs(G_sensorValueY) + fabs(G_sensorValueZ)) < 0.8 )
	        	{
					l_st_gsensor_ctrl.initial = FALSE;
					DEBUGMSG(ZONE_GSENS_ERROR,("%s(): Initial ERROR RETRY(%d)!\r\n", __func__, retry ));
					return;
	        	}
				
	        	G_sensorX = fabs(G_sensorValueX - G_sensorInitialValueX);
				G_sensorY = fabs(G_sensorValueY - G_sensorInitialValueY);
				G_sensorZ = fabs(G_sensorValueZ - G_sensorInitialValueZ);

				if (G_sensorX < 0.05 && G_sensorY < 0.05 && G_sensorZ < 0.05)
				{
					float fMax_value;

					fMax_value = MAX_VALUE(MAX_VALUE(fabs(G_sensorInitialValueX), fabs(G_sensorInitialValueY)), fabs(G_sensorInitialValueZ));
						
					if(fabs(G_sensorInitialValueX) == fMax_value) {
						l_st_gsensor_ctrl.position = G_SENSOR_BOTTOM_POSITION_X;
						
						if(G_sensorInitialValueX > ROTATION_CHECK_VALUE)
							l_st_gsensor_ctrl.rotate = DX_ROTATE_180;
					}
					else if(fabs(G_sensorInitialValueY) == fMax_value) {
						l_st_gsensor_ctrl.position = G_SENSOR_BOTTOM_POSITION_Y;
					}
					else {
						l_st_gsensor_ctrl.position = G_SENSOR_BOTTOM_POSITION_Z;

						if(G_sensorInitialValueX > ROTATION_CHECK_VALUE)
							l_st_gsensor_ctrl.rotate = DX_ROTATE_180;
					}

					DEBUGMSG(ZONE_GSENS_FUNC,("++%s()\r\n	retry %d\r\n", __func__, retry));
					DEBUGMSG(ZONE_GSENS_FUNC,("	X : %0.2f\r\n", G_sensorInitialValueX));
					DEBUGMSG(ZONE_GSENS_FUNC,("	Y : %0.2f\r\n", G_sensorInitialValueY));
					DEBUGMSG(ZONE_GSENS_FUNC,("	Z : %0.2f\r\n", G_sensorInitialValueZ));
					DEBUGMSG(ZONE_GSENS_FUNC,("--%s(): g_sensor bottom position[%c] %d\r\n", __func__, \
						(l_st_gsensor_ctrl.position == G_SENSOR_BOTTOM_POSITION_X) ? 'X' : \
						(l_st_gsensor_ctrl.position == G_SENSOR_BOTTOM_POSITION_Y) ? 'Y' : 'Z', \
						l_st_gsensor_ctrl.rotate));
					
					l_st_gsensor_ctrl.initial = TRUE;

					l_pre_g_sensorTrgLevel = 0.0;
					
					DEBUGMSG(ZONE_GSENS_FUNC,("%s(): END(%d) \r\n", __func__, sys_clock_gettick() -  l_g_sensor_event_time));
					return;
			    }

				G_sensorInitialValueX = G_sensorValueX;
				G_sensorInitialValueY = G_sensorValueY;
				G_sensorInitialValueZ = G_sensorValueZ; 
				return;
	        }
			 return;
	    }
			
		if(retry >= DEF_MAX_RETRY_COUNT)
		{
			l_st_gsensor_ctrl.initial = FALSE;
			DEBUGMSG(ZONE_GSENS_ERROR,("%s(): Initial ERROR !\r\n", __func__ ));
		}
	}
}

void G_sensorEventISR(void)
{
	G_sensorInterrupt = 1;
}

/*------------------------------------------------------------------------------
    Function name   : G_sensorEventDet
    Return          : 
    Argument        : 
    Comments        :  G Sensor event process.
------------------------------------------------------------------------------*/
void G_sensorEventDet(BOOL bInterrupt, float accm)
{
	G_sensorInterrupt = bInterrupt;
	
	if(bInterrupt){
		G_senosr_parking_check(1, 1, 1);
		//drv_gsensor_resetInterrupt();		
	}

//	if( !bInterrupt && l_st_gsensor_ctrl.parking == TRUE) // SW Event는 무시하도록 함
//		return;
	
	if( l_bG_sensorEventEnable /*&& i_mcu_get_prebuzzer_cnt() == 0 */) // buzzer 동작중에 진동 때문에 G-Sensor 감지 막음
	{
		l_bG_sensorEventEnable = FALSE;
		l_g_sensor_event_time = sys_clock_gettick(); // 이벤트가 너무 자주 발생하는 것을 막기 위함.

#ifdef DEF_SAFE_DRIVING_MONITORING
		if(accm > l_st_gsensor_ctrl.trgLevelC)
			RecorderPostCommand(kStartEventGSensorLevelC);
		else if (accm > l_st_gsensor_ctrl.trgLevelB)
			RecorderPostCommand(kStartEventGSensorLevelB);
		else if (accm >= l_st_gsensor_ctrl.trgLevel)
			RecorderPostCommand(kStartEventGSensorLevelA);
#endif
		{
			// send event
			RecorderPostCommand(kStartEventRecording);
		}

		G_SensorDataSend(1, G_sensorValueX, G_sensorValueY, G_sensorValueZ, accm);
				
		if(bInterrupt)
			DEBUGMSG(ZONE_GSENS_INIT,(" <INT> ")); 
		else
			DEBUGMSG(ZONE_GSENS_INIT,(" <SW> ")); 
		
		DEBUGMSG(ZONE_GSENS_INIT,(" ----> G-Sensor EVENT detection. %.02fg (TrgLevel : %.02fg)\r\n", accm, l_st_gsensor_ctrl.trgLevel));
	}
}

/*------------------------------------------------------------------------------
    Function name   : G_sensorGetParkingMode
    Return          : 	BOOL 
    Argument        :  
    Comments        :  
------------------------------------------------------------------------------*/
BOOL G_sensorGetParkingMode( void )
{
	return l_st_gsensor_ctrl.parking;
}

void G_sensorParkingModeEnterTime(int min)
{
	l_st_gsensor_ctrl.iAutoPmEnterTime = min;
}
	
/*------------------------------------------------------------------------------
    Function name   : G_sensorSetTrgLevel
    Return          : 	float
    Argument        :  
    Comments        :  get/set trgLevel
------------------------------------------------------------------------------*/
float G_sensorSetTrgLevel( float levl, float levl_B, float levl_C )
{
	if( levl_B != 0.0)
		l_st_gsensor_ctrl.trgLevelB = levl_B;
	
	if( levl_C != 0.0)
		l_st_gsensor_ctrl.trgLevelC = levl_C;
	
	if( levl != 0.0)
		return l_st_gsensor_ctrl.trgLevel = levl;
	else
		return l_st_gsensor_ctrl.trgLevel;
}


/*------------------------------------------------------------------------------
    Function name   : G_sensorGetControl
    Return          : 
    Argument        : 
    Comments        : 
------------------------------------------------------------------------------*/
PST_GSENSOR_CONTROL G_sensorGetControl( void )
{
	return &l_st_gsensor_ctrl;
}

