/*******************************************************************************
 * xfs_ex.c : Functions to work with extent data via struct xfs_ex
 ******************************************************************************/


#include "common.h"
#include "log.h"
#include "utils.h"
#include "xfs_ex.h"


int xfs_read_ex( xfs_ex_t* ex, uint8_t const* data ) {
	RETURN_INT_IF_NULL( ex );
	RETURN_INT_IF_NULL( data );

	ex->is_prealloc = data[0] & 0x80; // == b10000000 aka first bit
	ex->offset      = flip64( ( *( uint64_t* )( data + 0 ) & 0x7ffffffffffffe00 ) >> 9 ); // = Bits 2 to 55
	// 0b01111111 11111111 11111111 11111111 11111111 11111111 11111110 00000000
	//   12345678 90123456 78901234 56789012 34567890 12345678 90123456 78901234
	//             1          2          3          4           5          6
	//          0        1        2        3        4        5        6        7
	ex->block       = flip64( ( *( uint64_t* )( data + 6 ) & 0x01ffffffffffffe0 ) >> 5 ); // = Bits 56 to 107
	// 0b00000001 11111111 11111111 11111111 11111111 11111111 11111111 11100000
	//   90123456 78901234 56789012 34567890 12345678 90123456 78901234 56789012
	//    5          6          7          8           9         10         11
	//          6        7        8        9       10       11       12       13
	ex->length      = flip32( ( *( uint32_t* )( data + 12 ) & 0x001fffff ) ); // = Bits 108-128
	// 0b00000000 00011111 11111111 11111111
	//   78901234 56789012 34567890 12345678
	//     10         11         12
	//         12       13       14       15

	return 0;
}

