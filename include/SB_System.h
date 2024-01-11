#ifndef SB_System_H
#define SB_System_H

#ifdef __cplusplus
extern "C"
{
#endif
//==========================================================================
void SB_SetPriority (int priority);
//==========================================================================
unsigned long SB_GetTick ();
//==========================================================================
void SB_msleep (int msec);
//==========================================================================
void SB_AliveTime (int *day, int *hour, int *min, int *sec);
void SB_MakeStartTime ();
void SB_KillProcessFromPid(const char * pid_file);
void SB_KillProcessName(const char * name);
int SB_Get_Stations_count(void);
int SB_Cat(const char *cmd, char * result, int size);
int SB_CatWord(const char *cmd, char * result, int size);
int SB_CmdScanf(const char *cmd, const char *fmt, ...);
#define sb_malloc(len) malloc(len)

#define sb_free(p) free(p)

#ifdef __cplusplus
}
#endif

#endif // SB_System_H