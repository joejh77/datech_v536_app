// pai_r_data.cpp: implementation of the CPai_r_data class.
//
//////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include <fcntl.h>
#include <thread>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "datools.h"
#include "sysfs.h"
#include "bb_micom.h"

#define DBG_BB_M_FUNC 	1 // DBG_MSG
#define DBG_BB_M_INIT 	1 // DBG_MSG
#define DBG_BB_M_ERR  		DBG_ERR
#define DBG_BB_M_WRN		DBG_WRN

#define BB_MICOM_SLAVE_ADDRESS 0x8

#define BbMicom_get_reg_address(reg) (unsigned char)((u32)&reg) - (((u32)&m_i2c_reg))
#define BbMicom_get_reg_data(reg)			read(BbMicom_get_reg_address(reg), (char* )&reg, sizeof(reg))
#define BbMicom_set_reg_data(reg) write(BbMicom_get_reg_address(reg), (char* )&reg, sizeof(reg))

CBbMicom::CBbMicom()
{
	m_is_init = false;
	m_i2cFd = 0;
	m_iCarBatVoltageCalib = 0;
	m_volt = 0.0;
	m_temp= 0.0;
}

CBbMicom::~CBbMicom()
{
	if(m_is_init){
		
	}

	i2c_close(m_i2cFd);
}

int CBbMicom::Initialize(void)
{
	m_i2cFd = i2c_open("/dev/i2c-0");

	if(m_i2cFd){
		if(read(0, (char* )m_i2c_reg.byte, sizeof(m_i2c_reg)) > 0){
			m_is_init = true;
			dbg_printf(DBG_BB_M_INIT, "%s() : Sub Micom Ver:%d (%f V, Temp %f C)\r\n", __func__, m_i2c_reg.st.FIRMWARE_VER, (double)m_i2c_reg.st.BATT_VOLT/1000.0, getTemp(m_i2c_reg.st.ADC7));
		}
	}

	return m_i2cFd;
}

int CBbMicom::read(unsigned char reg_addr, char* buf, int readByteNum)
{
    return i2c_read(m_i2cFd, BB_MICOM_SLAVE_ADDRESS, reg_addr, buf, readByteNum);;
}


int CBbMicom::write(unsigned char reg_addr, char* buf, int writeByteNum)
{
    return i2c_write(m_i2cFd, BB_MICOM_SLAVE_ADDRESS, reg_addr, buf, writeByteNum);
}


u32 CBbMicom::GetEepromData(u16 addr)
{
	m_i2c_reg.st.EEPROM_DATA.data = 0;
	m_i2c_reg.st.EEPROM_ADDR.addr = addr;
	
	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_ADDR.addr);
	msleep(50);
	
	if(BbMicom_get_reg_data(m_i2c_reg.st.EEPROM_DATA.data) > 0){
		dbg_printf(DBG_BB_M_FUNC, "%s() : %08x \r\n", __func__, m_i2c_reg.st.EEPROM_DATA.data);
	}

	return m_i2c_reg.st.EEPROM_DATA.data;
}

int CBbMicom::SetEepromData(u16 addr, u32 data)
{
	m_i2c_reg.st.EEPROM_ADDR.addr = addr;
	m_i2c_reg.st.EEPROM_DATA.data = data;
	
	
	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_ADDR.addr);
	msleep(50);

	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_DATA.byte[0]);
	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_DATA.byte[1]);
	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_DATA.byte[2]);
	BbMicom_set_reg_data(m_i2c_reg.st.EEPROM_DATA.byte[3]);
	msleep(50);

	return 1;
}

double CBbMicom::getTemp(u16 adc)
{
//                               to cpu adc
//vss _____/\/\/\/\_____T_____/\/\/\/\______vdd
//     		rThermistor				BALANCE_RESISTOR          

/* In order to use the Beta equation, we must know our other resistor
  within our resistor divider. If you are using something with large tolerance,
  like at 5% or even 1%, measure it and place your result here in ohms. */
#define BALANCE_RESISTOR    12000.0

// This helps calculate the thermistor's resistance (check article for details).
#define MAX_ADC            1023.0

/* This is thermistor dependent and it should be in the datasheet, or refer to the
  article for how to calculate it using the Beta equation.
  I had to do this, but I would try to get a thermistor with a known
  beta if you want to avoid empirical calculations. */
#define BETA               3434.0	//NCP15XH103F03RC : B-Constant(25/85¡É)(Reference Value)

/* This is also needed for the conversion equation as "typical" room temperature
  is needed as an input. */
#define ROOM_TEMP           298.15   // room temperature in Kelvin (25 celsius)

/* Thermistors will have a typical resistance at room temperature so write this 
  down here. Again, needed for conversion equations. */
#define RESISTOR_ROOM_TEMP  10000.0

 // variables that live in this function
 double rThermistor = 0;            // Holds thermistor resistance value
 double tKelvin     = 0;            // Holds calculated temperature
 
 /* Here we calculate the thermistor¡¯s resistance using the equation 
    discussed in the article. */
 	rThermistor = BALANCE_RESISTOR * ( (MAX_ADC / (MAX_ADC - adc)) - 1);

 /* Here is where the Beta equation is used, but it is different
    from what the article describes. Don't worry! It has been rearranged
    algebraically to give a "better" looking formula. I encourage you
    to try to manipulate the equation from the article yourself to get
    better at algebra. And if not, just use what is shown here and take it
    for granted or input the formula directly from the article, exactly
    as it is shown. Either way will work! */
 	tKelvin = (BETA * ROOM_TEMP) / (BETA +  (ROOM_TEMP * log(rThermistor / RESISTOR_ROOM_TEMP)));

 /* I will use the units of Celsius to indicate temperature. I did this
    just so I can see the typical room temperature, which is 25 degrees
    Celsius, when I first try the program out. I prefer Fahrenheit, but
    I leave it up to you to either change this function, or create
    another function which converts between the two units. */

  return tKelvin - 273.15;  // convert kelvin to celsius 
}

double CBbMicom::getTemp(void)
{
	if(BbMicom_get_reg_data(m_i2c_reg.st.ADC7) > 0){
		m_temp = getTemp(m_i2c_reg.st.ADC7);
		
		dbg_printf(DBG_BB_M_FUNC, "%s() : Temp %f C\r\n", __func__,  m_temp);
	}

	return m_temp;
}

double CBbMicom::getBatteryVoltage(void)
{
	for(int i = 0; i < 2; i++)
	{
		if(BbMicom_get_reg_data(m_i2c_reg.st.BATT_VOLT) > 0){
			double volt = (double)(m_i2c_reg.st.BATT_VOLT/1000.0) + (m_iCarBatVoltageCalib * 0.1);
			//dbg_printf(DBG_BB_M_FUNC, "%s() : %0.1f V\r\n", __func__, volt);

			if(volt < 100.0){
				m_volt = volt;
				break;
			}
		}
		else {
			dbg_printf(DBG_BB_M_ERR, "%s() : read error!\r\n", __func__);
			break;
		}
	}

	return m_volt;
}

int CBbMicom::SetWatchdogTme(u8 sec)
{
	m_i2c_reg.st.WATCHDOG_TM = sec;
	
	return BbMicom_set_reg_data(m_i2c_reg.st.WATCHDOG_TM);
}

int CBbMicom::SetLowVoltage(u8 volt_100mv)
{
	m_i2c_reg.st.LOW_BATT_VOLT = volt_100mv;
	
	return BbMicom_set_reg_data(m_i2c_reg.st.LOW_BATT_VOLT);
}

int CBbMicom::SetBatteryMode(u8 mode) // 1:12V, 2:24V
{
	if(mode)
		m_i2c_reg.st.BATT_MODE = mode;
	
	return BbMicom_set_reg_data(m_i2c_reg.st.BATT_MODE);
}

int CBbMicom::SetPowerMode(eBB_MICOM_POWER mode)
{
	m_i2c_reg.st.POWER_MODE = (u8)mode;
	
	return BbMicom_set_reg_data(m_i2c_reg.st.POWER_MODE);
}


//=============================================================
static std::thread bb_micom_thread;
bool bb_micom_exit = false;
int bb_micom_msg_q_id = -1;

static void bb_micom_task(void)
{
		
		CBbMicom BB_Mcu;
		
		BB_Mcu.Initialize();
		
		sleep(1);
		
		do {
			if(BB_Mcu.read(0, (char* )BB_Mcu.m_i2c_reg.byte, sizeof(BB_Mcu.m_i2c_reg)) > 0){

				dbg_printf(DBG_BB_M_FUNC, "%s() : %f V, Temp %f C\r\n", __func__, (double)BB_Mcu.m_i2c_reg.st.BATT_VOLT/1000.0, BB_Mcu.getTemp(BB_Mcu.m_i2c_reg.st.ADC7));
			}
				
			sleep(1);
		} while(!bb_micom_exit);

}


int BB_MICOM_START(void)
{
	bb_micom_exit = false;
	
   	bb_micom_thread = std::thread(bb_micom_task);
	dbg_printf(DBG_BB_M_FUNC, "%s() : %d\r\n", __func__, bb_micom_msg_q_id);
	return 0;
}

int BB_MICOM_STOP(void)
{
	bb_micom_exit = true;
	
	 if (bb_micom_thread.joinable()) {
		bb_micom_thread.join();
 	}
	 
	dbg_printf(DBG_BB_M_FUNC, "%s() : \r\n", __func__);
	 return 0;
}

int i2c_open(const char *str_dev_name)
{
	int fd = open(str_dev_name, O_RDWR); // open("/dev/i2c-0", O_RDWR);
    if(fd < 0)
    {
        printf("Open Error...!! ErrorCode %d\n", fd);
        return -1;
    }

	return fd;
}

void i2c_close(int fd)
{
	if(fd)
		close(fd);
}

int i2c_read(int fd, int slave_addr, unsigned char reg_addr, char* buf, int readByteNum)
{
	if(!fd)
		return -1;
	
    if(ioctl(fd, I2C_SLAVE_FORCE, slave_addr) < 0)
    {
        printf("Failed to access slave for i2c read!!\n");
        return -1;
    }

    if(write(fd, (void*)&reg_addr, 1) != 1)
    {
        printf("Failed to set reg_addr for read from I2C!!\n");
        return -1;
    }


    if(read(fd, (void*)buf, readByteNum) != readByteNum)
    {
        printf("Failed to read from i2c bus!!\n");
        return -1;
    }

    return 1;
}


int i2c_write(int fd, int slave_addr, unsigned char reg_addr, char* buf, int writeByteNum)
{
    char write_buf[11];
	if(!fd)
		return -1;
	
    if(ioctl(fd, I2C_SLAVE_FORCE, slave_addr) < 0)
    {
        printf("Failed to access slave for i2c write!!\n");
        return -1;
    }

    write_buf[0] = reg_addr;
    memcpy(&(write_buf[1]), buf, writeByteNum);

    if(write(fd, write_buf, writeByteNum + 1) != writeByteNum + 1)
    {
        printf("Failed to set reg_addr for read from I2C!!\n");
        return -1;
    }    

    //printf("fd = %x, slave_addr = 0x%x, reg_addr = 0x%x, buf = 0x%x, length = %d\n", fd, slave_addr, write_buf[0], write_buf[1], writeByteNum);

    return 1;
}



