#ifndef	_HDLC_H
#define	_HDLC_H

#include "datypes.h"
#include "recorder.h"
#include "user_data.h"


#define TIMEOUT_INTER_CHARCTER		50
#define TIMEOUT_RESPONSE 					200

#define  HDLC_PACKET_HEADER_SIZE		4 // code 2byte + length 2byte

#define	BB_REQ_IMAGE0					0x0000
#define	BB_REQ_IMAGE1					0x0001

#define	BB_ACK			0x0100
#define	BB_NAK			0x0101
#define 	BB_ERR			0x0102


#define BB_ERR_BUSY_TCP	1


//return write size
typedef int (*backup_cb_fn)(u8 *data, int *pData_len, u8 finished);

 #ifdef __cplusplus
extern "C" {
#endif

extern int HdlcGetChar(u8 *pucRxedChar, int xBlockTime);
extern void HdlcSendChar(u8 ucTxedChar);
extern u16 HdlcSendPayload(u16 fcs, int len, u8 *data);
extern u16 HdlcPreparePacket(u16 fcs, u8 *data, int len, u8 *dest, int *ret_dest_len, int dest_max_len);

//default use
extern void HdlcSend(u8 *data, int len);
extern int HdlcReceive(u8 data, u8 *dest, int dest_max_len, int *p_dest_pos);

//mms use
extern void HdlcSendPacket(u16 code, int len, u8 *data);
// linuxBD use
/*Uart use
HDLC FRAME Start {
 STX 	|	DEVICE ADDR R/nW	| REG ADDR	|	NUMBER OF BYTES	| ---- BYTE1 	|	BYTE n |	ETX
} HDCL FRAME End
*/
extern void HdlcSendPacket2(u8 dev_addr, u8 reg_addr, u16 length, u8 *data);

extern int HdlcFrameDecode(u8 *dest, int dest_max_size, u8 *src, int srclen);
extern int HdlcFrameEncode(u8 *dest, int dest_max_len, u8 *src, int srclen);
extern BOOL Hdlc_is_hdlc_frame(void);

extern int Hdlc_serial_init(const char *dev_name, int baudrate);

#ifdef __cplusplus
 }
#endif
#endif


