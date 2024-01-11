/**
 *******************************************************************************
 * @file    serial.h
 * @author
 * @version v0.0.3
 * @date    June-20-2016
 *@brief This file contains the function prototypes of serial operation.
 *******************************************************************************
 */

#ifndef	_SERIAL_H_
#define	_SERIAL_H_

/**
 * @brief Communication interfaces, including serial and I2C ports
 * @defgroup INTERFACE_LIBRARY Interface Library
 * @{
 */

#include "datypes.h"

 
#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is to open a serial file.
 * @param dev_name 		The device name to be opened.
 * @param baud 			The baud rate.
 * @param stop 			The stop rate.
 * @param parity 		The parity value.
 * @param bytes 		The bite rate.
 * @param wait_ms 		Timeout for non-canonical read.
 * @return 			A file descriptor if success.
 */
extern int open_serial(const char *dev_name, int baud, int stop, int parity, int bytes,
                int wait_ms);

/**
 * This is to close a serial file.
 * @param fd 			The file descriptor.
 */
extern void close_serial(int fd);

/**
 * This is to clear the caching buffer of a file descriptor.
 * @param fd 			The file descriptor.
 */
extern void clear_serial(int fd);

/**
 * This is to read data from a serial port.
 * @param fd 			The serial file descriptor.
 * @param buf 			The buffer to hold the data read from serial port.
 * @param buf_limit 		The number of to-read data.
 * @param wait_msec 		Waiting time to receive next byte, in millisecond.
 * @return 			1 on success; 0 on error.
 */
extern int read_serial(int fd, char *buf, int buf_limit, int wait_msec);

/**
 * This is to write data into a serial port.
 * @param fd 			The serial file descriptor.
 * @param buf 			The buffer holding the data to write into serial port.
 * @param len 			The length of to-write data.
 * @return
 */
extern void write_serial(int fd, char *buf, int len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _SERIAL_H_ */
