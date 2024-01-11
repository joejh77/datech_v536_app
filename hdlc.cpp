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

#define ACK	0x06
#define NAK	0x15
#define STX	0x02
#define ETX	0x03


#define HDLC_ZONE_FUNC			0
#define HDLC_ZONE_INIT			1
#define HDLC_ZONE_ERROR		1

#define HDLC_SEND_CHAR		HdlcSendChar

#define HDLC_INITFCS16    0xffff  /* Initial FCS value */
#define HDLC_GOODFCS16    0xf0b8  /* Good final FCS value */
#define HDLC_END 0x7e
#define HDLC_ESC 0x7d

BOOL l_isHdlc_frame = FALSE;
int hdlc_fd = 0;
int mms_uart_baudrate = 115200;

u8 l_esc = 0;
u16 l_fcs = HDLC_GOODFCS16;


static const u_short fcstab[256] = {
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
* Calculate a new fcs given the current fcs and the new data.
*/
u16 pppfcs16Byte(u16 fcs, u8 cp)
{
	return (fcs >> 8) ^ fcstab[(fcs ^ cp) & 0xff];
}

u16 pppfcs16(u16 fcs, u8 *cp, int len)
{
	while (len--)
	{
		fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
	}

	return (fcs);
}

int HdlcGetChar(u8 *pucRxedChar, int xBlockTime)
{
	return read_serial(hdlc_fd, (char *)pucRxedChar, 1, xBlockTime);
}

void HdlcSendChar(u8 ucTxedChar)
{
	//DEBUGMSG(HDLC_ZONE_INIT, ("%02x ", ucTxedChar));
	return write_serial(hdlc_fd, (char *)&ucTxedChar, 1);
}

//return fcs
u16 HdlcSendPayload(u16 fcs, int len, u8 *data)
{
	int i;
	for(i=0; i < len; i++) {
		fcs = pppfcs16Byte(fcs, data[i]);
		
		if (data[i] == HDLC_END || data[i] == HDLC_ESC /* ||  d[i] < 0x20 */) {
			HDLC_SEND_CHAR(HDLC_ESC);
			HDLC_SEND_CHAR(data[i] ^ 0x20);
		}
		else {
			HDLC_SEND_CHAR(data[i]);
		}
	}

	return fcs;
}

u16 HdlcPreparePacket(u16 fcs, u8 *data, int len, u8 *dest, int *ret_dest_len, int dest_max_len)
{
	int i;
	for(i=0; i < len; i++) {
		fcs = pppfcs16Byte(fcs, data[i]);
		
		if (data[i] == HDLC_END || data[i] == HDLC_ESC /* ||  d[i] < 0x20 */) {
			dest[(*ret_dest_len)++] = HDLC_ESC;
			dest[(*ret_dest_len)++] = (data[i] ^ 0x20);
		}
		else {
			dest[(*ret_dest_len)++] = (data[i]);
		}

		if(*ret_dest_len >= dest_max_len){
			DEBUGMSG(HDLC_ZONE_ERROR, ("%s():dest buffer size error!!(%d:%d)\r\n", __func__, *ret_dest_len, dest_max_len));
			break;
		}
	}

	return fcs;
}

void HdlcSend(u8 *data, int len)
{
	u16 fcs = HDLC_INITFCS16;
	u8 d[4];
	int i;

	if(data == NULL)
		return;
	
	//Start flag Send
	HDLC_SEND_CHAR(HDLC_END);

	// payload Send
	fcs = HdlcSendPayload(fcs, len, data);

	//CRC 16 Send
	fcs ^= HDLC_INITFCS16;

	len = 0;
	fcs = HdlcPreparePacket(fcs, (u8 *)&fcs, 2, d, &len, sizeof(d));
	for(i = 0; i < len; i++)
		HDLC_SEND_CHAR(d[i]);	


	//Final flag Send
	HDLC_SEND_CHAR(HDLC_END);
}

//MMS 전송모듈 통신 패킷.docx 문서 참고
//|Start Flag   |  HDLC Frame payload                                        | CRC16	| Final flag |
// | 0x7E         |Type 2Byte | Length 2Byte | .... Data(Max 1024) |              | 0x7E       |
void HdlcSendPacket(u16 code, int len, u8 *data)
{

	u16 fcs = HDLC_INITFCS16;
	int i;
	u8	d[4];

	//Make Payload Header
	d[0] = (u8)(code & 0x00ff);
	d[1] = (u8)(code >> 8);
	d[2] = (u8)(len & 0x00ff);
	d[3] = (u8)(len >> 8);

	//Start flag Send
	HDLC_SEND_CHAR(HDLC_END);

	// payload Send
		// type, length Send
		fcs = HdlcSendPayload(fcs, sizeof(d), d);
		// data Send
		fcs = HdlcSendPayload(fcs, len, data);

	//CRC 16 Send
	fcs ^= HDLC_INITFCS16;

	len = 0;
	fcs = HdlcPreparePacket(fcs, (u8 *)&fcs, 2, d, &len, sizeof(d));
	for(i = 0; i < len; i++)
		HDLC_SEND_CHAR(d[i]);	


	//Final flag Send
	HDLC_SEND_CHAR(HDLC_END);
}

/*Uart use
HDLC FRAME Start {
 STX 	|	DEVICE ADDR R/nW	| REG ADDR	|	NUMBER OF BYTES	| ---- BYTE1 	|	BYTE n |	ETX
} HDCL FRAME End
*/
void HdlcSendPacket2(u8 dev_addr, u8 reg_addr, u16 length, u8 *data)
{
	u16 fcs = HDLC_INITFCS16;
	int i, len;
	u8 start_flag = STX;
	u8 end_flag = ETX;
	u8 d[4];


	//Make Payload Header
	d[0] = dev_addr;
	d[1] = reg_addr;
	d[2] = (u8)(length & 0x00ff);
	d[3] = (u8)(length >> 8);
	
	//Start flag Send
	HDLC_SEND_CHAR(HDLC_END);

	// payload Send
	fcs = HdlcSendPayload(fcs, 1, &start_flag);
	fcs = HdlcSendPayload(fcs, sizeof(d), d);
	
	// data payload Send
	if(length && data)
		fcs = HdlcSendPayload(fcs, length, data);

	fcs = HdlcSendPayload(fcs, 1, &end_flag);
	
	//CRC 16 Send
	fcs ^= HDLC_INITFCS16;

	len = 0;
	fcs = HdlcPreparePacket(fcs, (u8 *)&fcs, sizeof(fcs), d, &len, sizeof(d));
	for(i = 0; i < len; i++)
		HDLC_SEND_CHAR(d[i]);	

	//Final flag Send
	HDLC_SEND_CHAR(HDLC_END);
}

int HdlcReceive(u8 data, u8 *dest, int dest_max_len, int *p_dest_pos)
{
	int result = 0;

	static u16 headCmd;
	static u16 packetSize;
	static u32 packetpos = 0;
#if 1
	static u32 last_update_time_tic;
	u32 update_time_tic = get_tick_count();

	if(update_time_tic > (last_update_time_tic + TIMEOUT_INTER_CHARCTER)){
		packetpos = 0;
		*p_dest_pos = 0;
		l_fcs = HDLC_INITFCS16;
		l_esc = 0;
		l_isHdlc_frame = FALSE;
	}

	last_update_time_tic = update_time_tic;
#endif	
	
	if (data == HDLC_END) {
		
		if (HDLC_GOODFCS16 != l_fcs && HDLC_INITFCS16 != l_fcs) {
				DEBUGMSG(HDLC_ZONE_ERROR, ("%s():checksum error!!(0x%04x)\r\n", __func__, l_fcs));
				l_isHdlc_frame = FALSE;
		}
		else if(packetpos >= 2) {
			result = packetpos -2; // - l_fcs 2byte
			l_isHdlc_frame = FALSE;
		}
		else if(l_isHdlc_frame == FALSE)
			l_isHdlc_frame = TRUE;

		*p_dest_pos = 0;
		
		l_fcs = HDLC_INITFCS16;
		l_esc = 0;
		

		packetpos = 0;
	}
	else if (data == HDLC_ESC) {
		l_esc = 1;
	}
	else if(l_isHdlc_frame){
		if (l_esc) data = data ^ 0x20;
		l_esc = 0;

		packetpos++;
		
		l_fcs = pppfcs16Byte(l_fcs, data);

		if (*p_dest_pos < dest_max_len) {
			dest[*p_dest_pos] = data;
			(*p_dest_pos)++;
		}

		if (packetpos == 4) {
			headCmd = *((unsigned short *)&dest[0]);						
			packetSize = *((unsigned short *)&dest[2]);
		}
	}

	return result;
}

int HdlcFrameDecode(u8 *dest, int dest_max_size, u8 *src, int srclen)
{
	u16 fcs = HDLC_INITFCS16;
	u8 d;
	int i = 0, size = 0;
	int esc = 0;

	while (i < srclen)
	{
		d = src[i++];

		if (d == HDLC_END) {

			if (HDLC_GOODFCS16 != fcs || size < 2) {
				if(fcs != HDLC_INITFCS16)
					DEBUGMSG(HDLC_ZONE_ERROR, ("%s():checksum error!!\r\n", __func__));
				
				fcs = HDLC_INITFCS16;
				size = 0;
				esc = 0;
			}
			else {
				return size - 2;
			}
		}
		else if (d == HDLC_ESC) {
			esc = 1;
		}
		else {
			if (esc) d = d ^ 0x20;
			esc = 0;

			fcs = pppfcs16Byte(fcs, d);

			if (size < dest_max_size) {
				*dest++ = d;
				size++;
			}
		}
	}

	return 0;
}

int HdlcFrameEncode(u8 *dest, int dest_max_len, u8 *src, int srclen)
{
	u16 fcs;
	int i, flen = 0;
	u8 * packet_data = dest;
	int packet_pos = 0;
	
	fcs = HDLC_INITFCS16;
	packet_data[packet_pos++] = HDLC_END;

	fcs = HdlcPreparePacket(HDLC_INITFCS16, src, srclen, packet_data, &packet_pos, dest_max_len);

	fcs ^= 0xFFFF;
	//add crc
	fcs = HdlcPreparePacket(fcs, (u8 *)&fcs, 2, packet_data, &packet_pos, dest_max_len);

	packet_data[packet_pos++] = HDLC_END;


	return packet_pos;
}

BOOL Hdlc_is_hdlc_frame(void)
{
	return l_isHdlc_frame;
}

int Hdlc_serial_init(const char *dev_name, int baudrate)
{
	if(hdlc_fd)
		close_serial(hdlc_fd);
	
	hdlc_fd = open_serial(dev_name, baudrate, 1, 0, 8, 1);

	DEBUGMSG(HDLC_ZONE_INIT, ("%s(): %s %d (%d)\r\n", __func__, dev_name, baudrate, hdlc_fd));
	
	return hdlc_fd;
}
