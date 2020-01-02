/*******************************************************************************
 * xfs_dir.c : Functions for handling xfs_dir_t instances
 ******************************************************************************/


#include "common.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_in.h"
#include "xfs_dir.h"


#include <ctype.h>
#include <string.h>


xfs_entry_t* xfs_free_entry(xfs_entry_t* entry) {
	if (entry) {
		entry->address = 0;
		entry->file    = NULL;
		FREE_PTR(entry->name);
		entry->next    = NULL;
		entry->parent  = NULL;
		entry->sub     = NULL;
		entry->type    = FT_INVALID;
	}
	return entry;
}


static char const* get_safe_name( char const name[256] ) {
	static char safe_name[256] = { 0x0 };
	bool        name_end       = false;

	memset(safe_name, 0, 256);

	for (size_t i = 0; !name_end && (i < 255); ++i) {
		if ( isprint(name[i]) )
			safe_name[i] = name[i];
		else if (name[i])
			safe_name[i] = '?';
		else
			name_end = true; // NULL byte?
	}

	return safe_name;
}


int xfs_init_packed_dir(xfs_dir_t* dir, uint8_t const* data) {
	if ( NULL == dir ) {
		log_critical("%s", "BUG! Called without dir!");
		return -1;
	}
	if ( NULL == data ) {
		log_critical("%s", "BUG! Called without data!");
		return -1;
	}

	dir->entry_count    = get_flip8u(data, 0);
	dir->entries_64bit  = get_flip8u(data, 1);
	dir->parent_address = dir->entries_64bit ? get_flip64u(data, 2) : get_flip32u(data, 2);
	dir->parent         = NULL;
	dir->root           = NULL;

	// Check 1: We can't have more 64bit entries than total entries
	// --------------------------------------------------------------------
	if ( dir->entries_64bit > dir->entry_count ) {
		log_error("Can not have %hhu/%hhu 64bit entries!", dir->entries_64bit, dir->entry_count);
		return -1;
	}

	// Check 2: The parent address must lie on the source drive
	// --------------------------------------------------------------------
	if ( (0 == dir->parent_address) || (dir->parent_address > full_disk_size) ) {
		log_error("Invalid parent inode address at 0x08x!", dir->parent_address);
		return -1;
	}

	return 0;
}


int xfs_read_packed_dir_entry(xfs_entry_t* entry, xfs_dir_t* dir, uint8_t const* data) {
	static char name_buf[256];

	if ( NULL == entry ) {
		log_critical("%s", "BUG! Called without entry!");
		return -1;
	}
	if ( NULL == dir ) {
		log_critical("%s", "BUG! Called without dir!");
		return -1;
	}
	if ( NULL == data ) {
		log_critical("%s", "BUG! Called without data!");
		return -1;
	}

	// Do some early initialization. Maybe none will be filled later.
	entry->address = 0;
	entry->file    = NULL;
	entry->name    = NULL;
	entry->next    = NULL;
	entry->parent  = dir;
	entry->sub     = NULL;
	entry->type    = FT_INVALID;


	// Check 1: The file name is always a good hint.
	// --------------------------------------------------------------------
	bool name_is_valid = true;
	uint8_t name_len = get_flip8u(data, 0);

	memset(name_buf, 0, 256);
	memcpy(name_buf, data + 2, name_len);

	for (size_t i = 0; name_is_valid && (i < name_len); ++i) {
		if ( !isprint(name_buf[i]) )
			name_is_valid = false;
	}

	if (!name_is_valid) {
		log_error("The file name '%s' contains invalid characters!", get_safe_name(name_buf));
		return 1;
	}

	// Well, copy it then...
	entry->name = strdup(name_buf);

	// Check 2: The file type, the byte after the name
	// --------------------------------------------------------------------
	uint8_t ftype_num = get_flip8u(data, name_len + 2 );
	entry->type = get_file_type( ftype_num );
	if ( FT_INVALID == entry->type ) {
		log_error("The file type 0x%02x is invalid!", ftype_num);
		return -1;
	}

	// Check 3: The inode address, it must be on the disk!
	// --------------------------------------------------------------------
	entry->address    = dir->entries_64bit ? get_flip64u(data, name_len + 3) : get_flip32u(data, name_len + 3);
	uint32_t del_part = dir->entries_64bit
	                  ? (uint32_t)(entry->address & 0xffffffff00000000 >> 32)
	                  : (uint32_t)(entry->address & 0x00000000ffffffff);
	if ( 0xffff != (del_part & 0xffff) ) {
		// Okay, this is not marked as deleted. Let's check the address
		if ( (0 == entry->address) || (entry->address > full_disk_size) ) {
			log_error("Entry address 0x08x is invalid!", entry->address);
			return -1;
		}
	}
	// Note: if the inode is deleted, the first 4 bytes have been overwritten.
	//       There is nothing we can check right now.

	return 0;
}


