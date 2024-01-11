
 #include <unistd.h> //include for sleep function
#include <stdarg.h>
#include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/msg.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <math.h>

#include "sysfs.h"

#include "datools.h"
//#include "zip.h"

#include <string>
#include <cstdlib>
#include "SB_System.h"

#define NMEA_PI                     (3.141592653589793)             /**< PI value */
#define NMEA_PI180                  (NMEA_PI / 180)                 /**< PI division by 180 */
#define NMEA_EARTHRADIUS_KM         (6378)                          /**< Earth's mean radius in km */
#define NMEA_EARTHRADIUS_M          (NMEA_EARTHRADIUS_KM * 1000)    /**< Earth's mean radius in m */


int g_dbglevel = DBG_ALL;
u32 tick_count_offset = 0;
time_t l_last_time = 0;

int syslog_printf(const char *user, int view, const char *fmt, ...)
{
	char logbuf[1024];
	va_list argptr;
	int len1, len2;

	len1 = sprintf(logbuf, "logger -t %s %s\"", user, (view) ? "-s " : "");

	va_start(argptr, fmt);
	len2 = vsprintf(&logbuf[len1], fmt, argptr);
	va_end(argptr);

	if (len1 > 0 && len2 > 0) {
		len1 += len2;
		logbuf[len1] = '"';
		logbuf[len1 + 1] = 0;
		system(logbuf);
		return 1;
	}

	return 0;
}

int dbg_printf(int status, const char *fmt, ...)
{
	va_list argptr;
	int ret = -1;

	if (g_dbglevel & status) {
		if ((status & DBG_ERR)) {
			putchar('\a');
		}

		va_start(argptr, fmt);
		ret = vprintf(fmt, argptr);
		va_end(argptr);
	}
	return ret;
}


//*Adapted from https://gist.github.com/ne-sachirou/882192
//*std::rand() can be replaced with other algorithms as Xorshift for better perfomance
//*Random seed must be initialized by user


std::string datool_generateUUID(){
#if 1
	char result[128];
	SB_CatWord("cat /proc/sys/kernel/random/uuid", result, sizeof(result));
	
	return std::string(result);
#else
  const std::string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string uuid = std::string(36,' ');
  int rnd = 0;
  int r = 0;
  
  uuid[8] = '-';
  uuid[13] = '-';
  uuid[18] = '-';
  uuid[23] = '-';

  uuid[14] = '4';

  for(int i=0;i<36;i++){
    if (i != 8 && i != 13 && i != 18 && i != 14 && i != 23) {
      if (rnd <= 0x02) {
			std::srand((unsigned)time(0)+i);
          rnd = 0x2000000 + (std::rand() * 0x1000000) | 0;
      }
      rnd >>= 4;
      uuid[i] = CHARS[(i == 19) ? ((rnd & 0xf) & 0x3) | 0x8 : rnd & 0xf];
    }
  }
  return uuid;
#endif	
}

void msleep(unsigned int t)
{
	usleep(t * 1000);
}

//return us
uint64_t systemTimeUs(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)(ts.tv_sec * 1000000ull + (ts.tv_nsec / 1000ull));
}

void parseUsec(int64_t usec, int &h, int &m, int &s, int &u)
{
	h = usec/1000000ll/3600ll;
	usec -= h*1000000ll*3600ll;
	m = usec/1000000/60;
	usec -= m*1000000ll*60ll;
	s = usec/1000000;
	usec -= s*1000000;
	u = (int)usec;
}

void bytesToString(size_t size, char *size_string)
{
	if(size >= 1024*1024) {
		sprintf(size_string, "%.2fMB", (double)size/1048576);
	} else if(size >= 1024) {
		sprintf(size_string, "%.2fKB", (double)size/1024);
	} else {
		sprintf(size_string, "%dB", size);
	}
}

//return ms
u32 get_tick_count(void)
{
	return (u32)(systemTimeUs() / 1000);
}

//return ms
u32 get_tick_count_sync(void)
{
	return get_tick_count() - tick_count_offset;
}

void tick_count_sync(time_t time_sec)
{

	if(l_last_time != time_sec ){
		l_last_time = time_sec;

		tick_count_offset = (get_tick_count() % 1000);
	}
}


u32 get_sec_count(void)
{
	time_t cur_time;

	time(&cur_time);

	return (cur_time);
}



int is_time_out_sec(u32 *StartSec, u32 TimeOutSec)
{
	u32 prevTm, currTm;

	currTm = get_sec_count();
	prevTm = *StartSec;

	if (TimeOutSec == 0) {
		*StartSec = currTm;
		return 1;
	}

	if(currTm > prevTm){
		if ((currTm - prevTm) >= TimeOutSec) {
			*StartSec = currTm;
			return 1;
		}
	}
	else {
		if ((currTm +  ~prevTm + 1) >= TimeOutSec) {
			*StartSec = currTm;
			return 1;
		}
	}

	return 0;
}

int is_time_out_ms(u32 *StartTm, u32 TimeOutMs)
{
	u32 prevTm, currTm;

	currTm = get_tick_count();
	prevTm = *StartTm;

	if (TimeOutMs == 0) {
		*StartTm = currTm;
		return 1;
	}

	if(currTm > prevTm){
		if ((currTm - prevTm) >= TimeOutMs) {
			*StartTm = currTm;
			return 1;
		}
	}else {
		if ((currTm + ~prevTm +1) >= TimeOutMs) {
			*StartTm = currTm;
			return 1;
		}
	}
	return 0;
}
//
std::string make_time_string(time_t t)
{
	struct tm tm_t;
	localtime_r(&t, &tm_t);
	return format_string("%4d-%02d-%02d %02d:%02d:%02d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
}

time_t pai_r_time_string_to_time(std::string time)
{
	time_t t = 0;
	struct tm tm_t;
	int nYear, nMonth, nDay, nHour, nMinute, nSecond;

	if(sscanf(time.c_str(), "%4d-%02d-%02d %02d:%02d:%02d", &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond)==6)
	{
		tm_t.tm_year = 70+((nYear+30)%100);	// 2019 - 1900
		tm_t.tm_mon = nMonth-1;		// Month, 0 - jan
		tm_t.tm_mday = nDay;				// Day of the month
		tm_t.tm_hour = nHour;
		tm_t.tm_min = nMinute;
		tm_t.tm_sec = nSecond;

		tm_t.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

		t = mktime(&tm_t);
	}
	
	return t;
}

std::string make_recording_time_string(time_t t)
{
	struct tm tm_t;
	localtime_r(&t, &tm_t);
	return format_string("%4d%02d%02d_%02d%02d%02d", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
}

time_t recording_time_string_to_time(std::string time)
{
	time_t t = 0;
	struct tm tm_t;
	int nYear, nMonth, nDay, nHour, nMinute, nSecond;

	if(sscanf(time.c_str(), "%4d%02d%02d_%02d%02d%02d", &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond)==6)
	{
		tm_t.tm_year = 70+((nYear+30)%100);	// 2019 - 1900
		tm_t.tm_mon = nMonth-1;		// Month, 0 - jan
		tm_t.tm_mday = nDay;				// Day of the month
		tm_t.tm_hour = nHour;
		tm_t.tm_min = nMinute;
		tm_t.tm_sec = nSecond;

		tm_t.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

		t = mktime(&tm_t);
	}
	
	return t;
}

std::string format_string(const std::string fmt, ...) 
{ 
	int size = ((int)fmt.size()) * 2;
	std::string buffer;va_list ap;
	while (1) {
		buffer.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char*)buffer.data(), size, fmt.c_str(), ap);
		va_end(ap);
		
		if (n > -1 && n < size) 
		{
			buffer.resize(n); 
			return buffer;
		} 

		if (n > -1)
			size = n + 1; 
		else
			size *= 2;
	} 
	return buffer; 
} 

/* Stringify binary data. Output buffer must be twice as big as input,
 * because each byte takes 2 bytes in string representation */
void
bin2str(char *to, const unsigned char *p, size_t len)
{
	static const char *hex = "0123456789abcdef";

	for (; len--; p++) {
		*to++ = hex[p[0] >> 4];
		*to++ = hex[p[0] & 0x0f];
	}
	*to = '\0';
}

//Km/h
double pulse_count_to_speed(u32 count, double parameter)
{
	return  count * 3600 / (637 * parameter / 10.0);
}

double pulse_speed_to_parameter(u32 count, double speed)
{
	return count * 3600 / (speed * 637 / 10.0);
}

//meter
double datool_nmea_distance(
        const nmeaPOS *from_pos,    /**< From position in degrees */
        const nmeaPOS *to_pos       /**< To position in degrees */
        )
{
    double dist;
		//degrees to radius
		double lat1 = from_pos->lat * NMEA_PI180;
		double lon1 = from_pos->lon * NMEA_PI180;
		double lat2 = to_pos->lat * NMEA_PI180;
		double lon2 = to_pos->lon * NMEA_PI180;
	
		
		dist = ((double)NMEA_EARTHRADIUS_M) * acos(
        sin(lat2) * sin(lat1) +
        cos(lat2) * cos(lat1) * cos(lon2 - lon1)
        );
    return dist;
}

double datool_nmea_distance_with_altitude(
        const nmeaPOS *from_pos,    /**< From position in degrees */
        const nmeaPOS *to_pos       /**< To position in degrees */
        )
{
	if(from_pos->alt <= 0.0 || to_pos->alt <= 0.0)
		return datool_nmea_distance(from_pos, to_pos);
	else {
	    double dist;
		//degrees to radius
		double lat_1 = from_pos->lat * NMEA_PI180;
		double lon_1 = from_pos->lon * NMEA_PI180;
		double r_1= from_pos->alt +  NMEA_EARTHRADIUS_M;
		
		double lat_2 = to_pos->lat * NMEA_PI180;
		double lon_2 = to_pos->lon * NMEA_PI180;
		double r_2= to_pos->alt + NMEA_EARTHRADIUS_M;
	
		double x_1 = r_1 * sin(lon_1) * cos(lat_1);
       double y_1 = r_1 * sin(lon_1) * sin(lat_1);
       double z_1 = r_1 * cos(lon_1);

       double x_2 = r_2 * sin(lon_2) * cos(lat_2);
       double y_2 = r_2 * sin(lon_2) * sin(lat_2);
       double z_2 = r_2 * cos(lon_2);

       dist = sqrt((x_2 - x_1) * (x_2 - x_1) + (y_2 - y_1) *    
                             (y_2 - y_1) + (z_2 - z_1) * (z_2 - z_1));

		 dbg_printf(DBG_ERR, "%s(): %0.1fm\r\n", __func__, dist);
	    return dist;
	}
}

//led
int datool_led_blue_set(bool set)
{
	return sysfs_putchar("/sys/class/leds/blue/brightness", set ? '1' : '0');
}

int datool_led_red_set(bool set)
{
	return sysfs_putchar("/sys/class/leds/red/brightness", set ? '1' : '0');
}

int datool_led_green_set(bool set)
{
	return sysfs_putchar("/sys/class/leds/green/brightness", set ? '1' : '0');
}

//ext trg_o
int datool_ext_trigger_out_set(bool set)
{
#ifdef TARGET_CPU_V536
	set != set;
#endif
	return sysfs_putchar("/sys/class/leds/trg_o/brightness", set ? '0' : '1');
}

//speed_limit / ext trg_o2
int datool_ext_speed_limit_out_set(bool set)
{
#ifdef TARGET_CPU_V536
		set != set;
#endif
	return sysfs_putchar("/sys/class/leds/speed_limit/brightness", set ? '0' : '1');
}

int datool_spk_shdn_set(bool set)
{
#ifdef TARGET_CPU_V536
	return sysfs_putchar("/sys/class/leds/spk_shdn/brightness", set ? '0' : '1');
#else
	return 0;
#endif
}


//IP 정보 가져오기
int datool_getIPAddress(char *ip_addr)
{
	int sock;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0) 
	{
		dbg_printf(DBG_ERR, "socket");
		return 0;
	}

	if(access("/sys/class/net/usb0", F_OK) == 0)
		strcpy(ifr.ifr_name, "usb0");
	else if(access("/sys/class/net/eth0", F_OK) == 0)
		strcpy(ifr.ifr_name, "eth0");
	else
		strcpy(ifr.ifr_name, "wlan0");
	
	if (ioctl(sock, SIOCGIFADDR, &ifr)< 0)    
	{
		dbg_printf(DBG_ERR, "ioctl() - get ip");
		close(sock);
		return 0;
	}

	sin = (struct sockaddr_in*)&ifr.ifr_addr;
	strcpy(ip_addr, inet_ntoa(sin->sin_addr));

	close(sock);
	return 1;

}
////////////


int datool_ipc_msgget(key_t key)
{
	int msqid = msgget((key_t) key, IPC_CREAT | 0666);
	if (msqid == -1) {
		dbg_printf(DBG_ERR, "cannot open message queue\n");
		return (-1);
	}

	dbg_printf(DBG_MSG, " id (%d) \n", msqid);
	return msqid;
}

int datool_ipc_msgsnd(int msgq_id, void * data, size_t size)
{
	if(msgq_id == -1)
		return (-1);
	
	if (msgsnd(msgq_id, data, size - sizeof(long), IPC_NOWAIT) == -1)
	{
		dbg_printf(DBG_ERR, "Fail to send message\r\n");
		return (-1);
	}

	return msgq_id;
}

int datool_ipc_msgsnd2(int msgq_id, long type, u32 data)
{
	if(msgq_id != -1){
		ST_QMSG msg;
		//while(msgrcv(msgq_id, (void *)&msg, sizeof(msg) - sizeof(long), type, IPC_NOWAIT) != -1){} //clear

		msg.type = type;
		msg.data = data ;
		msg.data2 = 0 ;
		
		datool_ipc_msgsnd(msgq_id, (void *)&msg, sizeof(msg));
		return 1;
	}

	return 0;
}

int datool_ipc_msgsnd3(int msgq_id, long type, u32 data, u32 data2)
{
	if(msgq_id != -1){
		ST_QMSG msg;
		//while(msgrcv(msgq_id, (void *)&msg, sizeof(msg) - sizeof(long), type, IPC_NOWAIT) != -1){} //clear

		msg.type = type;
		msg.data = data ;
		msg.data2 = data2 ;
		
		datool_ipc_msgsnd(msgq_id, (void *)&msg, sizeof(msg));
		return 1;
	}

	return 0;
}


int datool_ipc_msgrcv(int msgq_id, void * data, size_t size)
{
	if(msgq_id == -1)
		return (-1);
	
	if (msgrcv(msgq_id, data, size - sizeof(long), 0, IPC_NOWAIT) == -1)
	{
		//error
		return (-1);
	}

	//dprintf(DLOG_ERROR, "rcv %d %d %d\r\n", pLed_msg->type, pLed_msg->onTimeMs, pLed_msg->offTimeMs);

	
	return msgq_id;
}

int datool_led_ipc_msgsnd_ex(int msgq_id, int led_type, int onTimeMs, int offTimeMs)
{
	LED_MSG msg;
	msg.type = led_type;
	msg.onTimeMs = onTimeMs;
	msg.offTimeMs = offTimeMs;

	return datool_ipc_msgsnd(msgq_id, &msg, sizeof(msg));
}

int datool_is_appexe_stop(const char *cmd, int flg, int *num)
{
	FILE *fp_ps;
	char tmp;
	char num_str[10];
	int bpnum = 0;

	fp_ps = popen(cmd, "r");

	u32 i = 0;
	do {
		tmp = fgetc(fp_ps);
		if (feof(fp_ps) || tmp == ' ') {
			break;
		}
		num_str[i++] = tmp;
		
		if(i >= sizeof(num_str) - 1)
			break;
	} while(1);
	pclose(fp_ps);
	num_str[i] = '\0';

	if (strlen(num_str)) {
		bpnum = strtoul(num_str, NULL, 10);
	}

	*num = bpnum;

	if (bpnum > flg)
		return 0;

	return 1;
}


////////////////////////////////////////////////////////////////////////////
//return 1 exit program

int datool_test(void)
{

#if 0
		
		SB_CatWord("cat /proc/sys/kernel/random/uuid", msg, sizeof(msg));
		printf(msg);
		return 1;
#endif
#if 0 //testcode 

		int length = 4000;
		unsigned long zip_buf_len =length+1024;
		char * zip_buf  = new char[ zip_buf_len ];
		unsigned long zip_length = 0;
		void *zbuf_get;
		char md5_hash[33];
		//압축
		// afford to be generous: no address-space is allocated unless it's actually needed.
  		HZIP hz;
		hz = CreateZip(zip_buf, zip_buf_len, 0);
		if (hz==NULL) 
			dprintf(DLOG_ERROR,"%x Failed to create zip!\r\n", hz);
		
		//ZRESULT zret = ZipAdd(hz, startOfName, buf, length);
		ZRESULT zret = ZipAdd(hz, "speed1.zip", "/mnt/extsd/speed1.zip");
		
		if (zret !=ZR_OK) 
			dprintf(DLOG_ERROR,"%x Failed to add file(%s)\r\n", hz, "speed1.zip");

		zret = ZipGetMemory(hz, (void **)&zbuf_get, &zip_length);

		dprintf(DLOG_INFO,"%x : %s(%d), %s(%d)\r\n", zret, "/mnt/extsd/speed1.zip", length, "speed1.zip", zip_length);
		
		if(zret == ZR_OK) {
			mg_md5_data(md5_hash, zbuf_get, zip_length);
		}

		dprintf(DLOG_WARN,"\n%s\r\n", md5_hash);

		CloseZip(hz);
		delete [] zip_buf;

		return 1;
#endif
#if 0
		char * zip_buf;
		int length;
		
		FILE* file=  fopen( "/mnt/extsd/speed1.zip", "r");

		if(!file){
			dprintf(DLOG_ERROR, " [%s] file open error!\n","/mnt/extsd/speed1.zip");
			return 0;
		}
		
		fseek( file, 0, SEEK_END );
		length = ftell( file );
		fseek( file, 0, SEEK_SET );

	// Strange case, but good to handle up front.
		if ( length <= 0 )
		{
			fclose(file);
			dprintf(DLOG_ERROR, " [%s] file size error!\n", "/mnt/extsd/speed1.zip");
			return 0;
		}

		char* buf = new char[ length+1 ];
		buf[0] = 0;

		if ( fread( buf, length, 1, file ) != 1 ) {
			delete [] buf;
			fclose(file);
			dprintf(DLOG_ERROR, " [%s] file read error!\n", "/mnt/extsd/speed1.zip");
			return 0;
		}

		char md5_hash[33];
		mg_md5_data(md5_hash, buf, length);
		

		dprintf(DLOG_WARN,"\n%s %d\r\n", md5_hash, length);
		
		delete[] buf;
		fclose(file);
		return 1;
#endif
	return 0;
}


