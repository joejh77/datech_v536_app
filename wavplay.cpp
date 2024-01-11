#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include "wavplay.h"

#define  SCERR_NO_PROC_DEVICE                      -1    // /proc/device �� �������� �ʴ´�.
#define  SCERR_DRIVER_NO_FIND                      -2    // /proc/device ���� ����̹��� ã�� ����
#define  SCERR_NO_SOUNDCARD                        -3    // ���� ī�尡 ����
#define  SCERR_NO_MIXER                            -4    // �ͼ��� ����
#define  SCERR_OPEN_SOUNDCARD                      -5    // ���� ī�带 ���µ� ���� �ߴ�.
#define  SCERR_OPEN_MIXER                          -6    // ���� ī�带 ���µ� ���� �ߴ�.
#define  SCERR_PRIVILEGE_SOUNDCARD                 -7    // ���� ī�带 ����� ������ ����
#define  SCERR_PRIVILEGE_MIXER                     -8    // �̼��� ����� ������ ����

#define  SCERR_NO_FILE                             -10   // ������ ����
#define  SCERR_NOT_OPEN                            -11   // ������ �� �� ��
#define  SCERR_NOT_WAV_FILE                        -12   // WAV ������ �ƴ�
#define  SCERR_NO_wav_info                       -13   // WAV ���� ������ ����

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
static int            fd_mixer         = -1;       // volume ����� ���� ��ũ����
static unsigned long  data_offset      =  0;       // wav �÷��̿� ���� ������

//------------------------------------------------------------------------------
// ���� : �޽����� ����ϰ� ���α׷� ����
// �μ� : format     : ����� ���ڿ� ���� ��) "%s%d"
//        ...        : ���Ŀ� �ʿ��� �μ�
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
// ���� : ���� ����� ���� ���� ũ�⸦ ���Ѵ�.
// ��ȯ : ���� ��� ���� ũ��
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
// ���� : ���� ī�� ����� ���׷����� ����
// �μ� : _enable - 1 : ���׷���
//                  0 : ���
//------------------------------------------------------------------------------
void  soundcard_set_stereo( int _enable)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_STEREO, &_enable);
}
//------------------------------------------------------------------------------
// ���� : ���� ī�� ��� ��Ʈ ����Ʈ�� ����
// �μ� : _rate   - ������ ��Ʈ ����Ʈ
//------------------------------------------------------------------------------
void  soundcard_set_bit_rate( int _rate)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_SPEED, &_rate );
}

//------------------------------------------------------------------------------
// ���� : ��� ������ ��Ʈ ũ�� ����
// �μ� : _size   - ���� ������ ��Ʈ ũ��
//------------------------------------------------------------------------------
void  soundcard_set_data_bit_size( int _size)
{
   if ( 0 <= fd_soundcard)    ioctl( fd_soundcard, SNDCTL_DSP_SAMPLESIZE, &_size);
}

//------------------------------------------------------------------------------
// ���� : sound card ���� ����
// �μ� : _channel - ������ ä�� ��ȣ
//             ����� ���� : SOUND_MIXER_WRITE_VOLUME
//             PCM    ���� : SOUND_MIXER_WRITE_PCM
//             ����Ŀ ���� : SOUND_MIXER_WRITE_SPEAKER
//             ����   ���� : SOUND_MIXER_WRITE_LINE
//             ����ũ ���� : SOUND_MIXER_WRITE_MIC
//             CD     ���� : SOUND_MIXER_WRITE_CD
//             PCM2   ���� : SOUND_MIXER_WRITE_ALTPCM    ��) ESP-MMI�� ����Ŀ ����
//             IGain  ���� : SOUND_MIXER_WRITE_IGAIN
//             ���� 1 ���� : SOUND_MIXER_WRITE_LINE1
//       _left : ������ ���� �� �Ǵ� ���� ��
//       _right: ������ ���� �� �Ǵ� ���� ��
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
// ����: wav ������ ����ϱ� ���� ����
// ��: wav ���ϸ� ����� ����
// �μ�: _filename   : wav ���� �̸�
// ��ȯ: 0 < ���� ���⿡ ����
//       0 > ���� ���� ���� �� ���� �ڵ�
//--------------------------------------------------------------------
int   soundcard_open_file(const char *_filename)
{
   int            fd_wavfile;
   int            read_size;
   unsigned char  buff[BUFF_SIZE+5];

   wav_info_t    *p_wav_info;
   wav_info_t     wav_info;
   unsigned char *ptr;

   if ( 0 != access(_filename, R_OK ) )                              // ������ ����
      return SCERR_NO_FILE;

   fd_wavfile = open(_filename, O_RDONLY );
   if ( 0 > fd_wavfile)                                              // ���� ���� ����
      return SCERR_NOT_OPEN;

                                 // ��� �κ� ó��

   memset( buff, 0 , BUFF_SIZE);
   read_size = read( fd_wavfile, buff, BUFF_SIZE);

   if ( 0 == memmem( buff, BUFF_SIZE, "RIFF", 4 ))                // "RIFF" ���ڿ��� �ִ°��� �˻��Ѵ�.
      return SCERR_NOT_WAV_FILE;

   if ( 0 == memmem( buff, BUFF_SIZE, "WAVE", 4 ))                // "WAVE" ���ڿ��� �ִ°��� �˻��Ѵ�.
      return SCERR_NOT_WAV_FILE;


   p_wav_info = (wav_info_t *)memmem( buff, BUFF_SIZE, "fmt ", 4 ); // ���� ������ ���Ѵ�.
   if ( NULL == p_wav_info)
      return SCERR_NO_wav_info;

   memcpy( &wav_info, p_wav_info, sizeof(wav_info_t) );

                                 //   printf( "CHANNEL   = %dn", (int) wav_info.channels );
                                 //   printf( "DATA BITS = %dn", (int) wav_info.data_bit);
                                 //   printf( "RATE      = %dn", (int) wav_info.rate);

   ptr = (unsigned char *)memmem( buff, BUFF_SIZE, "data", 4 );
   ptr += 8;

   data_offset = (unsigned long)( ptr - buff );

   lseek( fd_wavfile, data_offset , SEEK_SET );                      // ���� �б� ��ġ�� ������ ��ġ�� �̵� ��Ų��.
   soundcard_set_stereo       ( wav_info.channels >= 2 ? 1 : 0);   // sound ���׷��� ��� Ȱ��ȭ
   soundcard_set_data_bit_size( wav_info.data_bit             );   // sound ������ ũ�� ����
   soundcard_set_bit_rate     ( wav_info.rate                 );   // sound ��Ʈ ����Ʈ ����

   return fd_wavfile;
}

//--------------------------------------------------------------------
// ����: ����ī�� ����� �����ϱ� ���� �ݱ�
//--------------------------------------------------------------------
void soundcard_close( void)
{
   close( fd_soundcard);
   close( fd_mixer    );
}

//--------------------------------------------------------------------
// ����: ����ī�� ����� ���� ����
// �μ�: _mode - PLAYER    : ��� ����
// �μ�: _mode - RECODER   : ���� ����
// �μ�: _mode - AUDIO     : ����� ���� ��� ����
// ��ȯ: ����ī�� ���� ��ũ����
//--------------------------------------------------------------------
int   soundcard_open(int _mode)
{
   const char  *dsp_devname   = DSP_DEVICE_NAME;
   const char  *mixer_devname = MIXER_DEVICE_NAME;

   // ���� ī�� ��ġ ����
   if ( 0 != access( dsp_devname, W_OK ) )
      return SCERR_PRIVILEGE_SOUNDCARD;

   fd_soundcard = open( dsp_devname,_mode);
   if ( 0 > fd_soundcard)
      return SCERR_OPEN_SOUNDCARD;

   // MIXER ����

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
   case  SCERR_NO_PROC_DEVICE       :  printx( "/proc/device�� ����");
   case  SCERR_NO_SOUNDCARD         :  printx( "���� ī�尡 ����");
   case  SCERR_NO_MIXER             :  printx( "�ͼ��� ����");
   case  SCERR_PRIVILEGE_SOUNDCARD  :  printx( "���� ī�带 ����� ������ ����");
   case  SCERR_PRIVILEGE_MIXER      :  printx( "�ͼ� ��ġ�� ����� ������ ����");
   case  SCERR_OPEN_SOUNDCARD       :  printx( "���� ī�带 �� �� ����");
   case  SCERR_OPEN_MIXER           :  printx( "�ͼ� ��ġ�� ���� ����");
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
   char    *sound_buff;                                                    // ���� ����� ���� ����
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

   fd_wav_file   = soundcard_open_file( wav_file_name);                    // ����� wave ������ ����       
   switch( fd_wav_file)
   {
   case  SCERR_NO_FILE        :  printx( "WAV ������ ����");
   case  SCERR_NOT_OPEN       :  printx( "WAV ������ �� �� ����");
   case  SCERR_NOT_WAV_FILE   :  printx( "WAV ������ �ƴ�");
   case  SCERR_NO_wav_info  :  printx( "WAV ������ ����");
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
