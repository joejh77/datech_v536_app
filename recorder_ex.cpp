#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "daappconfigs.h"
#include "datools.h"
#include "recorder.h"
#include "recorder_ex.h"

int pai_r_httpd_start(void)
{
	char cmd[128];
	
	if(access(DA_HTTPD_R3_PATH_SD, F_OK) == 0)
		strcpy(cmd, DA_HTTPD_R3_PATH_SD " 1");
	else if(access(DA_HTTPD_R3_PATH_USER, F_OK) == 0)
		strcpy(cmd, DA_HTTPD_R3_PATH_USER " 2");
	else if(access(DA_HTTPD_R3_PATH_SYSTEM, F_OK) == 0)
		strcpy(cmd, DA_HTTPD_R3_PATH_SYSTEM " 3");

	if(Recorder_get_HttpdDebugEnable())
		strcat(cmd, format_string(" >> /mnt/extsd/httpd_debug_%s.txt", make_recording_time_string(time(0)).c_str()).c_str());

	strcat(cmd, " &");
	system(cmd);
	
	return 1;
}

int pai_r_httpd_end(void)
{
	return 1;
}