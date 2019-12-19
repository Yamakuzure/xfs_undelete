/*******************************************************************************
 * xfs_in.c : Functions to work with inode representations of struct xfs_in
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"


#include <string.h>

int xfs_read_in( xfs_in* in, xfs_sb const* sb, uint8_t const* data ) {
	if ( NULL == in ) {
		log_critical( "%s", "BUG: Called without 'in' pointert!" );
		return -1;
	}
	if ( NULL == data ) {
		log_critical( "%s", "BUG: Called without 'data' pointert!" );
		return -1;
	}

	// Copy and check magic first
	memcpy( in->magic, data, 2 );
	if ( strncmp( sb->magic, XFS_IN_MAGIC, 2 ) ) {
		log_critical( "Wrong magic: 0x%02x%02x instead of 0x%02x%02x",
		              in->magic[0],    in->magic[1], XFS_IN_MAGIC[0], XFS_IN_MAGIC[1] );
		return -1;
	}

	// We need the version first, as some features need version v3
	in->version = data[4];

	// Copy the inode number, we might need it for error messages
	in->inode_id = get_flip64u( data, 152 );


	// Copy CRC32 and UUID, then check the UUID
	memcpy( in->in_crc32, data + 100,  4 );
	memcpy( in->sb_UUID,     data + 160, 16 );
	if ( ( in->version > 2 ) && memcmp( sb->UUID, in->sb_UUID, 16 ) ) {
		char in_uuid_str[37] = { 0x0 };
		char sb_uuid_str[37] = { 0x0 };
		format_uuid_str( in_uuid_str, in->sb_UUID );
		format_uuid_str( sb_uuid_str, sb->UUID );
		log_error( "Inode %llu UUID mismatch:", in->version > 2 ? in->inode_id : ( uint64_t ) -1 );
		log_error( "Device UUID: %s", sb_uuid_str );
		log_error( "Inode UUID : %s", in_uuid_str );
		return -1; // skip this!
	}

	// Now just copy/flip the rest
	in->type_mode       = *( ( uint16_t* )( data + 2 ) ); // must not be flipped
	in->file_type       = ( 0xf000 & in->type_mode ) >> 12;
	in->file_mode       =  0x0fff & in->type_mode;
	in->data_fork_type  = get_flip8u(  data,   5 );
	in->num_links_v1    = get_flip16u( data,   6 );
	in->uid             = get_flip32u( data,   8 );
	in->gid             = get_flip32u( data,  12 );

	in->num_links_v2    = get_flip32u( data,  16 );
	in->project_id_lo   = get_flip16u( data,  20 );
	in->project_id_hi   = get_flip16u( data,  22 );
	in->inc_on_flush    = get_flip16u( data,  30 );

	in->atime_ep        = get_flip32u( data,  32 );
	in->atime_ns        = get_flip32u( data,  36 );
	in->mtime_ep        = get_flip32u( data,  40 );
	in->mtime_ns        = get_flip32u( data,  44 );
	in->ctime_ep        = get_flip32u( data,  48 );
	in->ctime_ns        = get_flip32u( data,  52 );

	in->file_size       = get_flip64u( data,  56 );
	in->file_blocks     = get_flip64u( data,  64 );
	in->ext_size_hint   = get_flip32u( data,  72 );
	in->ext_used        = get_flip32u( data,  76 );

	in->num_xattr_exts  = get_flip16u( data,  80 );
	in->xattr_off       = get_flip8u(  data,  82 );
	in->xattr_type_flg  = get_flip8u(  data,  82 );
	in->DMAPI_evnt_flg  = get_flip32u( data,  84 );
	in->DMAPI_state     = get_flip16u( data,  88 );
	in->flags           = get_flip32u( data,  90 );
	in->gen_number      = get_flip32u( data,  92 );

	in->nxt_unlnkd_ptr  = get_flip32u( data,  96 );

	/* v3 inodes (v5 file system) have the following fields */
	in->attr_changes    = get_flip64u( data, 104 );

	in->last_log_seq    = get_flip64u( data, 112 );
	in->ext_flags       = get_flip64u( data, 120 );

	in->cow_ext_size    = get_flip32u( data, 128 );
	in->padding[12]     = get_flip8u( data, 132 );

	in->btime_ep        = get_flip32u( data, 144 );
	in->btime_ns        = get_flip32u( data, 148 );

	return 0;
}
