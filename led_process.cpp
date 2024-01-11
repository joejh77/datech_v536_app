
#include "OasisAPI.h"
#include "OasisLog.h"
#include "datypes.h"

#include <sys/wait.h> //for fork wait
#include <stdlib.h>
#include <unistd.h>

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "daversion.h"
#include "datools.h"
#include "led_process.h"

struct LedCommand {
	int type;
	int onTimeMs;
	int offTimeMs;
	
	LedCommand() = default;
	LedCommand(int t, int on_tm, int off_tm) : type(t), onTimeMs(on_tm), offTimeMs(off_tm){}
	LedCommand(const LedCommand& src) : type(src.type), onTimeMs(src.onTimeMs), offTimeMs(src.offTimeMs){}
	LedCommand& operator=(const LedCommand& src) {
		if(this != &src) {
			type = src.type;
			onTimeMs = src.onTimeMs;
			offTimeMs = src.offTimeMs;
		}
		return *this;
	}
};


static std::condition_variable ledcmdq_cond_;
static std::mutex ledcmdq_lock_;
static std::list<LedCommand> ledcmdq_;

static std::thread led_thread;

static void led_postCommand(int type, int on_tm, int off_tm) {
	try {
		std::lock_guard<std::mutex> _l(ledcmdq_lock_);
		ledcmdq_.emplace_back(type, on_tm, off_tm);
		ledcmdq_cond_.notify_one();
	} catch(...) {

	}
}

#define LED_TIME_MS			100

typedef struct {
	int on;
	int off;
}LED_TIME;

typedef struct {
	LED_TIME S;
	LED_TIME C;
}LED_WORK;

typedef struct {
	LED_WORK Mode[2];

	int (*led_set)(bool set);
}LED_CTRL;


void __led_mode_init(LED_CTRL *leds, int mode, bool intersect_mode)
{
	for ( int id = 0; id < LED_END; id++) {
		if(leds[id].Mode[mode].S.on){
			if(intersect_mode)
				leds[id].led_set((bool)(id%2));
			else
				leds[id].led_set(1);
			//DLOG(DLOG_WARN, "onTime %d\n", leds[id].Mode[mode].S.on);
		}		
		else if(leds[id].Mode[mode].S.off){
				leds[id].led_set(0);
			//DLOG(DLOG_WARN, "offTime %d\n", leds[id].Mode[mode].S.off);
		}
	}
}

int _dummy_led_set(bool set){
	return 0;
}
	
static void led_task(void)
{
		LedCommand cmd;

		int mode = LED_WORK_NORMAL;
		int mode_working_time = 0;
		bool intersect_mode  = false;
			
		LED_CTRL leds[LED_END] = {0,};
		

		leds[LED_RED].led_set = datool_led_red_set;
		leds[LED_BLUE].led_set = datool_led_blue_set;
		
#if DEF_BOARD_WITHOUT_DCDC
		//Todo WiFi LED 
		leds[LED_WIFI].led_set = datool_led_green_set;
#else
		//Todo WiFi LED 
		leds[LED_WIFI].led_set = _dummy_led_set;
#endif

		do {
			{
				std::unique_lock<std::mutex> _l(ledcmdq_lock_);
				while(ledcmdq_.empty()) {
					ledcmdq_cond_.wait(_l);
				}
				cmd = ledcmdq_.front();
				ledcmdq_.pop_front();
			}

			if(cmd.type == LED_END) {
				DLOG(DLOG_INFO, "got ExitNow\r\n");
				break;
			}

			if(cmd.type == LED_TIME_100MS){
				if(mode != LED_WORK_NORMAL && mode_working_time != LED_TIME_INFINITY){
					if( mode_working_time > LED_TIME_MS) {
						mode_working_time -= LED_TIME_MS;
					}
					else{
						mode = LED_WORK_NORMAL;
						intersect_mode = false;
						mode_working_time = 0;
						__led_mode_init(leds, mode, intersect_mode);
					}
				}

				if(mode > LED_WORK_EVENT)
					mode = LED_WORK_NORMAL;
				
				
				for ( int id = 0; id < LED_END; id++) {
					if(leds[id].Mode[mode].S.on && leds[id].Mode[mode].S.off){
						if(leds[id].Mode[mode].C.on > LED_TIME_MS) {
							leds[id].Mode[mode].C.on -= LED_TIME_MS;
						}
						else if(leds[id].Mode[mode].C.on){
							leds[id].Mode[mode].C.on = 0;

							if(intersect_mode)
								leds[id].led_set(!(bool)(id%2));
							else
								leds[id].led_set(0);
						}
						else if(leds[id].Mode[mode].C.off > LED_TIME_MS) {
							leds[id].Mode[mode].C.off -= LED_TIME_MS;
						}
						else{
							leds[id].Mode[mode].C = leds[id].Mode[mode].S;

							if(intersect_mode)
								leds[id].led_set((bool)(id%2));
							else
								leds[id].led_set(1);
						}
					}
				}
				continue;
			}

			if(cmd.type == LED_WORK_MODE){
				intersect_mode = false;
				mode = cmd.onTimeMs; // mode
				mode_working_time = cmd.offTimeMs;
				
				switch(mode) {
				
				case LED_WORK_EVENT:
				case LED_WORK_FORMAT:
				case LED_WORK_BOOTING:
				case LED_WORK_SD_ERROR:
					intersect_mode = true;
					cmd.onTimeMs = 500;
					cmd.offTimeMs = 500;
					break;
					
				case LED_WORK_POWER_OFF:
				case LED_WORK_LED_OFF:
					intersect_mode = true;
					cmd.onTimeMs = 0;
					cmd.offTimeMs = LED_TIME_INFINITY;
					break;					
				}

				if(mode >= LED_WORK_EVENT){
					mode = LED_WORK_EVENT;

					for ( int id = 0; id < LED_END; id++) {
						leds[id].Mode[mode].S.on= cmd.onTimeMs;
						leds[id].Mode[mode].S.off = cmd.offTimeMs;

						leds[id].Mode[mode].C = leds[id].Mode[mode].S;
					}
				}

				__led_mode_init(leds, mode, intersect_mode);				
			}
			else if(cmd.type < LED_END ){
				int id = cmd.type;
				
				leds[id].Mode[LED_WORK_NORMAL].S.on= cmd.onTimeMs;
				leds[id].Mode[LED_WORK_NORMAL].S.off = cmd.offTimeMs;

				leds[id].Mode[LED_WORK_NORMAL].C = leds[id].Mode[LED_WORK_NORMAL].S;
				
				if(cmd.onTimeMs){
					leds[id].led_set(1);
					//DLOG(DLOG_WARN, "onTime %d\n", cmd.onTimeMs);
				}		
				else if(cmd.offTimeMs){
						leds[id].led_set(0);
					//DLOG(DLOG_WARN, "offTime %d\n", cmd.offTimeMs);
				}
			}

		} while(1);

		{
			std::lock_guard<std::mutex> _l(ledcmdq_lock_);
			ledcmdq_.clear();
		}
}

int led_process_postCommand(int type, int onTime_ms_mode, int offTime_ms)
{
	led_postCommand(type, onTime_ms_mode, offTime_ms);
	return 0;
}

int led_process_start(void)
{
   	led_thread = std::thread(led_task);
	DLOG(DLOG_INFO, "%s() : \r\n", __func__);
	return 0;
}

int led_process_stop(void)
{
	led_postCommand(LED_END, 0 , 0);
	
	 if (led_thread.joinable()) {
		led_thread.join();
 	}
	DLOG(DLOG_INFO, "%s() : \r\n", __func__);
	 return 0;
}

