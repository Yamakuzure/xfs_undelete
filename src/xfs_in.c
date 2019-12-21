/*******************************************************************************
 * xfs_in.c : Functions to work with inode representations of struct xfs_in
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"


#include <ctype.h>
#include <string.h>


#define DATA_START_V1 0x64
#define DATA_START_V3 0xB0


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
		log_error( "Special device xattr btrees (0x%02x) are not supported.", in->xattr_type_flg );
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


bool is_deleted_inode(uint8_t const* data) {
	if ( NULL == data ) {
		log_critical( "%s", "BUG: Called without 'data' pointer!" );
		return false;
	}

	// Check magic first
	if ( memcmp( data, XFS_IN_MAGIC, 2 ) )
		return false;

	// We need the version first, as some features need version v3
	uint8_t version = data[4];

	// Now copy what is zeroed/force on deletion (No flip needed, everything is fixed anyway)
	uint16_t type_mode       = *((uint16_t*)(data +  2)); // <-- (ZEROED on delete)
	uint8_t  data_fork_type  = *((uint16_t*)(data +  5)); // <-- (FORCED 2 on delete)
	uint16_t num_links_v1    = *((uint16_t*)(data +  6)); // <-- (ZEROED on delete)
	uint32_t num_links_v2    = *((uint16_t*)(data + 16)); // <-- (ZEROED on delete)
	uint64_t file_size       = *((uint16_t*)(data + 56)); // <-- (ZEROED on delete)
	uint64_t file_blocks     = *((uint16_t*)(data + 64)); // <-- (ZEROED on delete)
	uint32_t ext_used        = *((uint16_t*)(data + 76)); // <-- (ZEROED on delete)
	uint8_t  xattr_off       = *((uint16_t*)(data + 82)); // <-- (ZEROED on delete)
	uint8_t  xattr_type_flg  = *((uint16_t*)(data + 83)); // <-- (FORCED 2 on delete)

	// There are some forced values (see above) on deleted inodes.
	// These can be used to identify an inode that got deleted.
	if ( type_mode                           // <-- (ZEROED on delete)
	  || ( data_fork_type != 2 )             // <-- (FORCED 2 on delete)
	  || ( ( version < 3 ) && num_links_v1 ) // <-- (ZEROED on delete)
	  || ( ( version > 2 ) && num_links_v2 ) // <-- (ZEROED on delete)
	  || file_size                           // <-- (ZEROED on delete)
	  || file_blocks                         // <-- (ZEROED on delete)
	  || ext_used                            // <-- (ZEROED on delete)
	  || xattr_off                           // <-- (ZEROED on delete)
	  || ( xattr_type_flg != 2 ) )           // <-- (FORCED 2 on delete)
		// Not a deleted inode, so it is uninteresting for us.
		return false;

	// Found one
	return true;
}


typedef enum _recover_part {
	RP_DATA = 1, //!< Right after the core, extents or the B-Tree root of the data are located
	RP_GAP,      //!< There is (or might be) a zeroed gap between data and xattr
	RP_XATTR,    //!< Extended attributes are local or in extents
	RP_END       //!< xattrs are aligned to the end, but maybe we find strips of zero?
} e_recover_part;

static int recover_info( xfs_in* in, uint16_t inode_size, uint8_t const* data ) {
	size_t         start  = in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	e_recover_part e_part = RP_DATA;

	/* The theory is as follows:
	 * Most but very fragmented files are stored as ST_ARRAY, thus, in extents
	 * which are listed right here in the inode.
	 * Most but very long lists of extended attributes are stored as ST_LOCAL, thus
	 * right here in the inode.
	 * We go through the space left after the inode core and see whether we find
	 * extent information. These can be counted. 16 bytes of zeroes imply the end
	 * of a 1-n extent list. The next are then either extents to xattrs, or the
	 * attributes themselves. We try to read both and see whether either makes sense.
	 */
	size_t   strips         = ( inode_size - start ) / 16;
	uint64_t file_size      = 0; // <-- (ZEROED on delete)
	uint64_t file_blocks    = 0; // <-- (ZEROED on delete)
	uint32_t ext_used       = 0; // <-- (ZEROED on delete)
	uint32_t xattr_ext_used = 0;

	for ( size_t i = 0, offset = start       ;
	                ( RP_END != e_part ) && ( i < strips ) ;
	                ++i, offset += 16                  ) {

		uint8_t const* strip     = data + offset;
		bool           is_extent = false;

		// ------------------------------------------------------------
		// Check 1: If the strip is zeroed, we go forward
		// ------------------------------------------------------------
		bool is_zero = true;
		for ( size_t j = 0; is_zero && ( j < 16 ); ++j ) {
			if ( data[j] ) is_zero = false;
		}
		if ( is_zero ) {
			if ( RP_XATTR == e_part )
				e_part = RP_END;
			if ( RP_DATA == e_part )
				e_part = RP_GAP;
			continue;
		}

		// If we have data, but were in RP_GAP, we are in RP_XATTR now.
		if ( RP_GAP == e_part ) {
			e_part = RP_XATTR;
			in->xattr_off = ( offset - start ) / 8; // At least that one is easy.
		}

		// ------------------------------------------------------------
		// Check 2: The start block is inside the device?
		// ------------------------------------------------------------
		xfs_ex test_ex;
		xfs_read_ex( &test_ex, strip );
		if ( ( test_ex.block + test_ex.length ) < full_disk_blocks )
			is_extent = true; // Not necessarily the truth, yet, but a hint.

		// ------------------------------------------------------------
		// Check 3: See whether this might be an XATTR local block
		// ------------------------------------------------------------
		if ( RP_XATTR == e_part ) {
			xattr* xattr_test = unpack_xattr_data( strip, inode_size - offset );

			// xattr local data can have an offset of 8 bytes
			if ( NULL == xattr_test )
				xattr_test = unpack_xattr_data( strip + 8, inode_size - offset - 8 );

			// Okay, but now it counts!
			if ( xattr_test ) {
				// This is obviously just fine like that.
				in->xattr_type_flg = ST_LOCAL;
				in->num_xattr_exts = 0;
				free_xattr_chain( xattr_test ); // Not needed here.
				e_part = RP_END; // Finished!
				continue;
			}
			// Okay, so this does not seem to be local data.
			// If the strip has already been confirmed as an extent, the case is clear
			if ( is_extent )
				xattr_ext_used++;
			continue; // No matter what, we are done here.
		} // End of checking for XATTR

		// ------------------------------------------------------------
		// Check 4: If we have a valid text extent and are in ST_DATA,
		//          we can (almost) safely assume that we know what's what
		// ------------------------------------------------------------
		if ( ( RP_DATA == e_part ) && is_extent ) {
			ext_used++;
			file_blocks += test_ex.length;
			file_size   += test_ex.length * sb_block_size;
			// Note: As we do not account for zero padding, file_size can
			//       only be assumed to be a maximum file size.
		}
	} // End of scanning strips
	if ( file_blocks && file_size ) {
		// *yay* !!! Success !!!
		in->file_blocks    = file_blocks;
		in->file_size      = file_size;
		in->ext_used       = ext_used;
		in->num_xattr_exts = xattr_ext_used;
		return 0;
	}

	// *meh*
	return -1;
} // end of recover_info()


// Returns the root of an unpacked xattr chain, or NULL if no chain was found
xattr* unpack_xattr_data( uint8_t const* data, size_t data_len ) {
	if ( NULL == data )
		return NULL; // nothing to do

	uint16_t xattr_size  = get_flip16u( data, 0 ); // The length includes the 4 bytes header
	if ( ( 0 == xattr_size ) || ( xattr_size > data_len ) )
		// This is not xattr local data
		return NULL;

	uint8_t  xattr_count = data[2];
	uint8_t  xattr_padd  = data[3];
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
	if ( NULL == in ) {
		log_critical( "%s", "BUG: Called without 'in' pointer!" );
		return;
	}
	if ( NULL == *in )
		// Simply nothing to do
		return;

	// shortcut
	xfs_in* lin = *in; // [l]ocal in

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

	if ( lin->d_loc_data ) {
		FREE_PTR( lin->d_loc_data );
	}

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

	lin->xattr_root = free_xattr_chain( lin->xattr_root );

	FREE_PTR( *in );
}


int xfs_read_in( xfs_in* in, xfs_sb const* sb, uint8_t const* data ) {
	if ( NULL == in ) {
		log_critical( "%s", "BUG: Called without 'in' pointer!" );
		return -1;
	}
	if ( NULL == sb ) {
		log_critical( "%s", "BUG: Called without 'sb' pointer!" );
		return -1;
	}
	if ( NULL == data ) {
		log_critical( "%s", "BUG: Called without 'data' pointer!" );
		return -1;
	}

	// Copy and check magic first
	memcpy( in->magic, data, 2 );
	if ( strncmp( sb->magic, XFS_IN_MAGIC, 2 ) ) {
		log_critical( "Wrong magic: 0x%02x%02x instead of 0x%02x%02x",
		              in->magic[0],    in->magic[1], XFS_IN_MAGIC[0], XFS_IN_MAGIC[1] );
		return -1;
	}

	// As the magic is correct, check whether this is a deleted inode
	if (!is_deleted_inode(data))
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
	in->type_mode       = 0; // <-- (ZEROED on delete)
	in->data_fork_type  = 2; // <-- (FORCED 2 on delete)
	in->num_links_v1    = 0; // <-- (ZEROED on delete)
	in->num_links_v2    = 0; // <-- (ZEROED on delete)
	in->file_size       = 0; // <-- (ZEROED on delete)
	in->file_blocks     = 0; // <-- (ZEROED on delete)
	in->ext_used        = 0; // <-- (ZEROED on delete)
	in->num_xattr_exts  = 0; // <-- (ZEROED on delete)
	in->xattr_off       = 0; // <-- (ZEROED on delete)
	in->xattr_type_flg  = 2; // <-- (FORCED 2 on delete)

	// Try to recover data fork type, extents used/B-Tree root
	// and where the extended attributes start, if they are local.
	if ( -1 == recover_info( in, sb->inode_size, data ) )
		// Completely fubar!
		return -1;

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
	in->attr_changes    = get_flip64u( data, 104 );
	in->last_log_seq    = get_flip64u( data, 112 );
	in->ext_flags       = get_flip64u( data, 120 );
	in->cow_ext_size    = get_flip32u( data, 128 );
	in->padding[12]     = get_flip8u(  data, 132 );
	in->btime_ep        = get_flip32u( data, 144 );
	in->btime_ns        = get_flip32u( data, 148 );

	// Handle file storage (local, extents or btree)
	if ( -1 == build_data_map( in, sb->inode_size, data ) )
		return -1; // Already told what's wrong

	// Handle xattr storage (local, extents or btree)
	build_xattr_map( in, sb->inode_size, data );
	// No check here. xattrs aren't that mission critical!

	// We here? Fine!

	return 0;
}
