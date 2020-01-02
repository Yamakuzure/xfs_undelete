#ifndef PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED 1
#pragma once


#include <stdlib.h>


/** @brief check whether a file or directory exists.
  * @param[in] path  The path to check
  * @param[in] type Either 'd' for [d]irectory, or 'f' for [f]ile
  * @return 1 if it exists, 0 otherwise.
**/
int exists( char* path, int type );


/** @brief format @a uuid into @a str
  *
  * @a str *must* have at least 16 bytes!
  * @a tgt *must* have at least 37 bytes ! (36 chars plus \0)
  * Output format is aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee
  *
  * @param[out] str  The target string
  * @param[in]  uuid The source byte stream
**/
void format_uuid_str( char* str, uint8_t const* uuid );


/** @brief return a pointer to a static string with @a full_size as human readable short version
  *
  * Scale the value down by 1024 until a a value <1024 is reached and add the
  * corresponding size suffix.
  *
  * @param[in] full_size  The full size value in bytes.
  * @return Pointer to a char array with the result. This must *NOT* be freed!
**/
char const* get_human_size( size_t full_size );


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
char const* location_info( char const* path, size_t line, char const* func );


/** @brief Create a full path like 'mkdir -p'
  * @param[in] path  The path to create
  * @return 0 on success, -1 on failure.
**/
int mkdirs( char const* path );


#define flip16(x)                  \
	( ( ( (x) & 0x00ff ) << 8) \
	| ( ( (x) & 0xff00 ) >> 8) )

#define flip32(x)                       \
	( ( ( (x) & 0x000000ff ) << 24) \
	| ( ( (x) & 0x0000ff00 ) <<  8) \
	| ( ( (x) & 0x00ff0000 ) >>  8) \
	| ( ( (x) & 0xff000000 ) >> 24) )

#define flip64(x)                               \
	( ( ( (x) & 0x00000000000000ff ) << 56) \
	| ( ( (x) & 0x000000000000ff00 ) << 40) \
	| ( ( (x) & 0x0000000000ff0000 ) << 24) \
	| ( ( (x) & 0x00000000ff000000 ) <<  8) \
	| ( ( (x) & 0x000000ff00000000 ) >>  8) \
	| ( ( (x) & 0x0000ff0000000000 ) >> 24) \
	| ( ( (x) & 0x00ff000000000000 ) >> 40) \
	| ( ( (x) & 0xff00000000000000 ) >> 56) )

// Some helpful wrappers for getting flipped data:
#define get_flip8u( _b, _o) (_b)[(_o)]
#define get_flip16u(_b, _o) flip16( *( ( uint16_t* )( (_b) + (_o) ) ) )
#define get_flip32u(_b, _o) flip32( *( ( uint32_t* )( (_b) + (_o) ) ) )
#define get_flip64s(_b, _o) flip64( *( (  int64_t* )( (_b) + (_o) ) ) )
#define get_flip64u(_b, _o) flip64( *( ( uint64_t* )( (_b) + (_o) ) ) )

// Just a shortcut to make some pointer manipulations easier
#define FREE_PTR(p) if (p) { free((void*)(p)); } p = NULL
#define TAKE_PTR(p) (p); p = NULL
#define RELEASE(p)  (p)->prev = NULL; (p)->next = NULL

// Shortcut to dump a strip of data, hexdump -C style
// Note: Uses log_debug() so it does nothing on release builds.
#define DUMP_STRIP(_off, _dat) \
	log_debug("%08x |"     \
		  " %02x %02x %02x %02x %02x %02x %02x %02x " \
		  " %02x %02x %02x %02x %02x %02x %02x %02x " \
		  "| %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",       \
		  _off,                                       \
		  _dat[0], _dat[1], _dat[ 2], _dat[ 3], _dat[ 4], _dat[ 5], _dat[ 6], _dat[ 7], \
		  _dat[8], _dat[9], _dat[10], _dat[11], _dat[12], _dat[13], _dat[14], _dat[15], \
		  isprint(_dat[ 0]) ? _dat[ 0] : '.', isprint(_dat[ 1]) ? _dat[ 1] : '.',       \
		  isprint(_dat[ 2]) ? _dat[ 2] : '.', isprint(_dat[ 3]) ? _dat[ 3] : '.',       \
		  isprint(_dat[ 4]) ? _dat[ 4] : '.', isprint(_dat[ 5]) ? _dat[ 5] : '.',       \
		  isprint(_dat[ 6]) ? _dat[ 6] : '.', isprint(_dat[ 7]) ? _dat[ 7] : '.',       \
		  isprint(_dat[ 8]) ? _dat[ 8] : '.', isprint(_dat[ 9]) ? _dat[ 9] : '.',       \
		  isprint(_dat[10]) ? _dat[10] : '.', isprint(_dat[11]) ? _dat[11] : '.',       \
		  isprint(_dat[12]) ? _dat[12] : '.', isprint(_dat[13]) ? _dat[13] : '.',       \
		  isprint(_dat[14]) ? _dat[14] : '.', isprint(_dat[15]) ? _dat[15] : '.')


#endif // PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED
