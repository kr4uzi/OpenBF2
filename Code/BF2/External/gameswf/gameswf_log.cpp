// gameswf_log.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Helpers for logging messages & errors.


#include "gameswf_log.h"
#include "gameswf.h"

#include <stdio.h>
#include <stdarg.h>
#if !defined(_MSC_VER)
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#endif


namespace gameswf
{
	// Function pointer to log callback.
	static void (*s_log_callback)(bool error, const char* message) = NULL;

	// Workspace for vsnprintf formatting.
	static const int	BUFFER_SIZE = 500;
	static char	s_buffer[BUFFER_SIZE];


	void	register_log_callback(void (*callback)(bool error, const char* message))
	// The host app can use this to install a function to receive log
	// & error messages from gameswf.
	//
	// Pass in NULL to inhibit logging of messages & errors.
	{
		s_log_callback = callback;
	}


#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif // _WIN32

#define FORMAT_INTO_BUFFER(fmt)				\
		va_list ap;				\
		va_start(ap, fmt);			\
		vsnprintf(s_buffer, BUFFER_SIZE, fmt, ap);	\
		va_end(ap);

	void	log_msg(const char* fmt, ...)
	// Printf-style informational log.
	{
		if (s_log_callback == NULL)
		{
			return;
		}

		FORMAT_INTO_BUFFER(fmt);

		s_log_callback(false, s_buffer);
	}


	void	log_error(const char* fmt, ...)
	// Printf-style error log.
	{
		if (s_log_callback == NULL)
		{
			return;
		}

		FORMAT_INTO_BUFFER(fmt);

		s_log_callback(true, s_buffer);
	}

#if !defined(_MSC_VER)
	int	g_swf_mouse_pass_depth = 0;

	bool	swf_list_trace_on()
	{
		static int	s_on = -1;
		if (s_on < 0)
		{
			const char*	v = getenv("BF2_SWF_LIST_TRACE");
			s_on = (v && v[0] >= '1' && v[0] <= '9') ? 1 : 0;
		}
		return s_on != 0;
	}

	void	swf_list_trace(const char* fmt, ...)
	{
		static char	s_last[512];
		static int	s_repeats = 0;
		char	line[512];
		va_list	ap;
		va_start(ap, fmt);
		vsnprintf(line, sizeof(line), fmt, ap);
		va_end(ap);
		if (strcmp(line, s_last) == 0)
		{
			s_repeats++;
			return;
		}
		struct timeval	tv;
		gettimeofday(&tv, NULL);
		struct tm	tmv;
		localtime_r(&tv.tv_sec, &tmv);
		if (s_repeats > 0)
		{
			fprintf(stderr, "[swf-list] (previous line x%d more)\n", s_repeats);
		}
		fprintf(stderr, "%02d:%02d:%02d.%03d [swf-list] %s%s\n", tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
			(int) (tv.tv_usec / 1000), g_swf_mouse_pass_depth > 0 ? "[mouse] " : "", line);
		fflush(stderr);
		strncpy(s_last, line, sizeof(s_last) - 1);
		s_last[sizeof(s_last) - 1] = '\0';
		s_repeats = 0;
	}
#endif
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:

