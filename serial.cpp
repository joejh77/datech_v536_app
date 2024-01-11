/**
 **********************************************************************************
 * @file    serial.c
 * @author  
 * @version v0.0.0
 * @date    June-20-2016
 * @brief   This file provides the functions of serial operation.
 **********************************************************************************
 */

#include <termios.h>
#include <fcntl.h>

#include "OasisAPI.h"
#include "OasisLog.h"
#include "datools.h"
#include "serial.h"

//open_serial("/dev/ttyS2", 9600, 1, 0, 8, 1)
int open_serial(const char *dev_name, int baud, int stop, int parity, int bytes,
                int wait_ms)
{
	int retry = 3;
	int fd;
	struct termios newtio;

serial_reopen:
	// Not select NDELAY mode, in order to set wait-time VTIME later
	fd = open(dev_name, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		DLOG(DLOG_ERROR, "Device OPEN FAIL %s\n", dev_name);
		return -1;
	}

  /*
   IGNPAR   : Parity 에러가 있는 문자 바이트를 무시한다.
   ICRNL    : CR 문자를 NL 문자로 변환 처리한다. (이 설정을 안하면 다른
              컴퓨터는 CR 문자를 한 줄의 종료문자로 인식하지 않을 수 있다.)
    otherwise make device raw (no other input processing)
  */
  
/*
    BAUDRATE: 전송 속도. cfsetispeed() 및 cfsetospeed() 함수로도 세팅 가능
    CRTSCTS : 하드웨어 흐름 제어. (시리얼 케이블이 모든 핀에 연결되어 있는
              경우만 사용하도록 한다. Serial-HOWTO의 7장을 참조할 것.)
    CS8     : 8N1 (8bit, no parity, 1 stopbit)
    CLOCAL  : Local connection. 모뎀 제어를 하지 않는다.
    CREAD   : 문자 수신을 가능하게 한다.
  */
  

	memset(&newtio, 0, sizeof(newtio));
	newtio.c_iflag = IGNPAR;  // non-parity
	newtio.c_oflag = 0;

	//newtio.c_cflag = CS8 | CLOCAL | CREAD; // NO-rts/cts 
	newtio.c_cflag = CLOCAL | CREAD;

	if (bytes == 7)
		newtio.c_cflag |= CS7;
	else
		newtio.c_cflag |= CS8;

	if (stop == 2)
		newtio.c_cflag |= CSTOPB;  //CSTOPB:stop 2bit

	if (parity == 2)
		newtio.c_cflag |= PARENB;  //parity enable (even)
	else if (parity == 1)
		newtio.c_cflag |= (PARODD | PARENB);

	switch (baud) {
	case 921600 :
		newtio.c_cflag |= B921600;
		break;
	case 576000 :
		newtio.c_cflag |= B576000;
		break;
	case 460800 :
		newtio.c_cflag |= B460800;
		break;
	case 230400 :
		newtio.c_cflag |= B230400;
		break;
	case 115200 :
		newtio.c_cflag |= B115200;
		break;
	case 57600 :
		newtio.c_cflag |= B57600;
		break;
	case 38400 :
		newtio.c_cflag |= B38400;
		break;
	case 19200 :
		newtio.c_cflag |= B19200;
		break;
	case 9600 :
		newtio.c_cflag |= B9600;
		break;
	case 4800 :
		newtio.c_cflag |= B4800;
		break;
	case 2400 :
		newtio.c_cflag |= B2400;
		break;
	case 1200 :
		newtio.c_cflag |= B1200;
		break;
	default :
		newtio.c_cflag |= B115200;
		break;
	}

	if (wait_ms == 0)
		wait_ms = 1;

//	newtio.c_lflag &= ~(ICANON | ECHO);	//non-canonical mode
//	newtio.c_lflag |= (ICANON | ECHO);	//canonical mode
//	newtio.c_lflag &= ~ISIG;		//Ctrl+C, will not be able to terminate

	newtio.c_lflag = 0;

	newtio.c_cc[VTIME] = wait_ms;
	newtio.c_cc[VMIN] = 0;

	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd, TCSANOW, &newtio) < 0){
		DLOG(DLOG_ERROR, "%s tcsetattr setting error!\n", dev_name);
		
		close(fd);
		msleep(500);
		
		if(retry--)
			goto serial_reopen;
		else
			fd = -1;
	}

	return fd;
}

void close_serial(int fd)
{
	close(fd);
}

void clear_serial(int fd)
{
	char cc;

	while (read_serial(fd, &cc, 1, 0) > 0) {
	}
}

int read_serial(int fd, char *buf, int buf_limit, int wait_msec)
{
	int ret, rcv_len = 0;

	while (1) {

		ret = read(fd, buf, buf_limit - rcv_len);

		if (ret <= 0)
			break;
		buf += ret;
		rcv_len += ret;
		if (wait_msec <= 0)
			break;
		msleep(1);
		wait_msec--;
	}

	return rcv_len;
}

void write_serial(int fd, char *buf, int len)
{
	int ret;
	short cnt = 0;

	while (1) {
		ret = write(fd, buf, len);
		if (ret > 0) {
			len -= ret;
			buf += ret;
		}

		if (len > 0) {
			if (++cnt > 10)
				return;	// max wait time of 0.1 sec
			DLOG(DLOG_ERROR, "Write-times = %d, remain-len = %d\n", cnt, len);
			msleep(10);	// delay of 10 msec
		}
		else
			break;
	}
}
