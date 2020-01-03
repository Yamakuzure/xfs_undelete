#ifndef PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED 1
#pragma once


#include "xfs_in.h"


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Forwarding to circumvent circular dependency
struct _xfs_entry;


/// @brief Representation of on directory
typedef struct _xfs_dir {
	uint8_t            entries_64bit;  //!< Number of entries needing 64bit addressing
	uint8_t            entry_count;    //!< Total number of directory entries
	struct _xfs_dir*   parent;         //!< Parent structure, if we've found it.
	uint64_t           parent_address; //!< Absolute inode address of the parent
	struct _xfs_entry* root;           //!< Pointer to the first child in this directory
} xfs_dir_t;


/** @brief Representation of one directory entry
  *
  * As we try to reconstruct the directory structure first, it can happen
  * that we have to add entries before the corresponding inode is discovered.
  * We do not want to "read ahead" of the scanner, so both address and file
  * type are stored in here, too.
  * Also the address of deleted entries have their first four bytes overwritten.
  * If a 32bit address is stored in a 64bit space, this isn't much of a problem.
  * But if it is longer than 32bit, or if all entries in this directory have
  * been stored in 32bit, the address is no longer complete, or gone in the
  * latter case.
**/
typedef struct _xfs_entry {
	uint64_t           address; //!< Absolute inode address of the sub dir or file
	struct _xfs_in*    file;    //!< If this entry is a file, `file` points to its inode
	char const*        name;    //!< Name of the entry or NULL if unknown
	struct _xfs_entry* next;    //!< Next sibling with the same parent
	struct _xfs_dir*   parent;  //!< parent directory this entry is part of
	struct _xfs_dir*   sub;     //!< If this entry is a directory, `sub` points to its structure
	e_file_type        type;    //!< The file type as noted in the entry
} xfs_entry_t;


/** @brief This has to be called before destroying xfs_entry_t instances!
  *
  * This only clears the "inside". If the instance is malloc'd, you
  * still have to free() it yourself!
  *
  * @param[in,out] entry  Pointer to the entry to free
**/
xfs_entry_t* xfs_free_entry( xfs_entry_t* entry );


/** @brief interpret @a data as a short form packed directory header
  *
  * @param[out] dir  Pointer to the xfs_dir_t instance to fill
  * @param[in] data  The data to unpack
  * @return 0 on success, -1 on failure.
**/
int xfs_init_packed_dir( xfs_dir_t* dir, uint8_t const* data );


/** @brief interpret @a data as a short form packed directory entry.
  *
  * @param[out] entry  Pointer to the xfs_entry_t instance to fill
  * @param[in] dir  Pointer to the xfs_dir_t instance this is a child of
  * @param[in] data  The data to unpack
  * @return 0 on success, -1 on failure.
**/
int xfs_read_packed_dir_entry( xfs_entry_t* entry, xfs_dir_t* dir, uint8_t const* data );

#endif // PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED
