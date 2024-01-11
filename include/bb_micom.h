
#if !defined(__BB_MICOM_H)
#define __BB_MICOM_H

#include <string>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"

#define LOW_BATTERY_MIN_VOLT		60 // 6V
//////////////////////////////////////
// PIC Sub MCU EEPROM TABLE

#define HW_CONFIG_REAR_CAM_TYPE_MASK		0x3

enum {
	EEPROM_INIT_TIME = 0,
	EEPROM_SERIAL_NO,
	EEPROM_HW_CONFIG,

	EEPORM_AUTO_ID, //pai-r use

	EEPROM_TABLE_END
};
/////////////////////////////////////

typedef enum {
	BB_MICOM_POWER_ON = 0,
	BB_MICOM_POWER_OFF_FALL,
	BB_MICOM_POWER_OFF_SLEEP, // low volt , high temp ...

	BB_MICOM_POWER_END	
}eBB_MICOM_POWER;

typedef struct 
{
	uint8_t 	LATA;
	uint8_t 	TRISA;
	uint8_t 	WPUA;
	uint8_t		ANSELA;


	uint8_t 	LATC;
	uint8_t 	TRISC;
	uint8_t 	WPUC;
	uint8_t		ANSELC;


	uint8_t		ADCON0;
	uint8_t		ADCON1;

/*10*/
	uint8_t		WATCHDOG_TM;		// 0 disable, Sec
	uint8_t		LOW_BATT_VOLT;	// 100 mv unit


	uint8_t		TEMP_SAFE_VALUE;		//adc value (8bit)
	uint8_t		TEMP_STABLE_VALUE;	//adc value (8bit)
	union{
		uint16_t	addr;		// 4byte data address 0: 0~3, 1: 4~7 ...
		uint8_t 	byte[sizeof(uint16_t)];
	}EEPROM_ADDR;
	union{
	uint32_t	data;		// u32 data
	uint8_t 	byte[sizeof(uint32_t)];
	}EEPROM_DATA;

/* 20 */	
	uint16_t	ADC6;			//BATTERY Level
	uint16_t	ADC7;			//TEMP

	
	uint16_t	ADC_VALUE; //ch 0 ~
	uint8_t		PORTA;
	uint8_t		PORTC;


	uint8_t		FIRMWARE_VER;
	uint8_t		SLAVE_ADDRESS;
	uint8_t		SLAVE_MASK;
	uint8_t		TIMER_1SEC;
	
	
	uint32_t	BATT_VOLT; //mv
	uint8_t		BATT_MODE; // 1 : 12v, 2:24v
	uint8_t		POWER_MODE; // eBB_MICOM_POWER
	
}I2C_REG;

typedef union {
	uint8_t 	byte[sizeof(I2C_REG)];
	I2C_REG st;
}BB_MICOM_REG;

class CBbMicom 
{
public:
	CBbMicom();
	virtual ~CBbMicom();
	
	int Initialize(void);
	int read(unsigned char reg_addr, char* buf, int readByteNum);
	int write(unsigned char reg_addr, char* buf, int writeByteNum);

	u32 GetEepromData(u16 addr);
	int SetEepromData(u16 addr, u32 data);
	
	double getTemp(u16 adc);
	double getTemp(void);
	double getBatteryVoltage(void);
	int SetWatchdogTme(u8 sec);
	int SetLowVoltage(u8 volt_100mv);
	int SetBatteryMode(u8 mode); // 1:12V, 2:24V
	int SetPowerMode(eBB_MICOM_POWER mode);
	
	// 100mv unit (+- 0.1v)
	void SetCarBatVoltageCalib(int volt) { m_iCarBatVoltageCalib = volt; }
	
	BB_MICOM_REG m_i2c_reg;
	double m_volt;
	double m_temp;
	
protected:
	bool m_is_init;
	int m_i2cFd;

	int m_iCarBatVoltageCalib; // +- 0.1V

	
};


int BB_MICOM_START(void);
int BB_MICOM_STOP(void);

int i2c_read(int fd, int slave_addr, unsigned char reg_addr, char* buf, int readByteNum);
int i2c_write(int fd, int slave_addr, unsigned char reg_addr, char* buf, int writeByteNum);
int i2c_open(const char *str_dev_name);
void i2c_close(int fd);


#endif // !defined(__BB_MICOM_H)

