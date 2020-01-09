#ifndef PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED 1
#pragma once


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Forwarding to circumvent circular dependency
struct _xfs_entry;


/// @brief Representation of on directory
typedef struct _xfs_dir {
	size_t             dir_size;       //!< Size of the directory, also of its data block if short form
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
	char const*        name;    //!< Name of the entry or NULL if unknown
	struct _xfs_entry* next;    //!< Next sibling with the same parent
	struct _xfs_dir*   parent;  //!< parent directory this entry is part of
	struct _xfs_dir*   sub;     //!< If this entry is a directory, `sub` points to its structure
	e_file_type        type;    //!< The file type as noted in the entry
} xfs_entry_t;


/** @brief Clear and free all listed entries ; non recursively
  *
  * This only frees the direct entries in @a dir. If you need to
  * free a whole tree, use xfs_free_dir_recursive().
  *
  * Note: The given xfs_dir_t structure is not freed, you have to
  *       do this yourself if needed.
  *
  * @param[in,out] dir  Pointer to the directory to free
**/
void xfs_free_dir( xfs_dir_t* dir );


/** @brief Clear and free all listed entries recursively
  *
  * This frees the whole tree from @a dir down. If you need to
  * free one xfs_dir_t and its entries only, use xfs_free_dir_().
  *
  * Note: The given xfs_dir_t structure is not freed, you have to
  *       do this yourself if needed.
  *
  * @param[in,out] dir  Pointer to the directory to free
**/
void xfs_free_dir_recursive( xfs_dir_t* dir );


/** @brief interpret @a data as a short form packed directory.
  *
  * This will fully unpack the directory, but will *not* follow the files and
  * directories listed in there. This means that all xfs_entry_t::file and
  * xfs_entry_t::sub pointers will be NULL.
  *
  * @param[out] dir  Pointer to the xfs_dir_t instance to fill
  * @param[in] data  The data to unpack
  * @param[in] log_error  If set to true, issue an error message if any check fails
  * @return 0 on success, -1 on failure.
**/
int xfs_read_packed_dir( xfs_dir_t* dir, uint8_t const* data, bool log_error );


#endif // PWX_XFS_UNDELETE_SRC_XFS_DIR_H_INCLUDED
