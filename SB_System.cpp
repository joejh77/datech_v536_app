 #include <unistd.h> //include for sleep function

#include <sys/types.h>
 #include <sys/ipc.h>
 #include <sys/msg.h>
 #include <sys/stat.h>

 #include <fcntl.h>

#include "sysfs.h"
#include "datools.h"
#include "SB_System.h"

#define SYSTEM_INFO 	1
#define SYSTEM_ERR		1
#define SYSTEM_WARN 	1

//==========================================================================
void SB_SetPriority (int priority)
{
struct sched_param schedp;

    memset(&schedp, 0, sizeof(schedp));
    schedp.sched_priority = priority; 			// 1 ~ 99
    sched_setscheduler(0, SCHED_FIFO, &schedp);
}
//==========================================================================
unsigned long SB_GetTick ()
{
struct timeval tval;
	gettimeofday(&tval, NULL);
	return ((tval.tv_sec*1000) + (tval.tv_usec/1000));
}
//==========================================================================
void SB_msleep (int msec) 
{
struct timespec interval;

	interval.tv_sec  = msec / 1000;
	interval.tv_nsec = 1000000 * (msec % 1000);
	nanosleep (&interval, NULL);
}
//==========================================================================
void SB_AliveTime (int *day, int *hour, int *min, int *sec)
{
time_t tt;
unsigned int old = 0;
int fd;
char buf[30];

    time(&tt);
	fd = open("/var/run/timesec", O_RDONLY);
	if (fd > 0)
		{
		lseek (fd, 0, 0);
		read(fd, buf, 20);
		close(fd);
		old = atoi (buf);
		}	

	tt -= old;

	*day = tt / 86400;
	tt %= 86400;
	*hour = tt / 3600;
	tt %= 3600;
	*min = tt / 60;
	*sec = tt % 60;
}

//=============================================================
void SB_MakeStartTime ()
{
	int fd;
	char buf[30];
	time_t tt;
//	struct tm t;
	
    time(&tt);
	fd = open("/var/run/timesec", O_WRONLY | O_CREAT);
	if (fd > 0)
		{
		lseek (fd, 0, 0);
		sprintf (buf, "%d", (int)tt);
		write(fd, buf, 20);
		close(fd);
		chmod ("/var/run/timesec", S_IWUSR|S_IRUSR);
		}		
}

//=============================================================
void SB_KillProcessFromPid(const char * pid_file)
{
	int pid;
	
	if ( access(pid_file, R_OK ) == 0){
		FILE *fp;
		fp = fopen(pid_file, "r");
		fscanf(fp, "%d", &pid);
		fclose(fp);
	
		if(pid){
			char cmd[64];
			sprintf(cmd, "kill -9 %u", pid);
			dprintf(SYSTEM_INFO, "pid_file exit. %s\r\n", cmd);
			system(cmd);

			sprintf(cmd, "rm %s", pid_file);
			dprintf(SYSTEM_INFO, "file delete. %s\r\n", cmd);
			system(cmd);
		}
	}
}

int SB_Find_pid(const char *cmd, int flg, int *num)
{
	FILE *fp_ps;
	char tmp;
	char num_str[128] = { 0,};
	int bpnum = 0;
	
	fp_ps = popen(cmd, "r");

	int i = 0;
	do {
		tmp = fgetc(fp_ps);
		if (feof(fp_ps) || tmp == ' ' || tmp == '\r' || tmp == '\n') {
			break;
		}
		num_str[i++] = tmp;
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

void SB_KillProcessName(const char * name)
{
	int pid = 0;
	char cmd[256];
	int current_pid = 0;

	sprintf(cmd, "ps aux | grep '_____' | awk '{print $1}'");
	SB_Find_pid(cmd, 0, &current_pid);
	//dprintf(SYSTEM_INFO,"current pid : %d\r\n", current_pid);
	
	sprintf(cmd, "ps aux | grep '%s' | awk '{print $1}'", name);

	if(SB_Find_pid(cmd, current_pid, &pid)){
		//dprintf(SYSTEM_INFO,"%s pid : %d\r\n", name, pid);
		if(pid){
			sprintf(cmd, "kill -9 %d", pid);
			dprintf(SYSTEM_INFO, "%s\r\n", cmd);
			sleep(1);
			system(cmd);
		}
	}
}

int SB_Get_Stations_count(void)
{
	FILE *fp_ps;
	char tmp;
	char num_str[512] = { 0,};
	int start_line_pos = 0;
	int bpnum = 0;
	
	fp_ps = popen("arp", "r");

	int i = 0;
	do {
		tmp = fgetc(fp_ps);
		if( tmp == '\r' || tmp == '\n'){

			if(strstr( &num_str[start_line_pos], "incomplete")==NULL)
				bpnum++;
			
			start_line_pos = i+1;
		}
		
		if (feof(fp_ps))
			break;

		num_str[i++] = tmp;
	} while(i < sizeof(num_str));
	pclose(fp_ps);
	num_str[i] = '\0';

	if (strlen(num_str)) {
		dprintf(SYSTEM_INFO, "station count : %d\r\n%s\r\n", bpnum, num_str);
	}

	return bpnum;
}

int SB_Cat(const char *cmd, char * result, int size)
{
	FILE *fp_ps;
	char tmp;

	fp_ps = popen(cmd, "r");

	int i = 0;
	do {
		tmp = fgetc(fp_ps);
		if (feof(fp_ps)/* || tmp == ' ' || tmp == '\r' || tmp == '\n'*/) {
			break;
		}
		if(i < size-1)
			result[i++] = tmp;
		else
			break;
	} while(1);
	pclose(fp_ps);
	
	result[i] = '\0';
	return i;
}

int SB_CatWord(const char *cmd, char * result, int size)
{
	char tmp;
	int len = SB_Cat(cmd, result, size);
	
	int i = 0;
	for(; i < len; i++){
		tmp = result[i];
		if (tmp == '\0' || tmp == ' ' || tmp == '\r' || tmp == '\n') 
			break;
	}
	result[i] = '\0';
	return i;
}

int SB_CmdScanf(const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int ret = 0;
	char tmp;
	FILE *fp_ps;
	int i = 0;
	
	fp_ps = popen(cmd, "r");
	
	do {
		tmp = fgetc(fp_ps);
		if (feof(fp_ps)) {
			break;
		}
		
		if(i >= (sizeof(buf) -1))
			break;
		
		buf[i++] = tmp;
	} while(1);
	
	pclose(fp_ps);
	buf[i] = '\0';
	
	if (i > 0) {
		va_start(ap, fmt);
		ret = vsscanf(buf, fmt, ap);
		va_end(ap);
	}

	return ret;
}


