#ifndef PWX_XFS_UNDELETE_SRC_FORENSICS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_FORENSICS_H_INCLUDED 1
#pragma once


#include "xfs_in.h"


#include <stddef.h>
#include <stdint.h>


/** @brief Check whether @a data points to a deleted inode
  *
  * @param[in] data  pointer to the data (minimum 84 bytes!)
  * @return true if this is a deleted inode, false if it is anything else
**/
int is_deleted_inode(uint8_t const* data);


/** @brief Check whether @a data points to a directory block (or inode)
  *
  * @param[in] data  pointer to the data (minimum 4 bytes!)
  * @return true if this is a directory block or inode, false if it is anything else
**/
int is_directory_block(uint8_t const* data);


/** @brief Check whether @a data points to a valid inode structure.
  *
  * This function does not do a full forensic analysis, it only checks the
  * most prominent values that must exist in both deleted and directory inodes.
  *
  * @param[in] sb    Pointer to the xfs_sb structure the @a data belongs to
  * @param[in] data  Pointer to the data to check
  * @return true if this looks legit, false otherwise.
**/
bool is_valid_inode( xfs_sb_t const* sb, uint8_t const* data );


/** @brief Try to recover information about an deleted inode
  *
  * @param[in,out] in  The inode structure to use and complete
  * @param[in] inode_size  Size of the inode
  * @param[in] data  Pointer to the data to analyze, must have at least @a inode_size bytes
  * @param[in] fd  File descriptor to read from the source device
  * @return 0 on success, -1 on failure.
**/
int restore_inode( xfs_in_t* in, uint16_t inode_size, uint8_t const* data, int fd );


#endif // PWX_XFS_UNDELETE_SRC_FORENSICS_H_INCLUDED
