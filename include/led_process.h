
#ifndef _led_process_h
#define _led_process_h

#ifdef __cplusplus
extern "C" {
#endif

int led_process_postCommand(int type, int onTime_ms_mode, int offTime_ms);
int led_process_start(void);
int led_process_stop(void);

#ifdef __cplusplus
}
#endif

#endif //_led_process_h
