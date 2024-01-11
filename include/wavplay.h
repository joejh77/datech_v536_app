#ifndef _wavplay_h
#define _wavplay_h

#ifdef __cplusplus
extern "C" {
#endif

int wavplay_volume_set(int _left, int _right);
int wavplay_file_play( const char *wav_file_name);

#ifdef __cplusplus
}
#endif

#endif //_wavplay_h