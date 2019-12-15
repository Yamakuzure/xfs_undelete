#ifndef PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED 1
#pragma once

#include <stdlib.h>

int         exists       ( char* path, int type );
char const* location_info( char const* path, size_t line, char const* func );
int         mkdirs       ( char const* path );


#define flip16(x)            \
	( ( ( (x) & 0x00ff ) << 8) \
	  | ( ( (x) & 0xff00 ) >> 8) )

#define flip32(x)                 \
	( ( ( (x) & 0x000000ff ) << 24) \
	  | ( ( (x) & 0x0000ff00 ) <<  8) \
	  | ( ( (x) & 0x00ff0000 ) >>  8) \
	  | ( ( (x) & 0xff000000 ) >> 24) )


#endif // PWX_XFS_UNDELETE_SRC_UITLS_H_INCLUDED
