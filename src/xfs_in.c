/*******************************************************************************
 * xfs_in.c : Functions to work with inode representations of struct xfs_in
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "forensics.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"


#include <ctype.h>
#include <string.h>


size_t DATA_START_V1 = 0x64;
size_t DATA_START_V3 = 0xB0;


static int build_data_map( xfs_in* in, uint16_t inode_size, uint8_t const* data ) {
	size_t start = in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	size_t end   = in->xattr_off ? start + ( in->xattr_off * 8 ) : inode_size;

	if ( ST_DEV == in->data_fork_type ) {
		log_error( "Special device data forks (0x%02x) are not supported.", in->data_fork_type );
		log_info( "Ignoring inode %llu!", in->inode_id );
		return -1;
	} else if ( ST_LOCAL == in->data_fork_type ) {
		// Check whether the data part is in bounds
		size_t last = start + in->file_size - 1;
		if ( last >= end ) {
			log_error( "Local data block ends out of bounds at byte %zu/%zu", last, end );
			return -1;
		}

		// Create the buffer
		in->d_loc_data = malloc( in->file_size );
		if ( NULL == in->d_loc_data ) {
			log_critical( "Unable to allocate %zu bytes for local data block!",
			              in->file_size );
			return -1;
		}

		memcpy( in->d_loc_data, data + start, in->file_size );
	} else if ( ST_EXTENTS == in->data_fork_type ) {
		xfs_ex* curr  = NULL;
		xfs_ex* next  = NULL;
		size_t  first = 0; // first byte to read;
		size_t  last  = 0; // last byte to read

		for ( size_t i = 0; i < in->ext_used; ++i ) {
			// First check the end of the reading. Don't overshoot!
			first = start + ( 16 * i );
			last  = first + 15;
			if ( last >= end ) {
				log_error( "Data extent %zu ends out of bounds at byte %zu/%zu",
				           i, last, end );
				return -1;
			}

			// Then allocate another extent structure
			next = calloc( 1, sizeof( xfs_ex ) );
			if ( NULL == next ) {
				log_critical( "Unable to allocate %lu bytes for data extent!",
				              sizeof( xfs_ex ) );
				return -1;
			}

			// Can it be read? (Should. Only errors are bugs if either is NULL)
			if ( -1 == xfs_read_ex( next, data + first ) )
				return -1;

			// Set root or extent chain
			if ( curr )
				curr->next = next;
			else
				in->d_ext_root = next;
			curr = next;
			next = NULL;
		}
	} else if ( ST_BTREE == in->data_fork_type ) {
		/// @todo Implement B-Tree reading
	} else {
		log_error( "Ignoring inode %llu with unknown data fork type 0x%02x!",
		           in->inode_id, in->data_fork_type );
		return -1;
	}

	return 0;
}


static void build_xattr_map( xfs_in* in, uint16_t inode_size, uint8_t const* data ) {
	size_t start = ( in->xattr_off * 8 ) + in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	size_t end   = inode_size;

	if ( ST_DEV == in->xattr_type_flg ) {
		log_error( "Special device xattr forks (0x%02x) are not supported.", in->xattr_type_flg );
		log_info( " ==> Ignoring extended attributes for inode %llu!", in->inode_id );
		return;
	} else if ( ST_LOCAL == in->xattr_type_flg ) {
		// Let's get the size of the attributes, first
		uint16_t xattr_size = get_flip16u( data, start ); // The length includes the 4 bytes header

		// Check whether the yattr part is in bounds
		size_t last = start + xattr_size - 1;
		if ( last >= end ) {
			log_error( "Local xattr block ends out of bounds at byte %zu/%zu", last, end );
			log_info( " ==> Ignoring extended attributes for inode %llu!", in->inode_id );
			return;
		}

		// unpack xattr data
		in->xattr_root = unpack_xattr_data( data + start, xattr_size );
		// No check here. xattrs aren't that mission critical!
	} else if ( ST_EXTENTS == in->xattr_type_flg ) {
		xfs_ex* curr  = NULL;
		xfs_ex* next  = NULL;
		size_t  first = 0; // first byte to read;
		size_t  last  = 0; // last byte to read

		for ( size_t i = 0; i < in->ext_used; ++i ) {
			// First check the end of the reading. Don't overshoot!
			first = start + ( 16 * i );
			last  = first + 15;
			if ( last >= end ) {
				log_error( "xattr extent %zu ends out of bounds at byte %zu/%zu",
				           i, last, end );
				log_info( " ==> Ignoring remaining extended attributes for inode %llu!",
				          in->inode_id );
				return;
			}

			// Then allocate another extent structure
			next = calloc( 1, sizeof( xfs_ex ) );
			if ( NULL == next ) {
				log_critical( "Unable to allocate %lu bytes for xattr extent!", sizeof( xfs_ex ) );
				log_info( " ==> Ignoring remaining extended attributes for inode %llu!",
				          in->inode_id );
				return;
			}

			// Can it be read? (Should. Only errors are bugs if either is NULL)
			if ( -1 == xfs_read_ex( next, data + first ) ) {
				log_info( " ==> Ignoring remaining extended attributes for inode %llu!",
				          in->inode_id );
				return;
			}

			// Set root or extent chain
			if ( curr )
				curr->next = next;
			else
				in->x_ext_root = next;
			curr = next;
			next = NULL;
		}
	} else if ( ST_BTREE == in->xattr_type_flg ) {
		/* Can they even exist? */
		log_error( "xattr btrees (0x%02x) are not supported.", in->xattr_type_flg );
		log_info( " ==> Ignoring extended attributes for inode %llu!", in->inode_id );
		return;
	} else {
		log_error( "Ignoring xattrs with unknown fork type 0x%02x!", in->xattr_type_flg );
		log_info( " ==> Ignoring extended attributes for inode %llu!", in->inode_id );
		return;
	}
}


static xattr* free_xattr_chain( xattr* root ) {
	if ( root ) {
		xattr* curr = root;
		xattr* next = curr->next;
		while ( curr ) {
			FREE_PTR( curr->name );
			FREE_PTR( curr->value );
			free( curr );
			curr = next;
			if ( curr )
				next = curr->next;
		}
	}
	return NULL;
}


bool is_xattr_head( uint8_t const* data, size_t data_size, uint16_t* x_size, uint8_t* x_count, uint8_t* padding) {
	bool     result      = false;
	uint16_t xattr_size  = 0;
	uint8_t  xattr_count = 0;
	uint8_t  xattr_padd  = 0;

	if ( NULL == data )
		goto finish; // nothing to do

	xattr_size  = get_flip16u( data, 0 ); // The length includes the 4 bytes header
	if ( (0 == xattr_size) || (xattr_size > data_size) ) {
		// This is not xattr local data
		xattr_size = 0;
		goto finish;
	}

	xattr_count = data[2];
	xattr_padd  = data[3];
	result      = true;

finish:
	if ( x_size  ) *x_size  = xattr_size;
	if ( x_count ) *x_count = xattr_count;
	if ( padding ) *padding = xattr_padd;

	return result;
}


// Returns the root of an unpacked xattr chain, or NULL if no chain was found
xattr* unpack_xattr_data( uint8_t const* data, size_t data_len ) {
	uint16_t xattr_size  = 0;
	uint8_t  xattr_count = 0;
	uint8_t  xattr_padd  = 0;

	if ( !is_xattr_head(data, data_len, &xattr_size, &xattr_count, &xattr_padd))
		return NULL; // nothing to do

	size_t   offset      = 4; // Start after header
	xattr*   curr        = NULL;
	xattr*   next        = NULL;
	xattr*   root        = NULL;

	for ( size_t i = 0; i < xattr_count; ++i ) {
		uint8_t name_len = data[offset];
		uint8_t val_len  = data[offset + 1];
		size_t  end_byte = offset + 2 + name_len + xattr_padd + val_len - 1;

		// Make sure we are not out of bounds!
		if ( end_byte >= xattr_size ) {
			return root;
		}

		// See whether the name makes sense
		bool name_is_valid = true;
		for ( size_t i = 0; name_is_valid && ( i < name_len ); ++i ) {
			uint8_t check = data[offset + 3 + i];
			if ( !( isprint( check )                  // Must be printable, unless ...
			                || ( !check && ( ( i + 1 ) == name_len ) ) // ... it's the last character
			      ) ) name_is_valid = false;
		}
		if ( !name_is_valid )
			return root; // No fuss...

		// See whether the value makes sense
		bool value_is_valid = true;
		for ( size_t i = 0; value_is_valid && ( i < val_len ); ++i ) {
			uint8_t check = data[offset + 3 + name_len + xattr_padd + i];
			if ( !( isprint( check )                 // Must be printable, unless ...
			                || ( !check && ( ( i + 1 ) == val_len ) ) // ... it's the last character
			      ) ) value_is_valid = false;
		}
		if ( !value_is_valid )
			return root;

		// Allocate buffers for the name and value
		// But add 1 byte, as xattr names are not NULL terminated,
		// and values might, too, come without termination!
		char* name = calloc( name_len + 1, 1 );
		if ( NULL == name ) {
			log_critical( "Unable to allocate %lu bytes for xattr name!", name_len + 1 );
			return root;
		}
		char* value = calloc( val_len + 1, 1 );
		if ( NULL == name ) {
			log_critical( "Unable to allocate %lu bytes for xattr value!", val_len + 1 );
			FREE_PTR( name );
			return root;
		}
		memcpy( name,  data + offset + 3, name_len );
		memcpy( value, data + offset + 3 + name_len + xattr_padd, val_len );

		// Allocate the new xattr entry
		next = calloc( 1, sizeof( xattr ) );
		if ( NULL == next ) {
			log_critical( "Unable to allocate %zu bytes for xattr entry!", sizeof( xattr ) );
			log_info( "%s", " ==> Ignoring remaining extended attributes for this inode!" );
			FREE_PTR( name );
			FREE_PTR( value );
			return root;
		}

		next->flags = data[offset + 2];
		next->name  = TAKE_PTR( name );
		next->value = TAKE_PTR( value );

		if ( curr )
			curr->next = next;
		else
			root = next;
		curr   = next;
		next   = NULL;
		offset = end_byte + 1;
	}
	return root;
}


void xfs_free_in( xfs_in** in ) {
	RETURN_VOID_IF_NULL( in );
	if ( NULL == *in )
		// Simply nothing to do
		return;

	// shortcut
	xfs_in* lin = *in; // [l]ocal in

	// Remove data extent list
	if ( lin->d_ext_root ) {
		xfs_ex* curr    = lin->d_ext_root;
		xfs_ex* next    = curr->next;
		lin->d_ext_root = NULL;
		while ( curr ) {
			free( curr );
			curr = next;
			if ( curr )
				next = curr->next;
		}
	}

	// Remove local data (Maybe possible in some future XFS version)
	if ( lin->d_loc_data ) {
		FREE_PTR( lin->d_loc_data );
	}

	// Remove xattr extent list
	if ( lin->x_ext_root ) {
		xfs_ex* curr    = lin->x_ext_root;
		xfs_ex* next    = curr->next;
		lin->x_ext_root = NULL;
		while ( curr ) {
			free( curr );
			curr = next;
			if ( curr )
				next = curr->next;
		}
	}

	// Remove unpacked local xattr list
	lin->xattr_root = free_xattr_chain( lin->xattr_root );

	FREE_PTR( *in );
}


int xfs_read_in( xfs_in* in, xfs_sb const* sb, uint8_t const* data, int fd ) {
	RETURN_INT_IF_NULL( in );
	RETURN_INT_IF_NULL( sb );
	RETURN_INT_IF_NULL( data );

	// Copy and check magic first
	memcpy( in->magic, data, 2 );
	if ( memcmp( sb->magic, XFS_IN_MAGIC, 2 ) ) {
		log_error( "Wrong magic: 0x%02x%02x instead of 0x%02x%02x",
		           in->magic[0],    in->magic[1], XFS_IN_MAGIC[0], XFS_IN_MAGIC[1] );
		return -1;
	}

	// As the magic is correct, check whether this is a deleted inode or a directory
	in->is_deleted   = is_deleted_inode( data )   > 0 ? true : false;
	in->is_directory = is_directory_block(data) > 0 ? true : false;
	if ( !(in->is_deleted || in->is_directory) )
		// Uninteresting for us
		return -1;

	// We need the version first, as some features need version v3
	in->version = data[4];

	// Copy the inode number, we might need it for error messages
	if ( in->version > 2 )
		in->inode_id = get_flip64u( data, 152 );
	else
		in->inode_id = 0;


	// Copy CRC32 and UUID, then check the UUID
	memcpy( in->in_crc32, data + 100,  4 );
	memcpy( in->sb_UUID,  data + 160, 16 );
	if ( ( in->version > 2 ) && memcmp( sb->UUID, in->sb_UUID, 16 ) ) {
		char in_uuid_str[37] = { 0x0 };
		char sb_uuid_str[37] = { 0x0 };
		format_uuid_str( in_uuid_str, in->sb_UUID );
		format_uuid_str( sb_uuid_str, sb->UUID );
		log_error( "Inode %llu UUID mismatch:", in->inode_id );
		log_error( "Device UUID: %s", sb_uuid_str );
		log_error( "Inode UUID : %s", in_uuid_str );
		return -1; // skip this!
	}

	// Now just copy/flip the data for recovery
	in->type_mode       = get_flip16u( data,   2 ); // <-- (ZEROED on delete)
	in->data_fork_type  = get_flip8u(  data,   5 ); // <-- (FORCED 2 on delete)
	in->num_links_v1    = get_flip16u( data,   6 ); // <-- (ZEROED on delete)
	in->num_links_v2    = get_flip32u( data,  16 ); // <-- (ZEROED on delete)
	in->file_size       = get_flip64u( data,  56 ); // <-- (ZEROED on delete)
	in->file_blocks     = get_flip64u( data,  64 ); // <-- (ZEROED on delete)
	in->ext_used        = get_flip32u( data,  76 ); // <-- (ZEROED on delete)
	in->num_xattr_exts  = get_flip16u( data,  80 ); // <-- (ZEROED on delete)
	in->xattr_off       = get_flip8u(  data,  82 ); // <-- (ZEROED on delete)
	in->xattr_type_flg  = get_flip8u(  data,  83 ); // <-- (FORCED 2 on delete)

	// Try to recover data fork type, extents used/B-Tree root
	// and where the extended attributes start, if they are local.
	if ( in->is_deleted ) {
		if ( -1 == restore_inode( in, sb->inode_size, data, fd ) )
			// Completely fubar!
			return -1;
	} else
		// So as this is a directory, note it down
		in->ftype = FT_DIR;

	// Now read the rest of the inode data
	in->uid             = get_flip32u( data,   8 );
	in->gid             = get_flip32u( data,  12 );
	in->project_id_lo   = get_flip16u( data,  20 );
	in->project_id_hi   = get_flip16u( data,  22 );
	in->inc_on_flush    = get_flip16u( data,  30 );
	in->atime_ep        = get_flip32u( data,  32 );
	in->atime_ns        = get_flip32u( data,  36 );
	in->mtime_ep        = get_flip32u( data,  40 );
	in->mtime_ns        = get_flip32u( data,  44 );
	in->ctime_ep        = get_flip32u( data,  48 );
	in->ctime_ns        = get_flip32u( data,  52 );
	in->ext_size_hint   = get_flip32u( data,  72 );
	in->DMAPI_evnt_flg  = get_flip32u( data,  84 );
	in->DMAPI_state     = get_flip16u( data,  88 );
	in->flags           = get_flip32u( data,  90 );
	in->gen_number      = get_flip32u( data,  92 );
	in->nxt_unlnkd_ptr  = get_flip32u( data,  96 );
	/* v3 inodes (v5 file system) have the following fields */
	if (in->version > 2) {
		in->attr_changes    = get_flip64u( data, 104 );
		in->last_log_seq    = get_flip64u( data, 112 );
		in->ext_flags       = get_flip64u( data, 120 );
		in->cow_ext_size    = get_flip32u( data, 128 );
		memcpy(in->padding, data + 132, 12);
		in->btime_ep        = get_flip32u( data, 144 );
		in->btime_ns        = get_flip32u( data, 148 );
	}

	// Handle file/directory storage (local, extents or btree)
	if ( -1 == build_data_map( in, sb->inode_size, data ) )
		return -1; // Already told what's wrong

	// Handle xattr storage (local, extents or btree)
	if ( NULL == in->xattr_root )
		// Note: recover_inode() already unpacks xattr local data,
		//       this here is only for directory nodes.
		build_xattr_map( in, sb->inode_size, data );
		// Note 2: No check here. xattrs aren't that mission critical!

	// We here? Fine!

	return 0;
}
