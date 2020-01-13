#ifndef PWX_XFS_UNDELETE_SRC_EXTENT_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_EXTENT_H_INCLUDED 1
#pragma once


/// @brief each extent information is 16 bytes packed, not-aligned.
typedef struct _xfs_ex {
	uint8_t  is_prealloc; //!< Bit    1       : ( 1) Flag – Set if extent is preallocated but not yet written, zero otherwise
	uint64_t offset;      //!< Bits   2 -  55 : (54) Logical offset from the start of the file
	uint64_t block;       //!< Bits  56 - 107 : (52) Absolute block address of the start of the extent
	uint32_t length;      //!< Bits 108 - 128 : (21) Number of blocks in the extent
	struct _xfs_ex* next; //!< Pointer to the next extent following this
} xfs_ex_t;


/** @brief unpack a 16 bytes extent into its components
  * @param[out] ex    The xfs_ex structure to fill
  * @param[in]  data  The data to read
  * @return 0 on success, -1 one otherwise.
**/
int xfs_read_ex( xfs_ex_t* ex, uint8_t const* data );


#endif // PWX_XFS_UNDELETE_SRC_EXTENT_H_INCLUDED
