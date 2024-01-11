#ifndef	_BB_API_H
#define	_BB_API_H

#include <string>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"
#include "user_data.h"

#define DLB_MIN_PACKET_SIZE		6
#define DLB_MAX_DATA_SIZE		(256)

#define DEF_DEVICE_ADDRESS_DEBUG_UART		0x00	//0x00 Write, 0x01:Read
#define DEF_DEVICE_ADDRESS_SYSTEM				0x32	//0x32 Write, 0x33:Read
#define DEF_DEVICE_ADDRESS_SUBMICOM			0x34	
#define DEF_DEVICE_ADDRESS_GPS						0x46
#define DEF_DEVICE_ADDRESS_PULSE					0x48
#define DEF_DEVICE_ADDRESS_WIFI						0x50

/* Define -------------------------------------------------------------------*/
/*protocol format
HDLC FRAME Start {
 STX 	|	DEVICE ADDR R/nW	| REG ADDR	|	NUMBER OF BYTES (U16)	| ---- BYTE1 	|	BYTE n |	ETX
} HDCL FRAME End
*/
#pragma pack(push, 1)

typedef struct tagDlbPacket
{
	u8 	stx;
	u8 	dev_addr;
	u8	reg_addr;
	u16	length;
	u8	data;	//data 0
	//.... data n
	// etx;
}ST_DLB_PACKET, *PST_DLB_PACKET;
#pragma pack(pop)

enum RWTYPE
{
	RW_WRITE = 0x00
	,RW_READ = 0x01
};

enum LEDTYPE
{
    LED_REC= 0x01	//
    ,LED_GPS = 0x02 //
    ,LED_NET = 0x04 //
};

enum DITYPE
{
    DI_BRK = 0x01	//
    ,DI_SR = 0x02 //
    ,DI_SL = 0x04	//
    ,DI_BGR = 0x08//
    ,DI_TR = 0x10//
};
typedef union{
	u8 d8;
	struct {
	u8 Br  : 1;
	u8 R  : 1;
	u8 L  : 1;
	u8 Bg  : 1;
	u8 Tr  : 1;
	u8 tmp : 3;
	}b;
}ST_DIGITAL_IN;


enum DOTYPE
{
    DO_TO= 0x01	// trigger out
    ,DO_SL = 0x02 // speed limit
};

//DEF_DEVICE_ADDRESS_STM32_SYSTEM register
typedef enum{
	eSystemReg_mainver = 0
	,esystemReg_subver
	,esystemReg_subver2
	
	,eSystemReg_intdet = 4
	,eSystemReg_reqpoweroff
	,eSystemReg_sd_det
	,eSystemReg_net_det

	,eSystemReg_extbattvolt = 8
	,eSystemReg_intbattvolt = 10
	,eSystemReg_extadc1tvolt = 12

	,eSystemReg_bdtemperature = 14
	
	,eSystemReg_speed_pulse = 16
	,eSystemReg_rpm_count = 18

	,eSystemReg_test_result = 20
	,eSystemReg_gsensor_x = 24
	,eSystemReg_gsensor_y = 28
	,eSystemReg_gsensor_z = 32

	,eSystemReg_serial_no = 36
	
	,eSystemReg_led			= 40
	,eSystemReg_output 		//Bit 0 . trigger out, Bit 1 . speed limit

	,eSystemReg_jpg_width = 44
	,eSystemReg_jpg_height = 46

	,eSystemReg_jpg_quality = 48
	,eSystemReg_get_jpg					,eSystemReg_write_only = eSystemReg_get_jpg
	,eSystemReg_sound
	,eSystemReg_test_mode
	,eSystemReg_reset 
	
	
	,eControlReg_end = 255
}eSystemReg;

typedef union{
	u32 d32;
	struct {
	u32 rtc : 1;
	u32 wifi  : 1;
	u32 setup  : 1;
	u32 pulse  : 1;
	u32 pulse_Rpm  : 1;
	u32 pulse_Br  : 1;
	u32 pulse_R  : 1;
	u32 pulse_L  : 1;
	u32 pulse_Bg  : 1;
	u32 pulse_T  : 1;
	u32 gps  : 1;
	u32 gsensor  : 1;
	u32 submcu  : 1;
	u32 ext_cam  : 1;
	}b;
}ST_TEST_RESULT;



typedef struct{
// read only  {	
	u8 bVerMain;
	u8 bVerSub;
	u8 bVerSub2;
	u8 reserved1[1];
	
	ST_DIGITAL_IN bIntDet;		//DITYPE
	u8 bReqPowerOff;
	u8 sd_det;
	u8 net_det;

	u16 ext_batt_volt;		// x100 (Unit:10mV)
	u16 int_batt_volt;		// x100
	u16 ext_adc1_volt;		// x100

	s16 bd_temperature;	// x100

	u16 speed_pulse;
	u16 rpm_count;

	ST_TEST_RESULT test_result;

	float x;
	float y;
	float z;

	u32 serial_no;
//}

// rw control {
	u8 bLed;			//LEDTYPE
	u8 bOutput; 	//DOTYPE
	u8 reserved4_1[2];


	u16 jpg_width;
	u16 jpg_height;

	u8 jpg_quality;
	u8 bJpg;
	u8 bSound;
	u8 bTest_mode;
	
	u8 bReset;
	u8 reserved4[3];
//}
}SYSTEM_REG, *P_SYSTEM_REG;


class CBbApi
{
public:
	CBbApi();
	virtual ~CBbApi();
	void SerialHandleSet(int *pFd) { pFd_Serial = pFd; }
	u8 SendHdlc(u8 *data, int length);
	u8 SendHdlc(u16 code, int length, u8 *data);
	u8 SendHdlc(PST_DLB_PACKET pkt);
	u8 SendHdlc(u8 dev_addr, u8 reg_addr, u16 length, u8 *data);

	int SerialOpen(const char *dev_name, int baudrate);
	bool SerialParser(char data);
	u8 PacketParser(u8 *buff, int length);

	bool SendPicture(int camera_id, u16 width, u16 height, u8 quality = 50);
	void SystemRegUpdate(void);
	u8 SystemSetRegData(u8 addr, u8 data);
	u8 SystemGetRegData(u8 addr);
	u8 *SystemGetRegAddress(u8 addr);
	
	SYSTEM_REG sys_reg;
	
protected:
	int *pFd_Serial;
	u8 packet_buf[DLB_MAX_DATA_SIZE + DLB_MIN_PACKET_SIZE];
	int packet_index;

	
};


#endif 

