#include "OasisAPI.h"
#include "OasisLog.h"
#include "ttyraw.h"


TTYRaw::TTYRaw()
	: ttyfd_(STDIN_FILENO), is_raw_(false)
{
	memset(&orig_termios_, 0, sizeof(orig_termios_));
	pipe_fds_[0] = -1;
	pipe_fds_[1] = -1;

	tty_setup();
}

TTYRaw::~TTYRaw()
{
	tty_cleanup();
}


void TTYRaw::tty_setup()
{
	/* store current tty settings in orig_termios_ */
	 if (tcgetattr(ttyfd_, &orig_termios_) < 0) {
		 DLOG(DLOG_ERROR, "can't get tty settings\n");
		 return;
	 }

	 pipe(pipe_fds_);
}

void TTYRaw::tty_cleanup()
{
	tty_reset();

	if(pipe_fds_[0] >= 0) {
		close(pipe_fds_[0]);
		pipe_fds_[0] = -1;
	}
	if(pipe_fds_[1] >= 0) {
		close(pipe_fds_[1]);
		pipe_fds_[1] = -1;
	}
}

/* reset tty - useful also for restoring the terminal when this process
   wishes to temporarily relinquish the tty
*/
int TTYRaw::tty_reset(void)
{
	/* flush and reset */
	if (is_raw_ && tcsetattr(ttyfd_, TCSAFLUSH, &orig_termios_) < 0) {
		DLOG(DLOG_ERROR, "reset error\n");
		return -1;
	}
	is_raw_ = false;
	return 0;
}


/* put terminal in raw mode - see termio(7I) for modes */
void TTYRaw::tty_raw(void)
{
	struct termios raw;

	if(is_raw_) {
		DLOG(DLOG_ERROR, "already raw\n");
		return;
	}

	raw = orig_termios_;  /* copy original and then modify below */

#if 0
	/* input modes - clear indicated ones giving: no break, no CR to NL,
	   no parity check, no strip char, no start/stop output (sic) control */
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

	/* output modes - clear giving: no post processing such as NL to CR+NL */
	raw.c_oflag &= ~(OPOST);

	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);

	/* local modes - clear giving: echoing off, canonical off (no erase with
	   backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
	/*
	 * These special characters may be active in either canonical or noncanonical input mode,
	 * but only when the ISIG flag is set (see section Local Modes).
	 * VINTR, VQUIT, VSUSP
	 */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN /*| ISIG*/);
#else
	//raw.c_lflag &= ~(ECHO | ICANON );
	raw.c_lflag = 0;
#endif

	/* control chars - set return condition: min number of bytes and timer */
#if 0
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 8; /* after a byte or .8 seconds */
#else
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 5; /* .5 seconds */
#endif

	/* put terminal in raw mode after flushing */
	if (tcsetattr(ttyfd_, TCSAFLUSH, &raw) < 0) {
		DLOG(DLOG_ERROR, "can't set raw mode\n");
		return;
	}

	is_raw_ = true;
}

void TTYRaw::tty_cancel(void)
{
	if(pipe_fds_[1] >= 0) {
		close(pipe_fds_[1]);
		pipe_fds_[1] = -1;
	}
}

int TTYRaw::tty_getchar(int32_t timeout)
{
	int bytesread;
	char c_in;
	struct timeval tv;

	//DLOG(DLOG_INFO, ">>\n");
	tty_raw();
#if 0
	do {

		fd_set descriptor_set;

		FD_ZERO(&descriptor_set);
		FD_SET(ttyfd_, &descriptor_set);
		FD_SET(pipe_fds_[0], &descriptor_set);

		tv.tv_sec  = timeout;
		tv.tv_usec = 0;

		if (select(FD_SETSIZE, &descriptor_set, NULL, NULL, timeout > 0 ? &tv : NULL) <= 0) {
			c_in = -1;
			break;
		}

		if (FD_ISSET(STDIN_FILENO, &descriptor_set)) {
			bytesread = read(ttyfd_, &c_in, 1 /* read up to 1 byte */);
			DLOG(DLOG_DEBUG, "read %d 0x%02x, #%d\n", c_in, c_in, bytesread);
			if (bytesread < 0)  {
				//err
				DLOG(DLOG_ERROR, "read error\n");
				c_in = -1;
				break;
			} else if(bytesread == 0) {
				//timeout
				continue;
			} else {
				//DLOG(DLOG_INFO, "read %d, #%d\n", c_in, bytesread);
				if(c_in == 3 /*^C*/) {
					//raise(SIGINT);
				} else if(c_in == 26 /*^Z*/) {
					//raise(SIGTSTP);
				} else if(c_in == 28 /*^\*/) {
					//raise(SIGQUIT);
				}
				break;
			}
		}

		if (FD_ISSET(pipe_fds_[0], &descriptor_set)) {
			c_in = -1;
			break;
		}

	} while(1);
#else
	bytesread = read(ttyfd_, &c_in, 1 /* read up to 1 byte */);
	DLOG(DLOG_DEBUG, "read %d 0x%02x, #%d\n", c_in, c_in, bytesread);
	if (bytesread < 0)  {
		//err
		DLOG(DLOG_ERROR, "read error\n");
		c_in = -1;
		sleep(1);
	} else if(bytesread == 0) {
		//timeout
		c_in = -1;
	}
#endif
	tty_reset();
	//DLOG(DLOG_INFO, "<<\n");

	return (int)(int8_t)c_in;
}
