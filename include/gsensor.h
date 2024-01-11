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
#ifndef __APP_GSENSOR_H__
#define __APP_GSENSOR_H__
#include "datypes.h"

/**********************************************************
 * definitions
***********************************************************/

#define MAX_VALUE(a,b)((a>b)? a : b)

#define CONFIG_PARKING_CHECK_TIME		2	// 2 Hz
#define G_SENSOR_MIN_TRGLEVEL		(0.015625 * 2)
#define EVENT_IGNORE_TIME           3 // sec
 
/*************************************
 * structures
 *************************************/
typedef enum {
	G_SENSOR_BOTTOM_POSITION_X,
	G_SENSOR_BOTTOM_POSITION_Y,
	G_SENSOR_BOTTOM_POSITION_Z,
	G_SENSOR_BOTTOM_POSITION_END
} g_sensor_bottom_position;

typedef enum {
	G_SENSOR_POSITION_XP,
	G_SENSOR_POSITION_XM,
	G_SENSOR_POSITION_YP,
	G_SENSOR_POSITION_YM,
	G_SENSOR_POSITION_ZP,
	G_SENSOR_POSITION_ZM,
	
	G_SENSOR_POSITION_NULL,
	G_SENSOR_POSITION_END
} g_sensor_position;

typedef enum {
	G_SENSOR_MOTION_LEFT,
	G_SENSOR_MOTION_RIGHT,
	G_SENSOR_MOTION_UP,
	G_SENSOR_MOTION_DOWN,
	G_SENSOR_MOTION_PUSH,
	G_SENSOR_MOTION_PULL,
	G_SENSOR_MOTION_L_CIRCLE,
	G_SENSOR_MOTION_R_CIRCLE,

	G_SENSOR_MOTION_NULL,
	G_SENSOR_MOTION_END
} g_sensor_motion;

typedef enum {
	DX_ROTATE_0 = 0,
	DX_ROTATE_90,
	DX_ROTATE_180,
	DX_ROTATE_270,
	
	DX_ROTATE_END
}DX_ROTATE;

 typedef struct gsensor_motion_info
 {
	 g_sensor_position pos;
	 float value;
 } stGSENSOR_MOTION_INFO;

 typedef struct tagGsensorControl
 {
	 BOOL				   	initial;
	 BOOL					parking; // 진동으로 parking mode check
	 int					parkingCheckCnt;
	 float	 				trgLevel; //Level A
	 float	 				trgLevelB; //Level B
	 float	 				trgLevelC;	//Level C
	 int						iAutoPmEnterTime;
	 g_sensor_bottom_position 	 	position;
	 DX_ROTATE				rotate;

	 float					X;
	 float					Y;
	 float					Z;
 } ST_GSENSOR_CONTROL, * PST_GSENSOR_CONTROL;

#define G_DATA_COUNT		4
 typedef struct
 {
 		float err_est[G_DATA_COUNT];
		
 		float	last_estimate[G_DATA_COUNT]; // g value X,Y,Z and without gravity
 } ST_FILTER_VALUE, * PST_FILTER_VALUE;
 
#define GSENSOR_DATA_SIZE		1
 typedef struct tagGsensorData
 {
	u8 event_flag;
	u8 accm;	// unit : 2g /128
	s8 ax[GSENSOR_DATA_SIZE];	// unit : 2g /128
	s8 ay[GSENSOR_DATA_SIZE];	// unit : 2g /128
	s8 az[GSENSOR_DATA_SIZE];	// unit : 2g /128
 } ST_GSENSOR_DATA, * PST_GSENSOR_DATA;

typedef struct {
	short 	x;
	short	y;
	short	z;
}GVALUE, gsensor_acc;

#ifdef TARGET_CPU_V536	
#define GSAMPLING_SENSITIVITY		1024.0
#else
#define GSAMPLING_SENSITIVITY		256.0 // 10bit 2g sensitivity 256 , 4g = 128, 8g =  64
#endif

 /*************************************
 * functions
 *************************************/
 #ifdef __cplusplus
extern "C" {
#endif

//void G_sensorSpiInit(void);
extern BOOL G_sensor_EventBlockingTimeSet(u32 ulMsTime);
extern BOOL G_sensor_XYZ(gsensor_acc accel);
extern void G_sensor_SetInitialValue(void);
void G_sensorEventISR(void);
extern void G_sensorEventDet(BOOL bInterrupt, float accm);
extern BOOL G_sensorGetParkingMode( void );
extern void G_sensorParkingModeEnterTime(int min);
extern float G_sensorSetTrgLevel( float levl, float levl_B, float levl_C );
extern PST_GSENSOR_CONTROL G_sensorGetControl( void );

#ifdef __cplusplus
 }
#endif
#endif /* end of __APP_GSENSOR_H__ */
