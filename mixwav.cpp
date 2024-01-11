
#include "OasisAPI.h"
#include "OasisLog.h"
#include "datypes.h"
#include "datools.h"

#include <sys/wait.h> //for fork wait
#include <stdlib.h>
#include <unistd.h>

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#define SDL_AUDIO_USE 0
#if SDL_AUDIO_USE
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

#include "mixwav.h"

const char * wav_file[] = {
	"start.wav",				//kMixwaveRecordStart
	"Power_off.wav",	//kMixwaveRecordStop,
	"event.wav",			//kMixwaveEventRecording
	"GPS.wav",				//kMixwaveGPS
	"none.wav",				//kMixwaveNone
	"SD_card.wav",		//kMixwaveSDError
	"SD_init.wav",			//kMixwaveSDInit
	"Update.wav",			//kMixwaveUpdate
	"Update_end.wav",	//kMixwaveUpdateEnd
	"dingdong.wav",		//kMixwaveDingdong

	"Communication_error.wav",
	"Server_connection_failed.wav",
	"check_the_network_settings.wav",
	"Driverecorder_registration_failed.wav",
	"Update_new_version.wav",

	"WAVE1.wav",
	"WAVE2.wav",
	"WAVE3.wav",
	"WAVE4.wav",
	"WAVE5.wav",

	"Record_wa.wav",
	""
};

const char *wav_dir = "/datech/wav/";
const char *aplay_path = "/usr/bin/aplay";
//const char *aplay_path = "/mnt/extsd/sdl_wavplay";

struct MixwaveCommand {
	int32_t cmd_;
	MixwaveCommand() = default;
	MixwaveCommand(int32_t cmd) : cmd_(cmd) {}
	MixwaveCommand(const MixwaveCommand& src) : cmd_(src.cmd_) {}
	MixwaveCommand& operator=(const MixwaveCommand& src) {
		if(this != &src) {
			cmd_ = src.cmd_;
		}
		return *this;
	}
};


static std::condition_variable mixcmdq_cond_;
static std::mutex mixcmdq_lock_;
static std::list<MixwaveCommand> mixcmdq_;

static std::thread mixwave_thread;

static void mixwav_postCommand(int32_t cmd) {
	try {
		std::lock_guard<std::mutex> _l(mixcmdq_lock_);
		mixcmdq_.emplace_back(cmd);
		mixcmdq_cond_.notify_one();
	} catch(...) {

	}
}

#define WAIT_FOR_COMPLETION
static int exec_prog(const char **argv)
{
    pid_t   my_pid;
    int     status, timeout /* unused ifdef WAIT_FOR_COMPLETION */;

    if (0 == (my_pid = fork())) {
            if (-1 == execve(argv[0], (char **)argv , NULL)) {
                    DLOG0(DLOG_ERROR, "child process execve failed [%m]");
                    return -1;
            }
    }

#ifdef WAIT_FOR_COMPLETION
    timeout = 3000;

    while (0 == waitpid(my_pid , &status , WNOHANG)) {
            if ( --timeout < 0 ) {
                    DLOG0(DLOG_ERROR, "timeout");
                    return -1;
            }
            sleep(1);
    }

    DLOG0(DLOG_WARN, "%s WEXITSTATUS %d WIFEXITED %d [status %d]\n",
            argv[0], WEXITSTATUS(status), WIFEXITED(status), status);

    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
            DLOG0(DLOG_ERROR, "%s failed, halt system", argv[0]);
            return -1;
    }

#endif
    return 0;
}

#if SDL_AUDIO_USE
static void channel_complete_callback(int channel)
{
    Mix_Chunk *done_chunk = Mix_GetChunk(channel);

    printf("Sound at ch %d done\n", channel);

    if (done_chunk) {
        // 아래의 wav와 같은 메모리를 가리킨다.
        Mix_FreeChunk(done_chunk);
    }
}
#endif

static void mixwav_task(void)
{
		MixwaveCommand cmd;
		int iRecordStartDelay = 0;

#if SDL_AUDIO_USE
		int flags;
			// 시작: 앱 시작할 때 초기화 작업
	    atexit(SDL_Quit);

	    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
	        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
	        //return 2;
	    }

	    flags = Mix_Init(~0);
	    printf("mix flags %x\n", flags);

	    // 장치를 연다. 고품질이 필요하지 않을 경우 원하는 포맷을 지정한다.
	    if (Mix_OpenAudio(24000, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096) < 0) {
	        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio %s\n", SDL_GetError());
	       // return 3;
	    }

	    Mix_ChannelFinished(channel_complete_callback);
#endif
		msleep(1000);

		do {
			{
				std::unique_lock<std::mutex> _l(mixcmdq_lock_);
				while(mixcmdq_.empty()) {
					mixcmdq_cond_.wait(_l);
				}
				cmd = mixcmdq_.front();
				mixcmdq_.pop_front();
			}

			if(cmd.cmd_ == kMixwaveExitNow) {
				DLOG(DLOG_INFO, "got kRecorderExitNow\r\n");
				break;
			}
			else if(cmd.cmd_ == kMixwave10SecBlocking){
				sleep(10);
				continue;
			}

#if 1
			//RecordStart sound 2sec delay
			if(cmd.cmd_ == kMixwaveRecordStop && iRecordStartDelay){
				iRecordStartDelay = -1; // skip RecordStart sound
			}


			if(cmd.cmd_ == kMixwaveRecordStart){
				if(iRecordStartDelay == -1){ 
					iRecordStartDelay = 0; 
					continue;
				}
				if(iRecordStartDelay++ < 20){
					msleep(100);
					mixwav_postCommand(cmd.cmd_);
					continue;
				}
				iRecordStartDelay = 0;
			}
#endif

			if(cmd.cmd_ >= 0 && cmd.cmd_ < kMixwaveExitNow){
				char file_path[64];
				//-t raw -c 1 -f S16_LE -r 24000
				const char *s_argv[] ={ aplay_path, (const char *)file_path, NULL}; //"--disable-resample"
				//const char *s_argv[] ={ aplay_path, "-t", "raw", "-c", "1", "-f", "S16_LE", "-r", "22050", (const char *)file_path, NULL}; //"--disable-resample"

				if(mixwav_get_file_path(file_path, (eMIXWAVECMD)cmd.cmd_)){
#if SDL_AUDIO_USE
					Mix_Chunk *wav;
					wav = Mix_LoadWAV(file_path);
					Mix_PlayChannel(-1, wav, -1);
#else
					exec_prog(s_argv);  
					DLOG(DLOG_WARN, "aplay %s\n", file_path);
#endif		
				}
				else {
					DLOG(DLOG_ERROR, "%s: No such file or directory\r\n", file_path);
				}

				if(cmd.cmd_ == kMixwaveUpdate){
					DLOG(DLOG_INFO, "got kMixwaveUpdate\r\n");
					break;
				}
			}			
		} while(1);

#if SDL_AUDIO_USE
		Mix_CloseAudio();
#endif

		{
			std::lock_guard<std::mutex> _l(mixcmdq_lock_);
			mixcmdq_.clear();
		}
}

int mixwav_play(eMIXWAVECMD cmd)
{
	mixwav_postCommand((int32_t)cmd);
	return 0;
}

int mixwav_start(void)
{
	mixwav_postCommand(kMixwaveNone);
	
   	mixwave_thread = std::thread(mixwav_task);
	DLOG(DLOG_INFO, "%s() : \r\n", __func__);
	return 0;
}

int mixwav_stop(void)
{
	mixwav_postCommand((int32_t)kMixwaveExitNow);
	
	 if (mixwave_thread.joinable()) {
		mixwave_thread.join();
 	}
	DLOG(DLOG_INFO, "%s() : \r\n", __func__);
	 return 0;
}

/*
#amixer set Line 31
  Simple mixer control 'Line',0
  Capabilities: pvolume pvolume-joined
  Playback channels: Mono
  Limits: Playback 0 - 31
  Mono: Playback 31 [100%]
*/

int mixwav_set_volume(int volume)
{
	char szCmd[10];
	const char *szMixer = "/usr/bin/amixer";

#ifdef TARGET_CPU_V536
	const char *argv[] ={ szMixer, "set", "'lineout volume'", (const char *)szCmd, NULL};
	
//	const char *argvDigitalVolume[] ={ szMixer, "set", "'digital volume'", "0", NULL};
//	const char *argvLeftOutputMixerDACR[] = {szMixer, "set", "'Left Output Mixer DACR'", "on", NULL};
//	const char *argvRightOutputMixerDACR[] = {szMixer, "set", "'Right Output Mixer DACR'", "off", NULL};
#else
	const char *argv[] ={ szMixer, "set", "Line", (const char *)szCmd, NULL};
#endif

	datool_spk_shdn_set(0);
	
	if(volume)
		volume++;
	
	if(volume > 31)
		volume = 31;
	
	sprintf(szCmd, "%02d", volume);
	exec_prog(argv);  

#if 0 //def TARGET_CPU_V536
	exec_prog(argvDigitalVolume);  
	exec_prog(argvLeftOutputMixerDACR);  
	exec_prog(argvRightOutputMixerDACR);  
#endif
	
	return volume;
}

bool mixwav_get_file_path(char * file_path, eMIXWAVECMD cmd)
{
	sprintf(file_path, "%s%s",wav_dir, wav_file[cmd]);

	if ( access(file_path, R_OK ) != 0){
		sprintf(file_path, "/data/wav/%s", wav_file[cmd]);
		if ( access(file_path, R_OK ) != 0) {
			sprintf(file_path, "/mnt/extsd/%s", wav_file[cmd]);
		}
	}

	if(access(file_path, R_OK ) == 0)
		return true;
	

	return false;
}

void mixwav_system_aplay(eMIXWAVECMD cmd)
{
	char file_path[64];
	if(mixwav_get_file_path(file_path, cmd)){
		char cmd[128];
		sprintf(cmd, "%s %s", aplay_path,  file_path);
		system(cmd);
	}
	else {
		DLOG(DLOG_ERROR, "%s: No such file or directory\r\n", file_path);
	}
}
