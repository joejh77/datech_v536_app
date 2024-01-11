#include <stdarg.h>
#include <fcntl.h>
#include <thread>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "datools.h"
#include "serial.h"
#include "sysfs.h"
#include "hdlc.h"
#include "evdev_input.h"
#include "mixwav.h"
#include "recorder.h"
#include "bb_api.h"

#define ZONE_BBAPI_FUNC 		1
#define ZONE_BBAPI_ERROR	 	1

#define ACK	0x06
#define NAK	0x15
#define STX	0x02
#define ETX	0x03

CBbApi::CBbApi()
{
	packet_index = 0;
	memset((void*)&sys_reg, 0x00, sizeof(sys_reg));
}

CBbApi::~CBbApi()
{

}


u8 CBbApi::SendHdlc(u8 *data, int length)
{
	HdlcSend(data, length);
	return 1;
}

u8 CBbApi::SendHdlc(u16 code, int length, u8 *data)
{
	HdlcSendPacket(code, length, data);
	return 1;
}

u8 CBbApi::SendHdlc(PST_DLB_PACKET pkt)
{
	u8 *pEtx = (&pkt->data) + pkt->length;
	pkt->stx = STX;
	
	*pEtx = ETX;

	return SendHdlc((u8 *)pkt, pkt->length + DLB_MIN_PACKET_SIZE);
}

u8 CBbApi::SendHdlc(u8 dev_addr, u8 reg_addr, u16 length, u8 *data)
{
	HdlcSendPacket2(dev_addr, reg_addr, length, data);

	return 1;
}

int CBbApi::SerialOpen(const char *dev_name, int baudrate)
{
	*pFd_Serial = Hdlc_serial_init(dev_name, baudrate);

	return *pFd_Serial;
}

bool CBbApi::SerialParser(char data)
{
	u32 packet_length = HdlcReceive((u8)data, packet_buf, sizeof(packet_buf), &packet_index);

	if(packet_length) {
			PacketParser(packet_buf, packet_length);
	}
	else if(packet_length == -1){
			u8 nak = NAK;
			SendHdlc(&nak, 1);
			DEBUGMSG(ZONE_BBAPI_ERROR, ("Error!! HDLC Packet\n"));
	}
	
	return Hdlc_is_hdlc_frame();
}

u8 CBbApi::PacketParser(u8 *buff, int length)
{
	u8 result = ACK;
	PST_DLB_PACKET pkt = (PST_DLB_PACKET)buff;
	u8 data_length = 0;
	u8 read_data;

	if(length == 1 && (buff[0] == NAK || buff[0] == ACK)) {
		DEBUGMSG(ZONE_BBAPI_FUNC, ("Received : 0x%02x\n", buff[0]));
		return buff[0];
	}
	else if(length < DLB_MIN_PACKET_SIZE) {
		result = NAK;
		DEBUGMSG(ZONE_BBAPI_ERROR, ("Error!! Packet size = %d\n", length));
		SendHdlc(&result, 1);
		return result;
	}

	read_data = pkt->dev_addr & RW_READ;
	
	pkt->dev_addr &= ~RW_READ;

	if(length > DLB_MIN_PACKET_SIZE)
		data_length = length - DLB_MIN_PACKET_SIZE;

	DEBUGMSG(ZONE_BBAPI_FUNC, ("Received Packet :	dev[%02x]	reg[%02x]	length = %d:%d\n", pkt->dev_addr, pkt->reg_addr, pkt->length, data_length));
	
	if(read_data || (pkt->length == data_length)){
		int i;
		u8 etx = buff[length - 1];
		u8 *pData = &pkt->data;
		
		if(pkt->stx == STX && etx == ETX) {		
			u8 start_reg = pkt->reg_addr;
			u8 write_data = 0;

			if(data_length >= 0 && data_length == pkt->length)
				write_data = 1;
			
			switch(pkt->dev_addr)
			{
			case DEF_DEVICE_ADDRESS_DEBUG_UART:
				if(write_data) {
					DEBUGMSG(1, ((char *)pData));
				}

				if(read_data){

				}
				break;

			case DEF_DEVICE_ADDRESS_SYSTEM:
				if(write_data) {
					for(i=0; i < pkt->length; i++)
						SystemSetRegData(start_reg++, pData[i]);
				}

				if(read_data) {
					u8 *pReg = SystemGetRegAddress(pkt->reg_addr);
					if(pReg){
						memcpy((void *)pData, pReg, pkt->length);
						SendHdlc(pkt);
					}
					else
						result = NAK;
				}
				
				break;
				
			case DEF_DEVICE_ADDRESS_SUBMICOM:
				if(write_data) {

				}
				
				if(read_data) {

				}
				break;

			default:
				result = NAK;
				DEBUGMSG(ZONE_BBAPI_ERROR, (" %s(): undefined device address !!(0x%02x)\n", __func__, pkt->dev_addr));
				break;
			}
		}
		else {
			result = NAK;
			DEBUGMSG(ZONE_BBAPI_ERROR, (" Packet Error!!(stx:0x%02x, etx:0x%02x)\n", pkt->stx, etx));
		}		
	}
	else {
		result = NAK;
		DEBUGMSG(ZONE_BBAPI_ERROR, (" Packet Length Error!!%d(pkt length:%d)\n", length, pkt->length));
	}

	if(!read_data || result == NAK)
		SendHdlc(&result, 1);
	
	return result;
}

bool CBbApi::SendPicture(int camera_id, u16 width, u16 height, u8 quality)
{
	bool detected = false;
	
	oasis::key_value_map_t parameters;
	parameters.clear();
	parameters["width"] = std::to_string(width);
	parameters["height"] = std::to_string(height);
	parameters["quality"] = std::to_string(quality);
	parameters["timeout"] = "1000"; //wait timeout max in msec

	std::vector<char> image_data;
	struct timeval timestamp;
	if(oasis::takePicture(camera_id, parameters, image_data, &timestamp) == 0 && image_data.size() > 0) {
		u32 tick = get_tick_count();
		SendHdlc(BB_REQ_IMAGE0 + camera_id, image_data.size(), (u8 *)image_data.data());
		DEBUGMSG(ZONE_BBAPI_FUNC, ("%s() : send jpg %d byte(%d ms)\n", __func__, image_data.size(), get_tick_count() - tick));
		detected = true;
	}
	return detected;
}

void CBbApi::SystemRegUpdate(void)
{
	const char * pVer = __FW_VERSION__;
	const char *pDot;
	
	CPulseData pulse = evdev_input_get_pulsedata();
	ST_RECORDER_PARAM * pRec = RecorderGetParam();
	PST_GSENSOR_CONTROL gctl = G_sensorGetControl();

	pDot = strchr(pVer, '.');
	if(pDot){
		char buf[3] = {0};
		strncpy(buf, pVer, pDot - pVer);
		sys_reg.bVerMain = atoi(buf);
		pDot++;

		pVer = pDot;
		pDot = strchr(pVer, '.');
		if(pDot){
			char buf2[3] = {0};
			strncpy(buf2, pVer, pDot - pVer);
			sys_reg.bVerSub = atoi(buf2);
			pDot++;

			sys_reg.bVerSub2 = atoi(pDot);
		}
	}
	
	sys_reg.bIntDet.b.Br = pulse.m_bBrk;		//DITYPE
	sys_reg.bIntDet.b.R = pulse.m_bSR;
	sys_reg.bIntDet.b.L = pulse.m_bSL;
	sys_reg.bIntDet.b.Bg = pulse.m_bBgr;
	sys_reg.bIntDet.b.Tr = pulse.m_bTR;
	
	//sys_reg.bReqPowerOff;
	sys_reg.sd_det = *pRec->pSd_exist;
	sys_reg.net_det = *pRec->pNet_exist;
#if DEF_SUB_MICOM_USE	
	sys_reg.ext_batt_volt = (u16)(pRec->pSubmcu->m_volt * 100);		// x100 (Unit:10mV)
	sys_reg.int_batt_volt = 0;		// x100
	sys_reg.ext_adc1_volt = 0;		// x100

	sys_reg.bd_temperature= (u16)(pRec->pSubmcu->m_temp * 100);	// x100
#endif

	sys_reg.speed_pulse = pulse.m_iPulseSec;
	sys_reg.rpm_count = pulse.m_iRpmPulseSec;

	sys_reg.test_result.d32 = pRec->pTestResult->d32;

	sys_reg.x = gctl->X;
	sys_reg.y = gctl->Y;
	sys_reg.z = gctl->Z;

	sys_reg.serial_no = pRec->pRec_env->serial_no;
}

//return RW_READ is read only register, RW_WRITE is write register data
u8 CBbApi::SystemSetRegData(u8 addr, u8 data)
{
	u8 result = RW_READ;
	u8 ack = ACK;
	
	if(addr < sizeof(sys_reg)) {
		u8 *pRegBuff = (u8 *)&sys_reg;
		if(pRegBuff[addr] == data && addr < eSystemReg_write_only)
			return RW_READ;
		
		//control register
		switch(addr)
		{
		case eSystemReg_serial_no:
		case eSystemReg_serial_no + 1:
		case eSystemReg_serial_no + 2:
		case eSystemReg_serial_no + 3:
			result = RW_WRITE;
			
			if(addr == eSystemReg_serial_no + 3){
				pRegBuff[addr] = data;

				RecorderSetSerialNo(sys_reg.serial_no);
			}
			break;
			
		case eSystemReg_led:
			if((pRegBuff[addr] & LED_REC) != (data & LED_REC))
				datool_led_red_set((BOOL)(data & LED_REC));
					

			if((pRegBuff[addr] & LED_GPS) != (data & LED_GPS))
				datool_led_blue_set((BOOL)(data & LED_GPS));

			if((pRegBuff[addr] & LED_NET) != (data & LED_NET))
				datool_led_green_set((BOOL)(data & LED_NET));
			
			result = RW_WRITE;
			break;
		
		case eSystemReg_output:		//Bit 0 . relay 1, Bit 1 . relay 2
			if((pRegBuff[addr] & DO_TO) != (data & DO_TO))
				datool_ext_trigger_out_set((BOOL)(data & DO_TO));
					

			if((pRegBuff[addr] & DO_SL) != (data & DO_SL))
				datool_ext_speed_limit_out_set((BOOL)(data & DO_SL));
			
			result = RW_WRITE;
			break;

		case eSystemReg_jpg_width:
		case eSystemReg_jpg_width + 1:
		case eSystemReg_jpg_height:
		case eSystemReg_jpg_height + 1:
		case eSystemReg_jpg_quality:
			result = RW_WRITE;
			break;
			
		case eSystemReg_get_jpg:
			SendPicture(data, sys_reg.jpg_width, sys_reg.jpg_height, sys_reg.jpg_quality);
			break;

		case eSystemReg_sound:
			mixwav_play((eMIXWAVECMD)data);
			break;

		case eSystemReg_test_mode:
			SendHdlc(&ack, 1);
			
			if(data)
				this->SerialOpen(GPS_DEVICE, 19200); //921600
			else
				this->SerialOpen(GPS_DEVICE, 9600);
				
			result = RW_WRITE;
			break;
			
		case eSystemReg_reset:
			recorder_reboot();			
			break;
			
		default :
			break;
		}

		if(result == RW_WRITE)
			pRegBuff[addr] = data;
	}
	
	return result;
}

u8 CBbApi::SystemGetRegData(u8 addr)
{
	if(addr < sizeof(sys_reg)){
		u8 *pReg = (u8 *)&sys_reg;	
		
		return pReg[addr];
	}

	return 0;
}

u8 *CBbApi::SystemGetRegAddress(u8 addr)
{
	SystemRegUpdate();
	if(addr < sizeof(sys_reg)) {
		u8 *pReg = (u8 *)&sys_reg;	
		return &pReg[addr];
	}

	return 0;
}


