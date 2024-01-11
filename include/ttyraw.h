#ifndef TTYRAW_H
#define TTYRAW_H


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>

/*
 * terminal non-cannonical mode (allow ctrl+c)
 * http://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c
 * https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_chapter/libc_17.html
 * https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html
 * http://www.physics.udel.edu/~watson/scen103/ascii.html
 */


class TTYRaw
{
public:
	TTYRaw();
	~TTYRaw();

	void tty_setup();
	void tty_cleanup();
	void tty_cancel(void);
	int tty_getchar(int32_t timeout = 0); //in seconds

protected:

	int tty_reset(void);
	void tty_raw(void);

protected:

	//std::mutex lock_;

	struct termios orig_termios_;  /* TERMinal I/O Structure */
	int ttyfd_;     /* STDIN_FILENO is 0 by default */

	int pipe_fds_[2];


	bool is_raw_;
};















#endif // TTYRAW_H
