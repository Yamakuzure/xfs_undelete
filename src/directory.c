/*******************************************************************************
 * xfs_dir.c : Functions for handling xfs_dir_t instances
 ******************************************************************************/


#include "common.h"
#include "directory.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"


#include <ctype.h>
#include <errno.h>
#include <string.h>


/// ========================================
/// === Static functions implementations ===
/// ========================================


/** @brief This has to be called before destroying xfs_entry_t instances!
  *
  * This only clears the "inside". If the instance is malloc'd, you
  * still have to free() it yourself!
  *
  * @param[in,out] entry  Pointer to the entry to free
**/
static void xfs_free_entry( xfs_entry_t* entry ) {
	RETURN_VOID_IF_NULL( entry );

	if ( entry ) {
		entry->address = 0;
		FREE_PTR( entry->name );
		entry->next    = NULL;
		entry->parent  = NULL;
		entry->sub     = NULL;
		entry->type    = FT_INVALID;
	}
}


/** @brief interpret @a data as a short form packed directory header
  *
  * @param[out] dir  Pointer to the xfs_dir_t instance to fill
  * @param[in] data  The data to unpack
  * @param[in] log_error  If set to true, issue an error message if any check fails
  * @return 0 on success, -1 on failure.
**/
static int xfs_init_packed_dir( xfs_dir_t* dir, uint8_t const* data, bool log_error ) {
	RETURN_INT_IF_NULL( dir );
	RETURN_INT_IF_NULL( data );

	dir->dir_size       = 0; // Not sure whether really a dir header, yet
	dir->entry_count    = get_flip8u( data, 0 );
	dir->entries_64bit  = get_flip8u( data, 1 );
	dir->parent_address = dir->entries_64bit ? get_flip64u( data, 2 ) : get_flip32u( data, 2 );
	dir->parent         = NULL;
	dir->root           = NULL;

	// Check 1: We can't have more 64bit entries than total entries
	// --------------------------------------------------------------------
	if ( dir->entries_64bit > dir->entry_count ) {
		if ( log_error )
			log_error( "Can not have %hhu/%hhu 64bit entries!\n"
			           DUMP_STRIP_FMT,
			           dir->entries_64bit, dir->entry_count,
			           DUMP_STRIP_DATA(0, data) );
		return -1;
	}

	// Check 2: The parent address must lie on the source drive
	// --------------------------------------------------------------------
	if ( dir->parent_address > full_disk_size ) {
		if ( log_error )
			log_error( "Invalid parent inode address at 0x%x/0x%x!\n"
			           DUMP_STRIP_FMT,
			           dir->parent_address, full_disk_size,
			           DUMP_STRIP_DATA(0, data) );
		return -1;
	}

	// As this seems legit, set the dir_size to the header size.
	dir->dir_size = dir->entries_64bit ? 10 : 6;

	return 0;
}


/** @brief interpret @a data as a short form packed directory entry.
  *
  * @param[out] entry  Pointer to the xfs_entry_t instance to fill
  * @param[in] dir  Pointer to the xfs_dir_t instance this is a child of
  * @param[in] data  The data to unpack
  * @param[in] log_error  If set to true, issue an error message if any check fails
  * @return 0 on success, -1 on failure.
**/
static int xfs_read_packed_dir_entry( xfs_entry_t* entry, xfs_dir_t* dir, uint8_t const* data, bool log_error ) {
	static char name_buf[256];

	RETURN_INT_IF_NULL( entry );
	RETURN_INT_IF_NULL( dir );
	RETURN_INT_IF_NULL( data );

	// Do some early initialization. Maybe none will be filled later.
	entry->address = 0;
	entry->name    = NULL;
	entry->next    = NULL;
	entry->parent  = dir;
	entry->sub     = NULL;
	entry->type    = FT_INVALID;


	// Check 1: The file name is always a good hint.
	// --------------------------------------------------------------------
	uint8_t name_len = get_flip8u( data, 0 );

	memset( name_buf, 0, 256 );
	memcpy( name_buf, data + 3, name_len );

	char const* safe_name = get_safe_name( name_buf, name_len );

	if ( strncmp( name_buf, safe_name, name_len ) ) {
		if ( log_error )
			log_error( "The file name '%s' contains invalid characters!\n"
			           DUMP_STRIP_FMT "\n" DUMP_STRIP_FMT,
			           safe_name,
			           DUMP_STRIP_DATA(0, data), DUMP_STRIP_DATA(16, data + 16) );
		return 1;
	}

	// Well, copy it then...
	entry->name = strdup( name_buf );

	// Check 2: The file type, the byte after the name
	// --------------------------------------------------------------------
	uint8_t ftype_num = get_flip8u( data, name_len + 3 );
	entry->type = get_file_type_from_dirent( ftype_num );
	if ( FT_INVALID == entry->type ) {
		if ( log_error )
			log_error( "The file type 0x%02x is invalid!\n"
			           DUMP_STRIP_FMT, ftype_num,
			           // Note: Not the +3, so we get back 3 chars
			           DUMP_STRIP_DATA(name_len, data + name_len) );
		return -1;
	}

	// Check 3: The inode address, it must be on the disk!
	// --------------------------------------------------------------------
	entry->address    = dir->entries_64bit
	                  ? get_flip64u( data, name_len + 4 )
	                  : get_flip32u( data, name_len + 4 );
	uint32_t del_part = dir->entries_64bit
	                  ? ( uint32_t )( entry->address & 0xffffffff00000000 >> 32 )
	                  : ( uint32_t )( entry->address & 0x00000000ffffffff );
	/* Note: The first four bytes are changed when an entry is deleted:
	 *       bytes 0-1 : 0xffff - Marks the entry as deleted
	 *       bytes 2-3 : Length of the free space.
	 * Therefore we check for == 0xffff on the first two bytes, only.
	 */
	if ( 0xffff != ( del_part & 0xffff ) ) {
		// Okay, this is not marked as deleted. Let's check the address
		if ( ( 0 == entry->address ) || ( entry->address > full_disk_size ) ) {
			if ( log_error )
				log_error( "Entry address 0x%x/0x%x is invalid!\n"
				           DUMP_STRIP_FMT, entry->address, full_disk_size,
				           // Also not the +3, like above
				           DUMP_STRIP_DATA(name_len, data + name_len) );
			return -1;
		}
	}
	// Note: if the inode is deleted, the first 4 bytes have been overwritten.
	//       There is nothing we can check right now.

	// Eventually record the entry size:
	dir->dir_size += name_len + ( dir->entries_64bit ? 12 : 8 );

	return 0;
}


/// ============================================
/// === Non-static functions implementations ===
/// ============================================


void xfs_free_dir( xfs_dir_t* dir ) {
	RETURN_VOID_IF_NULL( dir );

	xfs_entry_t* curr = dir->root;
	xfs_entry_t* next = curr ? curr->next : NULL;

	while ( curr ) {
		xfs_free_entry( curr );
		FREE_PTR( curr );
		curr = next;
		next = curr ? curr->next : NULL;
	}
}


void xfs_free_dir_recursive( xfs_dir_t* dir ) {
	RETURN_VOID_IF_NULL( dir );

	xfs_entry_t* curr = dir->root;
	xfs_entry_t* next = curr ? curr->next : NULL;

	while ( curr ) {
		if ( curr->sub ) {
			xfs_free_dir_recursive( curr->sub );
			FREE_PTR( curr->sub );
		}
		xfs_free_entry( curr );
		FREE_PTR( curr );
		curr = next;
		next = curr ? curr->next : NULL;
	}
}


int xfs_read_packed_dir( xfs_dir_t* dir, uint8_t const* data, bool log_error ) {
	RETURN_INT_IF_NULL( dir );
	RETURN_INT_IF_NULL( data );

	int res = xfs_init_packed_dir( dir, data, log_error );
	if ( -1 == res )
		// This is not a packed dir...
		return res;

	xfs_entry_t* curr = NULL;
	xfs_entry_t* next = NULL;

	for ( uint8_t i = 0 ; (0 == res) && (i < dir->entry_count) ; ++i ) {
		next = malloc( sizeof(xfs_entry_t) );
		if ( NULL == next ) {
			log_critical( "Unable to allocate %zu bytes for xfs_entry_t! %m [%d]",
			              sizeof(xfs_entry_t), errno );
			res = -1;
			continue;
		}

		res = xfs_read_packed_dir_entry( next, dir, data + dir->dir_size, log_error );
		if ( 0 == res ) {
			if ( curr )
				// This is a second+ entry
				curr->next = next;
			else
				// This is the first entry
				dir->root = next;
			curr = next;
		}
	} // End of looping through the directory entries

	return res;
}
