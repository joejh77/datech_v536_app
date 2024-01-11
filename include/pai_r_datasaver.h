#ifndef pai_r_datasaver_H
#define pai_r_datasaver_H

#include "OasisAPI.h"
#include "OasisLog.h"
#include "json/json.h"

#include "OasisMedia.h"

#include "pai_r_data.h"

#ifdef __cplusplus
extern "C"
{
#endif
int pai_r_datasaver_addLocation(PAI_R_LOCATION loc, bool is_event, int pre_recording_time);
int pai_r_datasaver_addLocation_new(std::string& file_path, bool is_event, int pre_recording_time);
int pai_r_datasaver_addSpeed(PAI_R_SPEED_CSV spd, bool is_event);
int pai_r_datasaver_addThumbnail(u8 *data, size_t size, bool is_event);
bool pai_r_datasaver_is_ready(void);

u32 pai_r_datasaver_get_unique_local_autoid(void);
void pai_r_datasaver_set_unique_local_autoid(u32 id);

int pai_r_datasaver_data_list_size(void);

void pai_r_datasaver_emergency_backup(void);

int pai_r_datasaver_send_msg(u32 type, u32 data, u32 data2, u32 data3);
int pai_r_datasaver_send_msg_to_httpd(u32 type, u32 data, u32 data2, u32 data3);
int pai_r_datasaver_start(void);
int pai_r_datasaver_end(void);
#ifdef __cplusplus
}
#endif

#endif // pai_r_datasaver_H