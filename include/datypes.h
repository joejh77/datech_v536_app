#ifndef _datech_types_h
#define _datech_types_h
#include <memory>
#include <iostream>
#include <string>
#include <cstdio>

#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <cstring>
#ifndef TARGET_CPU_V536
#include <execinfo.h>
#endif

#define DEBUGMSG(cond, exp) ((void)((cond)?(printf exp), TRUE : FALSE))
#define dprintf(cond, ...)   		do { if(cond) fprintf(stdout, __VA_ARGS__); }while(0)


#undef LOG_TAG
#define LOG_TAG "APP"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef 	bool	 BOOL;
typedef	int32_t	key_t;

#define FALSE	0
#define TRUE		(!FALSE)

#endif //_datech_types_h
