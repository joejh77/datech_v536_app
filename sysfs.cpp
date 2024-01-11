
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>

#include "sysfs.h"


#define MAX_BUFFER  1024

int
sysfs_writeAppend(const char *path, const char *data, size_t size)
{
	int fd;
	int ret;

	fd = open(path, O_WRONLY | O_CREAT);
	if (fd < 0) {
		return fd;
	}

	lseek( fd, 0, SEEK_END );
	//length = ltell( fd );
	//lseek( fd, 0, SEEK_SET );
	

	ret = write(fd, data, size);

	close(fd);

	return ret;
}

int 
sysfs_write(const char *path, const char *data, size_t size)
{
	int fd;
	int ret;

	fd = open(path, O_WRONLY | O_CREAT);
	if (fd < 0) {
		return fd;
	}

	ret = write(fd, data, size);

	close(fd);

	return ret;
}

int
sysfs_replace(const char *path, const char *data, size_t size, int offset)
{
	int ret = 0;
#if 0	
	int fd = open(path, O_RDWR );
	
	if(fd){
		if(offset)
			lseek(fd, offset , SEEK_SET);
	
		ret = write(fd, data, size);				

		close(fd);
		
		if (ret != (int)size) {
			//printf("write failed: %d(%s) , at(%d) ret = %d : %d\n", errno, strerror(errno), write_pos, ret, size);
			printf("write failed: ret = %d : %d\n", ret, size);
			return 0;
		}
	}
	else
		printf("failed to open %s\n", path);
#else
	FILE *fp = fopen(path, "rb+");

	if(fp){
		fseek(fp, offset , SEEK_SET);
		
		ret = fwrite(data, 1, size, fp);				

		if (ret != (int)size) {
			printf( "write failed: %d(%s) , at(%d) ret = %d : %d\n", errno, strerror(errno), offset, ret, size);
		}
		fclose(fp);
		
	} else {
		printf( "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
	}
	
#endif
	return ret;
}


int
sysfs_putchar(const char *path, int ch)
{
	char buf = (char)ch;
	return sysfs_write(path, &buf, 1) > 0 ? ch : EOF;
}

int
sysfs_printf(const char *path, const char *fmt, ...)
{
	char buffer[MAX_BUFFER];
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (ret > 0) {
		if ((size_t)ret < sizeof(buffer)) {
			ret = sysfs_write(path, buffer, ret);
		} else {
			ret = -1;
		}
	}
	return ret;
}

int
sysfs_read(const char *path, char *buf, size_t len)
{
	return sysfs_read_offset(path, buf, len, 0);
}

int
sysfs_read_offset(const char *path, char *buf, size_t len, int offset)
{
	int fd;
	int ret;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	if(offset)
		lseek(fd, offset , SEEK_SET);
	
	ret = read(fd, buf, len);

	close(fd);

	return ret;
}

int
sysfs_getchar(const char *path)
{
	char buf[1];
	int ret;

	ret = sysfs_read(path, buf, 1);
	if (ret < 0) {
		return ret;
	}

	return buf[0];
}

int
sysfs_scanf(const char *path, const char *fmt, ...)
{
	va_list ap;
	char buf[MAX_BUFFER];
	int ret;

	ret = sysfs_read(path, buf, sizeof(buf));
	if (ret > 0) {
		va_start(ap, fmt);
		ret = vsscanf(buf, fmt, ap);
		va_end(ap);
	}

	return ret;
}

int
sysfs_getsize(const char *path)
{
	FILE* file=  fopen( path, "r");
	int length = 0;
	
	if(!file){
		printf(" [%s] file open error!\n", path);
		return 0;
	}
	
	fseek( file, 0, SEEK_END );
	length = ftell( file );
	fseek( file, 0, SEEK_SET );

	fclose(file);
	
// Strange case, but good to handle up front.
	if ( length <= 0 )
	{
		printf(" [%s] file size error!\n", path);
		return 0;
	}

	return length;
}
