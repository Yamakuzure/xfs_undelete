/*******************************************************************************
 * utils.c : General utilities
 ******************************************************************************/


#include "common.h"
#include "log.h"
#include "utils.h"


#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


/** @brief check whether a file or directory exists.
  * @param[in] path  The path to check
  * @param[in] type Either 'd' for [d]irectory, or 'f' for [f]ile
  * @return 1 if it exists, 0 otherwise.
**/
int exists( char* path, int type ) {
	if ( NULL == path ) {
		log_critical( "%s", "BUG: Called without a path!" );
		return 0;
	}

	struct stat buf;

	switch( type )	{
		case 'f':
			return !stat( path, &buf ) && S_ISREG( buf.st_mode );
			break;
		case 'd':
			return !stat( path, &buf ) && S_ISDIR( buf.st_mode );
			break;
		default:
			log_critical( "BUG: Called with '%c' type!", type );
	}

	return 0;
} /* exists */


/** @brief compile a location information string
  *
  * The output format is:
  *   basename(path):line:func
  * in DEBUG mode. Otherwise an empty string is returned.
  *
  * @param[in] path  Path to the file
  * @param[in] line  Line number in @a path
  * @param[in] func  Representation of the function
  *
  * @return Pointer to a static buffer. This must *NOT* be freed!
**/
char const* location_info( char const* path, size_t line, char const* func ) {
	static char base_buffer  [PATH_MAX] = { 0 };
#if defined(PWX_DEBUG)
	static char base_filename[PATH_MAX] = { 0 };
	strncpy ( base_filename, path, PATH_MAX - 1 );
	snprintf( base_buffer, PATH_MAX, "%s:%lu:%s|",
	          basename( base_filename ), line, func );
#endif // debug mode
	return base_buffer;
}


/** @brief Create a full path like 'mkdir -p'
  * @param[in] path  The path to create
  * @return 0 on success, -1 on failure.
**/
int mkdirs ( char const* path ) {
	if ( NULL == path ) {
		log_critical( "%s", "BUG: Called without a path!" );
		return -1;
	}

	static char buffer[PATH_MAX + 1]     = { 0x0 };
	static char check_path[PATH_MAX + 1] = { 0x0 };
	static char full_path[PATH_MAX + 1]  = { 0x0 };
	static char prefix[256]              = { 0x0 };
	size_t prefix_len  = 0;
	int  ret           = 0;

	memset( buffer,     0, PATH_MAX );
	memset( check_path, 0, PATH_MAX );
	memset( full_path,  0, PATH_MAX );
	memset( prefix,     0, 255 );

	strncpy( buffer, path, PATH_MAX );

	/* === If this is a network path, ignore the host/drive prefix === */
	if ( buffer[0] && ( '/' == buffer[0] )
	                && buffer[1] && ( '/' == buffer[1] ) ) {
		/* //host/drive/path */

		// Find first separator after <host>
		char* next = strchr( buffer + 2, '/' );

		if ( NULL == next ) {
			log_critical( "%s is invalid (no host)", path );
			return -1;
		}

		// Find second separator after <drive>
		next = strchr( next + 1, '/' );
		if ( NULL == next ) {
			log_critical( "%s is invalid (no drive)", path );
			return -1;
		}

		// Now split the path into prefix and path we can create.
		prefix_len = next - buffer;
		snprintf( prefix, 255, "//%s", prefix_len );
		strncpy( full_path, next + 1, PATH_MAX - prefix_len - 1 );
	} else
		strncpy ( full_path, buffer, PATH_MAX + 1 );

	/* === Now create the path strcture step by step === */
	size_t full_len = strlen( full_path );
	size_t offset   = full_path[0] == '/' ? 1 : 0;
	char*  next;

	while  ( offset <= full_len ) {
		next = strchr( full_path + offset, '/' );

		// Set the separator to 0x0, so the rest is ignored.
		if ( next )
			// note: When the last part is handled, next is NULL
			*next = 0x0;

		// Now create the portion up to 'next' if needed.
		snprintf( check_path, PATH_MAX, "%s%s", prefix, full_path );
		if ( !exists( check_path, 'd' ) && mkdir( check_path, 0777 ) ) {
			ret = -1;
			goto mkdirs_end;
		}

		// Restore next and increase offset.
		if ( next )
			*next = '/';
		offset = next ? ( size_t )( next + 1 - full_path ) : full_len + 1;
	}

mkdirs_end:
	if ( -1 == ret )
		log_critical( "Unable to create output directory %s : %m [%d]", path, errno );
	return ret;
}


