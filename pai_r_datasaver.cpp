/* Copyright (c) 2013-2014 the Civetweb developers
 * Copyright (c) 2013 No Face Press, LLC
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

// Simple example program on how to use Embedded C++ interface.
#include <stdio.h>
#include <string.h>
#include <thread>

#include "datools.h"
#include "daSystemSetup.h"
#include "recorder_ex.h"
#include "recorder.h"
#include "mixwav.h"
#include "led_process.h"
#include "pai_r_data.h"
#include "user_data.h"
#include "evdev_input.h"

#include "pai_r_datasaver.h"

#include <unistd.h>

static bool exitNow = false;
static bool emergency_backup = false;

#ifdef DEF_PAI_R_DATA_SAVE

#define DBG_DATA_SAVER_INFO 1


#define DEFAULT_NETWORK_CHECK_INTERVAL		8000 //more than 5000

static CUserData pai_r_data;
static std::thread l_thread_datasaver;
///////////////////////////////////////////////////////

#if IPC_MSG_QUEUE_USE
static int msg_q_loc_queue_out = -1;
static int msg_q_loc_queue_in = -1;
#endif

bool __user_data_backup(eUserDataType ud_type)
{
	if(pai_r_data.BackupLocation(ud_type))
	{
#if IPC_MSG_QUEUE_USE
		//pai_r_data.Location_queue_index_backup(); //½´ÆÛÄ¸ »ç¿ë½Ã Á¦°Å

		if(emergency_backup == false) {
			pai_r_datasaver_send_msg_to_httpd(QMSG_IN, pai_r_data.Location_queue_auto_id_in_get(eUserDataType_Normal), pai_r_data.Location_queue_auto_id_in_get(eUserDataType_Event), pai_r_data.m_unique_local_autoid);

			//pai_r_data.Location_queue_init_file(ud_type);
		}
		//msleep(200);
#endif	
		return true;
	}

	return false;
}

void __user_data_list_clear(void)
{
	while(pai_r_data.m_data_list[eUserDataType_Normal].size()) 
		__user_data_backup(eUserDataType_Normal);

	while(pai_r_data.m_data_list[eUserDataType_Event].size()) 
		__user_data_backup(eUserDataType_Event);
}

int pai_r_datasaver_addLocation(PAI_R_LOCATION loc, bool is_event, int pre_recording_time)
{
	return pai_r_data.addLocation(loc, (eUserDataType)is_event, pre_recording_time);
}

int pai_r_datasaver_addLocation_new(std::string& file_path, bool is_event, int pre_recording_time)
{
		PAI_R_LOCATION loc;
		CGPSData gps = evdev_input_get_gpsdata();
		const char * file = strrchr(file_path.c_str(), '/');
  		file++;
			
		strcpy(loc.file_path, file_path.c_str());
		loc.create_time = recording_time_string_to_time(file);
		loc.create_time_ms = (get_tick_count()%1000);
		loc.move_filesize = 0; //KBYTE

		loc.latitude = gps.GetLat();
		loc.longitude = gps.GetLng();
		loc.accurate = (u16)gps.m_fPdop;
		loc.direction = (u16)gps.m_nCog/100;
		loc.altitude= (u16)gps.m_fAltitude;
		//dbg_printf(1, "%s() : lat:%.06f, Lng:%.06f, speed %.02f\r\n", __func__, loc.latitude, gps.GetLng(), gps.m_fSpeed);
	return pai_r_data.addLocation(loc, (eUserDataType)is_event, pre_recording_time);
}

int pai_r_datasaver_addSpeed(PAI_R_SPEED_CSV spd, bool is_event)
{
	return pai_r_data.addSpeed(spd, (eUserDataType)is_event);
}

int pai_r_datasaver_addThumbnail(u8 *data, size_t size, bool is_event)
{
	return pai_r_data.addThumbnail(data, size, (eUserDataType)is_event);
}


bool pai_r_datasaver_is_ready(void)
{
	return pai_r_data.m_location_queue_init;
}

u32 pai_r_datasaver_get_unique_local_autoid(void)
{
	return pai_r_data.m_unique_local_autoid;
}

void pai_r_datasaver_set_unique_local_autoid(u32 id)
{
	pai_r_data.m_unique_local_autoid = id;
}

static void pai_r_datasaver_thread(void)
{
	u32 time_out_tm = 0;
	u32 time_out_1Sec_tm = 0;
	u32 time_out_httpd_debug_tm = 0;
	int appnum;

#if IPC_MSG_QUEUE_USE
	msg_q_loc_queue_out = datool_ipc_msgget(MSGQ_PAIR_LOC_Q_OUT_IDKEY);
	msg_q_loc_queue_in = datool_ipc_msgget(MSGQ_PAIR_LOC_Q_IN_IDKEY);

	pai_r_data.Location_queue_init();
#endif

	while (!exitNow) {
		msleep(100);
#if 1
		if(is_time_out_ms(&time_out_tm, DEFAULT_NETWORK_CHECK_INTERVAL)){		
			//datool_is_appexe_stop("ps -e | grep -c \"[h]ttpd_r3\"", 0, &appnum);
			//printf("appnum=%d\n", appnum);
			//if (appnum == 0) 
			{
				pai_r_httpd_start();
			}
			//else {
				// if app has been running
			//}		
		}
#endif

#if IPC_MSG_QUEUE_USE
		ST_QMSG msg;
		while(datool_ipc_msgrcv(msg_q_loc_queue_out, (void *)&msg, sizeof(msg)) != -1){
			dprintf(DBG_DATA_SAVER_INFO, "%s() : datool_ipc_msgrcv type:%d data:%d data2:%d \n", __func__, msg.type , msg.data, msg.data2);
			
			switch (msg.type){
				case QMSG_UP_FILE_HASH :
					pai_r_data.update_file_hash(msg.data2, msg.string, (eUserDataType)msg.data);
				break;
				
				case QMSG_UP_TIME :
					pai_r_data.update_upload_time(msg.data2, msg.time, (eUserDataType)msg.data);
				break;

				case QMSG_OUT : 
					pai_r_data.Location_queue_auto_id_out_set(msg.data, eUserDataType_Normal);
					pai_r_data.Location_queue_auto_id_out_set(msg.data2, eUserDataType_Event);

					pai_r_data.Location_queue_index_backup(eQueueInOutType_Out); // ½´¼hÄ¸ »ç¿ë½Ã Á¦°Å ?
					
					//dprintf(DBG_DATA_SAVER_INFO, "%s() : queue id out normal:%u event:%u\n", __func__, msg.data , msg.data2);
					break;
					
				case QMSG_SOUND :
					if(msg.data == kMixwavWiFi_setting_mode){
					}
					
					mixwav_play((eMIXWAVECMD)msg.data);
					break;
					
				case QMSG_LED : {
					int on = 0;
					int off = LED_TIME_INFINITY;
					
					if(msg.data == WIFI_LED_CONNECTED){
						on = 1000;
						off = 1000;
					}
					else if(msg.data == WIFI_LED_SERVER_OK){
						on = LED_TIME_INFINITY;
						off = 0;
					}
					else if(msg.data == WIFI_LED_AP_MODE){
						on = 50;
						off = 950;
					}
						
					led_process_postCommand(LED_WIFI, on, off);
				}
					break;
				
				case QMSG_SPEAKER : {
					int volume = (int) msg.data;
					if(volume > 5) //0(off) ~ 5
						volume = 5;
					
					mixwav_set_volume(volume * 6);
					if(volume) {
						mixwav_play((eMIXWAVECMD)(kMixwaveWAVE1 + volume -1));
					}
				}
				break;

				case QMSG_HTTPD_START :
				pai_r_datasaver_send_msg_to_httpd(QMSG_LOC_MAX, pai_r_data.Location_queue_max_count_get(eUserDataType_Normal), pai_r_data.Location_queue_max_count_get(eUserDataType_Event), 0);	
				pai_r_datasaver_send_msg_to_httpd(QMSG_OUT, pai_r_data.Location_queue_auto_id_out_get(eUserDataType_Normal), pai_r_data.Location_queue_auto_id_out_get(eUserDataType_Event), 0);
				pai_r_datasaver_send_msg_to_httpd(QMSG_IN, pai_r_data.Location_queue_auto_id_in_get(eUserDataType_Normal), pai_r_data.Location_queue_auto_id_in_get(eUserDataType_Event), pai_r_data.m_unique_local_autoid);
				break;

				case QMSG_HTTPD_OPERID :
				pai_r_data.update_operation_id(msg.data, msg.string);
				break;

				case QMSG_HTTPD_ALIVE :
				time_out_tm = get_tick_count();
				
				if(Recorder_get_HttpdDebugEnable()){
					if(is_time_out_ms(&time_out_httpd_debug_tm, Recorder_get_HttpdDebugEnable() * 1000))
						pai_r_datasaver_send_msg_to_httpd(QMSG_EXIT, 0, 0, 0);
				}
				break;

				case QMSG_HTTPD_SNAPSHOT_START : //camera, width, height
				{
					int write_ok = 0;
					oasis::key_value_map_t parameters;
					int camera = msg.data;
					int width = msg.data2;
					int height = msg.time;
					
					parameters["camera"] = std::to_string(camera);
					parameters["width"] = std::to_string(width);
					parameters["height"] = std::to_string(height);


					parameters["quality"] = "90";
					parameters["timeout"] = "3000"; //wait timeout max in msec
	
					fprintf(stdout, "snapshot: %d (%d x %d)\n", camera, width, height);
					struct timeval timestamp;
					std::vector<char> image_data;
					
					if(oasis::takePicture(camera/*camera id*/, parameters, image_data, &timestamp) < 0) {
						//"Request Timeout"
					} else {

						// image_data.data(), image_data.size()
						fprintf(stdout, "snapshot image_data: %d bytes\n", image_data.size());

						char * path = msg.string;
						time_t t = (time_t)timestamp.tv_sec;
						struct tm tm_t;
						localtime_r(&t, &tm_t);
						
						sprintf(path, "/tmp/snapshot-%4d%02d%02d-%02d%02d%02d.jpg", tm_t.tm_year + 1900, tm_t.tm_mon + 1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
						FILE *fp = fopen(path, "wb");
						if(fp) {
							fwrite(image_data.data(), 1, image_data.size(), fp);
							fclose(fp);

							write_ok = 1;
							DLOG(DLOG_INFO, "%s saved %d bytes.\n", path, image_data.size());
						} else {
							DLOG(DLOG_ERROR, "%s creation failed: %d(%s)\n", path, errno, strerror(errno));
						}
					}

					msg.type = QMSG_HTTPD_SNAPSHOT_DONE;
					msg.data = write_ok;	
					msg.data2 = image_data.size();	
					datool_ipc_msgsnd(msg_q_loc_queue_in, (void *)&msg, sizeof(msg));
				}
				break;
			}
		}
#endif

#if 1
		if(is_time_out_ms(&time_out_1Sec_tm, 1000)){
			u32 size = pai_r_data.Location_data_list_size(eUserDataType_Event);
			if(size > 1 || (size && !Recorder_isEventRunning())){
				__user_data_backup(eUserDataType_Event);
			}

			size = pai_r_data.Location_data_list_size(eUserDataType_Normal);
			//if(size && (Recorder_isMotionMode() &&!Recorder_isMotionRunning())
			if(size > 1 || (size && !Recorder_IsRecording()))
			{
				__user_data_backup(eUserDataType_Normal);
			}			
		}
#endif

		if(emergency_backup){
			__user_data_list_clear();
			emergency_backup = false;
			break;
		}
		else {
			//pai_r_data.Location_queue_init_file();
		}
	}

	__user_data_list_clear();

	pai_r_httpd_end();
 
	pai_r_data.Location_queue_deinit();
	//dprintf(DBG_DATA_SAVER_INFO, "pai_r_datasaver_thread : Bye!\n");
}

int pai_r_datasaver_data_list_size(void)
{
	return pai_r_data.m_data_list[eUserDataType_Normal].size() + pai_r_data.m_data_list[eUserDataType_Event].size();
}
#endif

void pai_r_datasaver_emergency_backup(void)
{
	emergency_backup = true;
}

int pai_r_datasaver_send_msg(u32 type, u32 data, u32 data2, u32 data3)
{
#if IPC_MSG_QUEUE_USE && defined(DEF_PAI_R_DATA_SAVE)
	ST_QMSG msg;

	msg.type = type;
	msg.data = data;
	msg.data2 = data2;
	msg.time = data3;

	return datool_ipc_msgsnd(msg_q_loc_queue_out, (void *)&msg, sizeof(msg));
#endif
	return 0;
}

int pai_r_datasaver_send_msg_to_httpd(u32 type, u32 data, u32 data2, u32 data3)
{
#if IPC_MSG_QUEUE_USE && defined(DEF_PAI_R_DATA_SAVE)
	ST_QMSG msg;

	msg.type = type;
	msg.data = data;
	msg.data2 = data2;
	msg.time = data3;

	return datool_ipc_msgsnd(msg_q_loc_queue_in, (void *)&msg, sizeof(msg));
#endif
	return 0;
}

int pai_r_datasaver_start(void)
{
#ifdef DEF_PAI_R_DATA_SAVE
	l_thread_datasaver = std::thread(pai_r_datasaver_thread);
#endif
	return 0;
}


int pai_r_datasaver_end(void)
{
	exitNow = true;
	sleep(1);
#ifdef DEF_PAI_R_DATA_SAVE
	if (l_thread_datasaver.joinable()) {
		l_thread_datasaver.join();
 	}
#endif	
	return 0;
}

