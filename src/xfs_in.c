/*******************************************************************************
 * xfs_in.c : Functions to work with inode representations of struct xfs_in
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"


#include <string.h>


#define DATA_START_V1 0x64
#define DATA_START_V3 0xB0


static int build_data_map( xfs_in* in, uint32_t inode_size, uint8_t const* data ) {
	size_t start = in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	size_t end   = in->xattr_off ? start + (in->xattr_off * 8) : inode_size;

	if (ST_DEV == in->data_fork_type) {
		log_error("Special device data forks (0x%02x) are not supported."
		          " Ignoring inode.", in->data_fork_type);
		return -1;
	} else if (ST_LOCAL == in->data_fork_type) {
		// Check whether the data part is in bounds
		size_t last = start + in->file_size - 1;
		if ( last >= end ) {
			log_error( "Local data block ends out of bounds at byte %zu/%zu",
				   last, end );
			return -1;
		}

		// Create the buffer
		in->d_loc_data = malloc(in->file_size);
		if (NULL == in->d_loc_data) {
			log_critical( "Unable to allocate %zu bytes for local data block!",
				      in->file_size );
			return -1;
		}

		memcpy(in->d_loc_data, data + start, in->file_size);
	} else if (ST_EXTENTS == in->data_fork_type) {
		xfs_ex* curr  = NULL;
		xfs_ex* next  = NULL;
		size_t  first = 0; // first byte to read;
		size_t  last  = 0; // last byte to read

		for ( size_t i = 0; i < in->ext_used; ++i ) {
			// First check the end of the reading. Don't overshoot!
			first = start + (16 * i);
			last  = first + 15;
			if ( last >= end ) {
				log_error( "Data extent %zu ends out of bounds at byte %zu/%zu",
				           i, last, end );
				return -1;
			}

			// Then allocate another extent structure
			next = calloc(1, sizeof(xfs_ex));
			if (NULL == next) {
				log_critical("Unable to allocate %lu bytes for data extent!",
				             sizeof(xfs_ex));
				return -1;
			}

			// Can it be read? (Should. Only errors are bugs if either is NULL)
			if ( -1 == xfs_read_ex(next, data + first) )
				return -1;

			// Set root or extent chain
			if (curr)
				curr->next = next;
			else
				in->d_ext_root = next;
			curr = next;
			next = NULL;
		}
	} else if (ST_BTREE == in->data_fork_type) {
	} else {
		log_error("Ignoring inode with unknown data fork type 0x%02x!",
		          in->data_fork_type);
		return -1;
	}

	return 0;
}


static void build_xattr_map( xfs_in* in, uint32_t inode_size, uint8_t const* data ) {
	size_t start = (in->xattr_off * 8) + in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	size_t end   = inode_size;

	if (ST_DEV == in->xattr_type_flg) {
		log_error("Special device xattr forks (0x%02x) are not supported.", in->xattr_type_flg);
		log_info("%s", " ==> Ignoring extended attributes for this inode!");
		return;
	} else if (ST_LOCAL == in->xattr_type_flg) {
		// Let's get the size of the attributes, first
		uint16_t xattr_size = get_flip16u(data, start); // The length includes the 4 bytes header

		// Check whether the yattr part is in bounds
		size_t last = start + xattr_size - 1;
		if ( last >= end ) {
			log_error( "Local xattr block ends out of bounds at byte %zu/%zu", last, end );
			log_info("%s", " ==> Ignoring extended attributes for this inode!");
			return;
		}

		// Create the buffer
		in->x_loc_data = malloc(xattr_size);
		if (NULL == in->d_loc_data) {
			log_error( "Unable to allocate %zu bytes for local xattr block!", xattr_size );
			log_info("%s", " ==> Ignoring extended attributes for this inode!");
			return;
		}

		memcpy(in->x_loc_data, data + start, xattr_size);
		unpack_xattr_data(in);
		// No check here. xattrs aren't that mission critical!
	} else if (ST_EXTENTS == in->xattr_type_flg) {
		xfs_ex* curr  = NULL;
		xfs_ex* next  = NULL;
		size_t  first = 0; // first byte to read;
		size_t  last  = 0; // last byte to read

		for ( size_t i = 0; i < in->ext_used; ++i ) {
			// First check the end of the reading. Don't overshoot!
			first = start + (16 * i);
			last  = first + 15;
			if ( last >= end ) {
				log_error( "xattr extent %zu ends out of bounds at byte %zu/%zu",
				           i, last, end );
				log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
				return;
			}

			// Then allocate another extent structure
			next = calloc(1, sizeof(xfs_ex));
			if (NULL == next) {
				log_critical("Unable to allocate %lu bytes for xattr extent!", sizeof(xfs_ex));
				log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
				return;
			}

			// Can it be read? (Should. Only errors are bugs if either is NULL)
			if ( -1 == xfs_read_ex(next, data + first) ) {
				log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
				return;
			}

			// Set root or extent chain
			if (curr)
				curr->next = next;
			else
				in->x_ext_root = next;
			curr = next;
			next = NULL;
		}
	} else if (ST_BTREE == in->xattr_type_flg) {
		/* Can they even exist? */
		log_error("Special device xattr forks (0x%02x) are not supported.", in->xattr_type_flg);
		log_info("%s", " ==> Ignoring extended attributes for this inode!");
		return;
	} else {
		log_error("Ignoring xattrs with unknown fork type 0x%02x!", in->xattr_type_flg);
		log_info("%s", " ==> Ignoring extended attributes for this inode!");
		return;
	}
}

static int set_file_mode( xfs_in* in, uint8_t const* data ) {
	uint16_t mode_bits = *((uint16_t*)data); // No flip needed
	in->file_modes[0] = (uint8_t)((mode_bits & 0x0e00) >> 9); // 0b0000 111 000 000 000
	in->file_modes[1] = (uint8_t)((mode_bits & 0x01c0) >> 6); // 0b0000 000 111 000 000
	in->file_modes[2] = (uint8_t)((mode_bits & 0x0038) >> 3); // 0b0000 000 000 111 000
	in->file_modes[3] = (uint8_t)((mode_bits & 0x0007)     ); // 0b0000 000 000 000 111

	if ( 0 == (in->file_modes[1] | in->file_modes[2] | in->file_modes[3]) ) {
		log_error("Filemode is %02o%02o%02o%02o ... ignoring inode!",
			in->file_modes[0], in->file_modes[1], in->file_modes[2], in->file_modes[3]);
		return -1;
	}

	return -1;
}


static int set_file_type( xfs_in* in, uint8_t const* data ) {
	uint8_t type_nibble = ( data[0] & 0xf0 ) >> 4;

	switch( type_nibble ) {
		case FT_FIFO:
			log_warning( "%s", "Ignoring FIFO inode..." );
			break;
		case FT_CSD:
			log_warning( "%s", "Ignoring Character special device..." );
			break;
		case FT_DIR:
			log_warning( "%s", "Ignoring Directory..." );
			break;
		case FT_BSD:
			log_warning( "%s", "Ignoring Block special device..." );
			break;
		case FT_FILE:
			in->file_type = FT_FILE;
			return 0; // All clear, that's what we want!
		case FT_SLN:
			log_warning( "%s", "Ignoring  Symlink..." );
			break;
		case FT_SCK:
			log_warning( "%s", "Ignoring  Socket..." );
			break;
		default:
			log_error( "Ignoring invalid file type 0x%0x!", type_nibble );
	}
	return -1;
}


void unpack_xattr_data( xfs_in* in ) {
	if ( NULL == in->x_loc_data )
		return; // nothing to do

	uint8_t const* data  = in->x_loc_data; // shortcut
	uint16_t xattr_size  = get_flip16u(data, 0); // The length includes the 4 bytes header
	uint8_t  xattr_count = data[2];
	uint8_t  xattr_padd  = data[3];
	size_t   offset      = 4; // Start after header
	xattr*   curr        = NULL;
	xattr*   next        = NULL;

	for (size_t i = 0; i < xattr_count; ++i) {
		uint8_t name_len = data[offset];
		uint8_t val_len  = data[offset + 1];
		size_t  end_byte = offset + 2 + name_len + xattr_padd + val_len - 1;

		// Make sure we are not out of bounds!
		if (end_byte >= xattr_size) {
			log_error( "next xattr entry ends out of bounds at byte %zu/%zu", end_byte, xattr_size );
			log_info("%s", " ==> Ignoring remaining xattr entries in this block!");
			return;
		}

		// Allocate buffers for the name and value
		// But add 1 byte, as xattr names are not NULL terminated,
		// and values might, too, come without termination!
		char* name = calloc(name_len + 1, 1);
		if (NULL == name) {
			log_critical("Unable to allocate %lu bytes for xattr name!", name_len + 1);
			log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
			return;
		}
		char* value = calloc(val_len + 1, 1);
		if (NULL == name) {
			log_critical("Unable to allocate %lu bytes for xattr value!", val_len + 1);
			log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
			FREE_PTR(name);
			return;
		}
		memcpy(name,  data + offset + 3, name_len);
		memcpy(value, data + offset + 3 + name_len + xattr_padd, val_len);

		// Allocate the new xattr entry
		next = calloc(1, sizeof(xattr));
		if (NULL == next) {
			log_critical("Unable to allocate %zu bytes for xattr entry!", sizeof(xattr));
			log_info("%s", " ==> Ignoring remaining extended attributes for this inode!");
			FREE_PTR(name);
			FREE_PTR(value);
			return;
		}

		next->flags = data[offset + 2];
		next->name  = TAKE_PTR(name);
		next->value = TAKE_PTR(value);

		if (curr)
			curr->next = next;
		else
			in->xattr_root = next;
		curr   = next;
		next   = NULL;
		offset = end_byte + 1;
	}

}


void xfs_free_in(xfs_in** in) {
	if ( NULL == in ) {
		log_critical( "%s", "BUG: Called without 'in' pointer!" );
		return;
	}
	if ( NULL == *in )
		// Simply nothing to do
		return;

	// shortcut
	xfs_in* lin = *in; // [l]ocal in

	if (lin->d_ext_root) {
		xfs_ex* curr    = lin->d_ext_root;
		xfs_ex* next    = curr->next;
		lin->d_ext_root = NULL;
		while (curr) {
			free(curr);
			curr = next;
			if (curr)
				next = curr->next;
		}
	}

	if (lin->d_loc_data) {
		FREE_PTR(lin->d_loc_data);
	}

	if (lin->x_ext_root) {
		xfs_ex* curr    = lin->x_ext_root;
		xfs_ex* next    = curr->next;
		lin->x_ext_root = NULL;
		while (curr) {
			free(curr);
			curr = next;
			if (curr)
				next = curr->next;
		}
	}

	if (lin->x_loc_data) {
		FREE_PTR(lin->x_loc_data);
	}

	if (lin->xattr_root) {
		xattr* curr     = lin->xattr_root;
		xattr* next     = curr->next;
		lin->xattr_root = NULL;
		while (curr) {
			FREE_PTR(curr->name);
			FREE_PTR(curr->value);
			free(curr);
			curr = next;
			if (curr)
				next = curr->next;
		}
	}

	FREE_PTR(*in);
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

	// We need the version first, as some features need version v3
	in->version = data[4];

	// Copy the inode number, we might need it for error messages
	in->inode_id = get_flip64u( data, 152 );


	// Copy CRC32 and UUID, then check the UUID
	memcpy( in->in_crc32, data + 100,  4 );
	memcpy( in->sb_UUID,  data + 160, 16 );
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

	// Get file type and permission modes
	if ( -1 == set_file_type( in, &data[2] ) )
		return -1; // Has already cried about it.
	if ( -1 == set_file_mode( in, &data[2] ) )
		return -1; // dito

	// Now just copy/flip the rest
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
	in->xattr_type_flg  = get_flip8u(  data,  83 );
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

	// Handle file storage (local, extents or btree)
	if ( -1 == build_data_map(in, sb->inode_size, data) )
		return -1; // Already told what's wrong

	// Handle xattr storage (local, extents or btree)
	build_xattr_map(in, sb->inode_size, data);
	// No check here. xattrs aren't that mission critical!

	// We here? Fine!

	return 0;
}
