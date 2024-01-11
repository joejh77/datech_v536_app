#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include "wavplay.h"

#define  SCERR_NO_PROC_DEVICE                      -1    // /proc/device 가 존재하지 않는다.
#define  SCERR_DRIVER_NO_FIND                      -2    // /proc/device 에서 드라이버를 찾지 못함
#define  SCERR_NO_SOUNDCARD                        -3    // 사운드 카드가 없음
#define  SCERR_NO_MIXER                            -4    // 믹서가 없음
#define  SCERR_OPEN_SOUNDCARD                      -5    // 사운드 카드를 여는데 실패 했다.
#define  SCERR_OPEN_MIXER                          -6    // 사운드 카드를 여는데 실패 했다.
#define  SCERR_PRIVILEGE_SOUNDCARD                 -7    // 사운드 카드를 사용할 권한이 없음
#define  SCERR_PRIVILEGE_MIXER                     -8    // 미서를 사용할 권한이 없음

#define  SCERR_NO_FILE                             -10   // 파일이 없음
#define  SCERR_NOT_OPEN                            -11   // 파일이 열 수 없
#define  SCERR_NOT_WAV_FILE                        -12   // WAV 파일이 아님
#define  SCERR_NO_wav_info                       -13   // WAV 포맷 정보가 없음

#define  BUFF_SIZE            1024
#define  DSP_DEVICE_NAME      "/dev/dsp" 
#define  MIXER_DEVICE_NAME    "/dev/audio"
#define  TRUE                 1

typedef  struct
{
   unsigned char     chunk_id[4];
   unsigned long     size;
   unsigned short    tag;
   unsigned short    channels;
   unsigned long     rate;
   unsigned long     avr_samples;
   unsigned short    align;
   unsigned short    data_bit;
} wav_info_t;

static int            fd_soundcard     = -1;
static int            fd_mixer         = -1;       // volume 제어용 파일 디스크립터
static unsigned long  data_offset      =  0;       // wav 플레이용 파일 오프셋

//------------------------------------------------------------------------------
// 설명 : 메시지를 출력하고 프로그램 종료
// 인수 : format     : 출력할 문자열 형식 예) "%s%d"
//        ...        : 형식에 필요한 인수
//------------------------------------------------------------------------------
void  printx( const char *_format, ...)
{
   const char     *bar  = "---------------------------------------------------------";
   va_list   parg;
   int       count;

   printf( "%s:ERRORn", bar);

   va_start( parg,_format);
   count = vprintf(_format, parg);
   va_end( parg);

   printf( "n%sn", bar);
   exit( -1);
}

//------------------------------------------------------------------------------
// 설명 : 사운드 출력을 위한 버퍼 크기를 구한다.
// 반환 : 사운드 출력 버퍼 크기
//------------------------------------------------------------------------------
int   soundcard_get_buff_size( void)
{
   int   size  = BUFF_SIZE;

   if ( 0 <= fd_soundcard)
   {
      ioctl( fd_soundcard, SNDCTL_DSP_GETBLKSIZE, &size);
      if ( size < BUFF_SIZE ){
            printx("file size error! (size=%d)\r\n",size);
         size  = BUFF_SIZE;
      }
   }
   return size;
}

//------------------------------------------------------------------------------
// 설명 : 사운드 카드 출력을 스테레오로 설정
// 인수 : _enable - 1 : 스테레오
//                  0 : 모노
//------------------------------------------------------------------------------
void  soundcard_set_stereo( int _enable)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_STEREO, &_enable);
}
//------------------------------------------------------------------------------
// 설명 : 사운드 카드 출력 비트 레이트를 지정
// 인수 : _rate   - 설정할 비트 레이트
//------------------------------------------------------------------------------
void  soundcard_set_bit_rate( int _rate)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_SPEED, &_rate );
}

//------------------------------------------------------------------------------
// 설명 : 출력 사운드의 비트 크기 지정
// 인수 : _size   - 사운드 데이터 비트 크기
//------------------------------------------------------------------------------
void  soundcard_set_data_bit_size( int _size)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_SAMPLESIZE, &_size);
}

//------------------------------------------------------------------------------
// 설명 : sound card 볼륨 지정
// 인수 : _channel - 변경할 채널 번호
//             헤드폰 볼륨 : SOUND_MIXER_WRITE_VOLUME
//             PCM    볼륨 : SOUND_MIXER_WRITE_PCM
//             스피커 볼륨 : SOUND_MIXER_WRITE_SPEAKER
//             라인   볼륨 : SOUND_MIXER_WRITE_LINE
//             마이크 볼륨 : SOUND_MIXER_WRITE_MIC
//             CD     볼륨 : SOUND_MIXER_WRITE_CD
//             PCM2   볼륨 : SOUND_MIXER_WRITE_ALTPCM    주) ESP-MMI의 스피커 음량
//             IGain  볼륨 : SOUND_MIXER_WRITE_IGAIN
//             라인 1 볼륨 : SOUND_MIXER_WRITE_LINE1
//       _left : 볼륨의 좌측 값 또는 하위 값
//       _right: 볼륨의 우측 값 또는 상위 값
//------------------------------------------------------------------------------
void soundcard_set_volume( int _channel, int _left, int _right )
{
   int volume;

   if ( 120 < _left  ) _left  = 120;
   if ( 120 < _right ) _right = 120;

   volume = 256 *_right +_left;
   ioctl( fd_mixer, _channel, &volume );
}

//--------------------------------------------------------------------
// 설명: wav 파일을 재생하기 위해 열기
// 상세: wav 파일만 사용이 가능
// 인수: _filename   : wav 파일 이름
// 반환: 0 < 파일 열기에 성공
//       0 > 파일 열기 실패 및 에러 코드
//--------------------------------------------------------------------
int   soundcard_open_file(const char *_filename)
{
   int            fd_wavfile;
   int            read_size;
   unsigned char  buff[BUFF_SIZE+5];

   wav_info_t    *p_wav_info;
   wav_info_t     wav_info;
   unsigned char *ptr;

   if ( 0 != access(_filename, R_OK ) )                              // 파일이 없음
      return SCERR_NO_FILE;

   fd_wavfile = open(_filename, O_RDONLY );
   if ( 0 > fd_wavfile)                                              // 파일 열기 실패
      return SCERR_NOT_OPEN;

                                 // 헤더 부분 처리

   memset( buff, 0 , BUFF_SIZE);
   read_size = read( fd_wavfile, buff, BUFF_SIZE);

   if ( 0 == memmem( buff, BUFF_SIZE, "RIFF", 4 ))                // "RIFF" 문자열이 있는가를 검사한다.
      return SCERR_NOT_WAV_FILE;

   if ( 0 == memmem( buff, BUFF_SIZE, "WAVE", 4 ))                // "WAVE" 문자열이 있는가를 검사한다.
      return SCERR_NOT_WAV_FILE;


   p_wav_info = (wav_info_t *)memmem( buff, BUFF_SIZE, "fmt ", 4 ); // 포맷 정보를 구한다.
   if ( NULL == p_wav_info)
      return SCERR_NO_wav_info;

   memcpy( &wav_info, p_wav_info, sizeof(wav_info_t) );

                                 //   printf( "CHANNEL   = %dn", (int) wav_info.channels );
                                 //   printf( "DATA BITS = %dn", (int) wav_info.data_bit);
                                 //   printf( "RATE      = %dn", (int) wav_info.rate);

   ptr = (unsigned char *)memmem( buff, BUFF_SIZE, "data", 4 );
   ptr += 8;

   data_offset = (unsigned long)( ptr - buff );

   lseek( fd_wavfile, data_offset , SEEK_SET );                      // 파일 읽기 위치를 데이터 위치로 이동 시킨다.
   soundcard_set_stereo       ( wav_info.channels >= 2 ? 1 : 0);   // sound 스테레오 모드 활성화
   soundcard_set_data_bit_size( wav_info.data_bit             );   // sound 데이터 크기 지정
   soundcard_set_bit_rate     ( wav_info.rate                 );   // sound 비트 레이트 지정

   return fd_wavfile;
}

//--------------------------------------------------------------------
// 설명: 사운드카드 사용을 종료하기 위해 닫기
//--------------------------------------------------------------------
void soundcard_close( void)
{
   close( fd_soundcard);
   close( fd_mixer    );
}

//--------------------------------------------------------------------
// 설명: 사운드카드 사용을 위한 열기
// 인수: _mode - PLAYER    : 재생 전용
// 인수: _mode - RECODER   : 녹음 전용
// 인수: _mode - AUDIO     : 재생과 녹음 모두 실행
// 반환: 사운드카드 파일 디스크립터
//--------------------------------------------------------------------
int   soundcard_open(int _mode)
{
   const char  *dsp_devname   = DSP_DEVICE_NAME;
   const char  *mixer_devname = MIXER_DEVICE_NAME;

   // 사운드 카드 장치 열기
   if ( 0 != access( dsp_devname, W_OK ) )
      return SCERR_PRIVILEGE_SOUNDCARD;

   fd_soundcard = open( dsp_devname,_mode);
   if ( 0 > fd_soundcard)
      return SCERR_OPEN_SOUNDCARD;

   // MIXER 열기

   if ( 0 != access( mixer_devname, W_OK ) )
      return SCERR_PRIVILEGE_MIXER;

   fd_mixer = open( mixer_devname, O_RDWR|O_NDELAY );
   if ( 0 > fd_soundcard)
      return SCERR_OPEN_MIXER;

   return fd_soundcard;
}

int wavplay_soundcard_open(void)
{
   int      fd_soundcard;

   fd_soundcard   = soundcard_open( O_WRONLY);

 	switch( fd_soundcard)
   {
   case  SCERR_NO_PROC_DEVICE       :  printx( "/proc/device가 없음");
   case  SCERR_NO_SOUNDCARD         :  printx( "사운드 카드가 없음");
   case  SCERR_NO_MIXER             :  printx( "믹서가 없음");
   case  SCERR_PRIVILEGE_SOUNDCARD  :  printx( "사운드 카드를 사용할 권한이 없음");
   case  SCERR_PRIVILEGE_MIXER      :  printx( "믹서 장치를 사용할 권한이 없음");
   case  SCERR_OPEN_SOUNDCARD       :  printx( "사운드 카드를 열 수 없음");
   case  SCERR_OPEN_MIXER           :  printx( "믹서 장치를 열수 없음");
   }

 	if(fd_soundcard < 0){
		soundcard_close();
		return 0;
 	}

	return fd_soundcard;
}
//--------------------------------------------------------------------
//--------------------------------------------------------------------
int wavplay_volume_set( int _left, int _right)
{
	if(wavplay_soundcard_open()){
		soundcard_set_volume( SOUND_MIXER_WRITE_LINE   , _left, _right);
		soundcard_close();
		return 1;
	}
	return 0;
}
	
//--------------------------------------------------------------------
//--------------------------------------------------------------------
int wavplay_file_play( const char *wav_file_name)
{
   int      fd_soundcard;
   int      fd_wav_file;
   char    *sound_buff;                                                    // 사운드 출력을 위한 버퍼
   int      sound_buff_size;
   int      read_size;

   fd_soundcard   = wavplay_soundcard_open();
	 
 	if(fd_soundcard < 0){
		soundcard_close();
		return 0;
 	}
		
   //soundcard_set_volume( SOUND_MIXER_WRITE_ALTPCM, 100, 100);
   //soundcard_set_volume( SOUND_MIXER_WRITE_MIC   , 80, 80);
   //soundcard_set_volume( SOUND_MIXER_WRITE_IGAIN , 60, 60);

   sound_buff_size   = soundcard_get_buff_size();
   sound_buff        = ( char *)malloc( sound_buff_size);

   fd_wav_file   = soundcard_open_file( wav_file_name);                    // 출력할 wave 파일을 열기       
   switch( fd_wav_file)
   {
   case  SCERR_NO_FILE        :  printx( "WAV 파일이 없음");
   case  SCERR_NOT_OPEN       :  printx( "WAV 파일을 열 수 없음");
   case  SCERR_NOT_WAV_FILE   :  printx( "WAV 파일이 아님");
   case  SCERR_NO_wav_info  :  printx( "WAV 정보가 없음");
   }

   while( 1)
   {
      read_size = read( fd_wav_file, &sound_buff[0], soundcard_get_buff_size());
      if( 0 < read_size)
      {
         write( fd_soundcard, &sound_buff[0], read_size );
      }
      else
         break;
   }

    sleep(5);
    close( fd_wav_file);
    soundcard_close();
    return 0;
}
