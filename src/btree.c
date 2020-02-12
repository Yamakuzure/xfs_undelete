/*******************************************************************************
 * xfs_in.c : Functions to work with inode representations of struct xfs_in
 ******************************************************************************/


#include "btree.h"
#include "log.h"
#include "utils.h"


int is_btree_block( uint8_t const* data ) {
	RETURN_ZERO_IF_NULL( data );

	// --- Is this a regular directory inode ? ---
	if ( ( 0 == memcmp( data, XFS_IN_MAGIC, 2 ) )
	  && ( FT_DIR == ( ( data[2] & 0xf0 ) >> 4 ) ) )
		return 1;

	// --- It might be a long directory block ---
	if ( ( 0 == memcmp( data, XFS_DB_MAGIC, 4 ) )
	  || ( 0 == memcmp( data, XFS_DD_MAGIC, 4 ) )
	  || ( 0 == memcmp( data, XFS_DT_MAGIC, 2 ) ) )
		return 1;

	return 0;
}
