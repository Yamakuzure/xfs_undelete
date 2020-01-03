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


int exists( char* path, int type ) {
	RETURN_ZERO_IF_NULL ( path );

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


void format_uuid_str( char* str, uint8_t const* uuid ) {
	RETURN_VOID_IF_NULL( str );
	RETURN_VOID_IF_NULL( uuid );

	snprintf( str, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	          uuid[ 0], uuid[ 1], uuid[ 2], uuid[ 3],
	          uuid[ 4], uuid[ 5],
	          uuid[ 6], uuid[ 6],
	          uuid[ 8], uuid[ 7],
	          uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15] );
}


char const* get_human_size( size_t full_size ) {
	int         reduct        = 0;
	static char result[8]     = { 0x0 };
	static char suffix[12][4] = {
		"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "XiB", "SiB", "DiB"
	};
	size_t      value         = full_size;

	while ( value > 1023 ) {
		value /= 1024;
		++reduct;
	}

	snprintf( result, 8, "%4zu%3s", value, reduct < 12 ? suffix[reduct] : "N/A" );

	return result;
}


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


int mkdirs ( char const* path ) {
	RETURN_INT_IF_NULL( path );

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

	/* === Now create the path structure step by step === */
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


