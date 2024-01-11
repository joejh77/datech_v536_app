#include <stdio.h>
#include <sysfs.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "dagpio.h"

#define BUF_MAX 4
#define DIRECTION_MAX 40
#define VALUE_MAX 40

#define DIR_IN  0
#define DIR_OUT 1
 
#define LOW  0
#define HIGH 1

static int GPIOExport(int pin)
{
    char buf[BUF_MAX];
    size_t bytes_written;
    int fd;
    int ret;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0)
    {
        printf("Failed to open export for writing!\n");
        return fd;
    }

    bytes_written = sprintf(buf, "%d", pin);
    ret = write(fd, buf, bytes_written);

    close(fd);

    return ret;
}

static int GPIOUnexport(int pin)
{
    char buf[BUF_MAX];
    size_t bytes_written;
    int fd;
    int ret;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd < 0)
    {
        printf("Failed to open unexport for writing!\n");
        return fd;
    }

    bytes_written = sprintf(buf, "%d", pin);
    ret = write(fd, buf, bytes_written);

    close(fd);

    return ret;
}

static int GPIODirection(int pin, char* dir)
{
    char path[DIRECTION_MAX] = {0,};
    int fd;
    int ret;

    sprintf(path, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if(fd < 0)
    {
        printf("Failed to open gpio direction for writing!\n");
        return fd;
    }

    ret = write(fd, dir, strlen(dir));
    if(ret < 0)
    {
        printf("Failed to set direction!\n");
    }

    close(fd);
    return ret;
}


static int GPIORead(int pin)
{
    char path[VALUE_MAX] = {0,};
    char value_str[2];
    int fd;

    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        printf("Failed to open gpio value for reading!\n");
        return fd;
    }
    if(read(fd, value_str, 1) < 0)
    {
        printf("Failed to read value!\n");
    }
    printf("pin number %d read value = %s\n", pin, value_str);
    close(fd);

    return(atoi(value_str));
}

static int GPIOWrite(int pin, int value)
{
    static const char s_value_str[] = "01";

    char path[VALUE_MAX] = {0,};
    int fd;
    int ret;

    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);

    fd = open(path, O_WRONLY);

    if(fd < 0)
    {
        printf("Faild to open gpio value for writing\n");
        return fd;
    }

    ret = write(fd, &s_value_str[LOW == value ? 0 : 1], 1);
    if (ret != 1)
    {
        printf("Failed to write value!\n");
    }
    close(fd);

    return ret;
}

static int GPIOActiveLow(int pin, int value)
{
    static const char s_value_str[] = "01";

    char path[VALUE_MAX] = {0,};
    int fd;
    int ret;

    sprintf(path, "/sys/class/gpio/gpio%d/active_low", pin);

    fd = open(path, O_WRONLY);

    if(fd < 0)
    {
        printf("Failed to open gpio active_low for writing\n");
        return fd;
    }

    ret = write(fd, &s_value_str[LOW == value ? 0 : 1], 1);
    if(ret != 1)
    {
        printf("Failed to write active_low!\n");
    }
    close(fd);

    return ret;
}

int dagpio_pin_read(int pin)
{
	int value;
	
	GPIOExport(pin);
	GPIODirection(pin, (char *)"in");
	value = GPIORead(pin);
	GPIOUnexport(pin);

	return value;
}
	
int dagpio_main(int argc, char** argv)
{
    int pin, value;

    if(argc < 3)
    {
        printf("invalid argument\nUsage: %s <command > <gpio number> <value> \nexample:\n%s export 123 \n%s direction 123 high\n%s active_log 123 1\n%s get 123 \n%s set 123 1\n", 
        argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
        return -1;
    }

    if(strcmp("export", argv[1]) == 0) // export 123
    {
        pin = atoi(argv[2]);        
        GPIOExport(pin);
    }
    else if(strcmp("unexport", argv[1]) == 0) // unexport 123
    {
        pin = atoi(argv[2]);
        GPIOUnexport(pin);
    }
    else if(strcmp("direction", argv[1]) == 0) //direction 123 high
    {
        pin = atoi(argv[2]);
        GPIODirection(pin, argv[3]);
    }
    else if(strcmp("active_loq", argv[1]) == 0) //active_low 123 1
    {
        pin = atoi(argv[2]);
        value = atoi(argv[3]);
        GPIOActiveLow(pin, value);
    }
    else if(strcmp("set", argv[1]) == 0) //set 123 0
    {
        pin = atoi(argv[2]);
        value = atoi(argv[3]);
        GPIOWrite(pin, value);
    }
    else if(strcmp("get", argv[1]) == 0)// get 123
    {
        pin = atoi(argv[2]);
        GPIORead(pin);
    }

    return 0;

}

