/*******************************************************************************
 * log.c : Functions for printing log messages. No log file right now...
 ******************************************************************************/


#include "common.h"
#include "log.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <threads.h>


// Protect the writing of log messages, so we can have multiple threads quabbling
static mtx_t output_lock;


// Each thread gets their own buffer and message string, so we can fill them without locking
thread_local static char*  buffer       = NULL;
thread_local static size_t buffer_size  = 0;
thread_local static char*  message      = NULL;
thread_local static size_t message_size = 0;


// Progress lines are fixed (sorry) and need some info
#define     progress_len  93
static char progress_line[progress_len] = { 0x0 };
#define     progress_blnk "                                                                                             "
//                         123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012
//                                  1         2         3         4         5         6         7         8          0
// Demo:                   "[12/12] 1234567890/1234567890 sec (123.45%); 123456789/123456789 found; 123456789 restored"

static bool have_progress     = false;


/// @brief Simple log_level_t to char const*
static char const level_str[6][9] = {
	/* log_DEBUG    */ "*debug**",
	/* log_INFO     */ "==info==",
	/* log_STATUS   */ "-Status-",
	/* LOG_WARNING  */ "Warning ",
	/* log_ERROR    */ " ERROR  ",
	/* log_CRITICAL */ "CRITICAL"
};


static int resize_buf( char** buf, size_t* buf_size, size_t new_size ) {
	if ( new_size > *buf_size ) {
		char* new_buffer = NULL;

		if ( *buf && *buf_size )
			new_buffer = realloc( *buf, new_size );
		else
			new_buffer = calloc( new_size, 1 );

		if ( NULL == new_buffer ) {
			if ( *buf ) {
				free( *buf );
				*buf = NULL;
			}
			*buf_size = 0;
			return -ENOMEM;
		}
		if ( *buf != new_buffer )
			*buf = new_buffer;
		*buf_size    = new_size;
	}
	return 0;
}


// Internal generation and writing
static void log_direct_out( char const* time, log_level_t level, char const* location,
                            size_t intro_size, char const* fmt, va_list* ap ) {

	int r;

	// Now generate the intro
	r = resize_buf( &buffer, &buffer_size, intro_size + 1 );
	if ( r ) {
		fprintf( stderr, "CRITICAL: Failed to allocate %zu bytes for log buffer!", intro_size + 1 );
		return;
	}
	// Note: If set in debug mode, location comes with its own pipe.
	snprintf( buffer, intro_size, "%s|%s|%s", time, level_str[level], location );

	// Copy the intro
	r = resize_buf( &message, &message_size, buffer_size + 1 );
	if ( r ) {
		fprintf( stderr, "CRITICAL: Failed to allocate %zu bytes for log message!", buffer_size + 1 );
		return;
	}
	strncpy( message, buffer, buffer_size );
	memset( buffer, 0, buffer_size );


	// The full message must be expanded. The buffer is free now.
	// Note: The queued items don't have a va_list copy, as it would be invalid by now.
	size_t text_len;
	if ( ap ) {
		// We have to make a copy, or *ap would be invalidated for the later
		// application if the buffer wasn't big enough and *ap had to be reapplied.
		va_list ap_test;
		va_copy( ap_test, *ap );
		text_len = vsnprintf( buffer, buffer_size - 1, fmt, ap_test );
		va_end( ap_test );
	} else
		text_len = strlen( fmt );

	// Is the buffer big enough?
	if ( text_len >= buffer_size ) {
		r = resize_buf( &buffer, &buffer_size, text_len + 1 );
		if ( r ) {
			fprintf( stderr, "CRITICAL: Failed to allocate %zu bytes for log buffer!", text_len + 1 );
			return;
		}
		if ( ap )
			// Re-apply, the buffer is big enough, now.
			vsnprintf( buffer, text_len, fmt, *ap );
	}

	// Eventually copy fmt if this has no ap:
	if ( NULL == ap )
		strncpy( buffer, fmt, text_len );


	// Now the message can be completed
	size_t full_size = strlen( message ) + text_len;
	if ( full_size >= message_size ) {
		r = resize_buf( &message, &message_size, full_size + 1 );
		if ( r ) {
			fprintf( stderr, "CRITICAL: Failed to allocate %zu bytes for log message!", full_size + 1 );
			return;
		}
	}
	strncat( message, buffer, text_len );

	mtx_lock( &output_lock );
	FILE* target = level > LOG_WARNING ? stderr : stdout;
	if ( have_progress ) {
		fprintf( target, "\r%s\r", progress_blnk );
		have_progress = false;
	}
	fprintf ( target, "%s\n", message );
	fflush( target );
	mtx_unlock( &output_lock );
}


void show_progress( char const* fmt, ... ) {

	// We use a fixed buffer here, but warn if it is too small, as that is considered to be a bug
	size_t  text_len = progress_len;
	va_list ap;
	va_start ( ap, fmt );
	text_len = vsnprintf( progress_line, text_len, fmt, ap );
	va_end( ap );

	// Warn if we are too big
	if ( text_len >= progress_len )
		log_warning( "Progress line needs %zu characters, but %d are the limit!",
		             text_len, progress_len - 1 );

	// Lock, we do not want to be disturbed, now.
	mtx_lock( &output_lock );

	if ( have_progress )
		fprintf( stdout, "\r%s\r", progress_blnk );
	fprintf ( stdout, "%s", progress_line );
	have_progress = true;
	fflush( stdout );

	mtx_unlock( &output_lock );
}


// Preparation of the log message
void pwx_log( char const* location, log_level_t level, char const* message, ... ) {
	// First, get the date and time string
	static char timebuf[20] = { 0x0 }; // YYYY-mm-dd HH:MM:SS + 0x0
	time_t      t           = time( NULL );
	struct tm   tm;


	if ( localtime_r( &t, &tm ) ) {
		snprintf( timebuf, 20, "%04hu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu",
		          ( uint16_t )1900 + ( uint8_t )tm.tm_year,
		          ( uint8_t )tm.tm_mon + 1,
		          ( uint8_t )tm.tm_mday,
		          ( uint8_t )tm.tm_hour,
		          ( uint8_t )tm.tm_min,
		          ( uint8_t )tm.tm_sec );
	}

	// If no location is provided, offer a default
#if defined(PWX_DEBUG)
	char const* real_loc = location ? location : "<unknown>";
	// Intro Size: date (19) + level (9) + 3 pipes + 0x0 = 32
	size_t intro_size = 32 + strlen( real_loc );
#else
	static size_t      intro_size = 32;
	static char const* real_loc   = "";
#endif // debug mode

	// Now that we have the point in time, delegate to the log message builder
	va_list ap;
	va_start ( ap, message );
	log_direct_out( timebuf, level, real_loc, intro_size, message, &ap );
	va_end( ap );
}
