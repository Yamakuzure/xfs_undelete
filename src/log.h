#ifndef PWX_XFS_UNDELETE_SRC_LOG_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_LOG_H_INCLUDED 1
#pragma once


#include "utils.h"


/// @brief enumeration of log levels
typedef enum {
	LOG_DEBUG = 0, //!< Log everything including debug level messages
	LOG_INFO,      //!< Informational messages that are only interesting in development
	LOG_STATUS,    //!< Status messages like progress and counts
	LOG_WARNING,   //!< Something is off, but does not interrupt operation
	LOG_ERROR,     //!< A non-critical error has been encountered, the program can proceed
	LOG_CRITICAL   //!< A critical error has been encountered, the program must be terminated
} log_level_t;


/** @brief Central logging function
  *
  * The unified output is:
  *
  *    `%Y-%m-%d %H:%M:%S|<level>|<filename>:<lineno>:<function>|<message>`
  *
  * The message is split to not be longer than 120 characters per line. Follow-up lines
  * begin two spaces left of the first pipe, only singling out the date and time.
  *
  * Please use the helper macros `log_debug()`, `log_info()`, `log_status()`,
  * `log_warning()`, `log_error()` and `log_critical()`, which already fill in
  * `location` and `level`.
  *
  * This function is thread safe, it uses a central lock for concurrent writing.
  *
  * @param[in] location The location in the form <filename>:<lineno>:<function>
  * @param[in] level The severity of the message
  * @param[in] message Whatever you wish to tell your readers, printf style, with arguments following.
**/
void pwx_log( char const* location, log_level_t level, char const* message, ... );


/** @brief show a progress line
  *
  * These lines are not ended with a line break, and each call to this
  * function erases the previous line and flushes stdout.
  *
  * @param[in] fmt  The format string
  * @param[in] ...  Format arguments
**/
void show_progress( char const* fmt, ... );


/* --- Logging helper macros --- */
#define PWX_log_internal(_l_, _m_, ...)                       \
	pwx_log( location_info(__FILE__, __LINE__, __func__), \
	         _l_, _m_, __VA_ARGS__ )
#define log_info(    message_, ...) PWX_log_internal(LOG_INFO,     message_, __VA_ARGS__)
#define log_status(  message_, ...) PWX_log_internal(LOG_STATUS,   message_, __VA_ARGS__)
#define log_warning( message_, ...) PWX_log_internal(LOG_WARNING,  message_, __VA_ARGS__)
#define log_error(   message_, ...) PWX_log_internal(LOG_ERROR,    message_, __VA_ARGS__)
#define log_critical(message_, ...) PWX_log_internal(LOG_CRITICAL, message_, __VA_ARGS__)

#ifdef PWX_DEBUG
#  define log_debug( message_, ...) PWX_log_internal(LOG_DEBUG,    message_, __VA_ARGS__)
#else
#  define log_debug(...)
#endif // debug enabled

#endif // PWX_XFS_UNDELETE_SRC_LOG_H_INCLUDED
