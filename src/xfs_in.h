#ifndef PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED 1
#pragma once


#include "file_type.h"
#include "xfs_dir.h"
#include "xfs_ex.h"
#include "xfs_sb.h"


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/// @brief enum how data forks and extended attributes are stored
typedef enum _store_type {
	ST_DEV     = 0, //!< Special device file (data type only)
	ST_LOCAL   = 1, //!< Data is resident ("local") in the inode
	ST_EXTENTS = 2, //!< Array of extent structures follows
	ST_BTREE   = 3  //!< B+Tree root follows
} e_store_type;


/// @brief structure of one extended attribute
typedef struct _xattr {
	uint8_t        flags;
	char*          name;
	char*          value;
	struct _xattr* next;
} xattr_t;


/// @brief structure of an inode
typedef struct _xfs_in {
	char        magic[3];       //!< Bytes   0-  1 : Magic number
	uint16_t    type_mode;      //!< Bytes   2-  3 : file type and permissions        (ZEROED on delete)
	uint8_t     version;        //!< Byte    4     : Version (v5 file system uses v3 inodes)
	uint8_t     data_fork_type; //!< Byte    5     : Data fork type flag              (FORCED 2 on delete)
	uint16_t    num_links_v1;   //!< Bytes   6-  7 : v1 inode numlinks field (not v3) (ZEROED on delete)
	uint32_t    uid;            //!< Bytes   8- 11 : File owner UID
	uint32_t    gid;            //!< Bytes  12- 15 : File GID

	uint32_t    num_links_v2;   //!< Bytes  16- 19 : v2+ number of links              (ZEROED on delete)
	uint16_t    project_id_lo;  //!< Bytes  20- 21 : Project ID (low)
	uint16_t    project_id_hi;  //!< Bytes  22- 23 : Project ID (high)
	uint16_t    inc_on_flush;   //!< Bytes  30- 31 : Increment on flush

	uint32_t    atime_ep;       //!< Bytes  32- 35 : atime epoch seconds
	uint32_t    atime_ns;       //!< Bytes  36- 39 : atime nanoseconds
	uint32_t    mtime_ep;       //!< Bytes  40- 43 : mtime epoch seconds
	uint32_t    mtime_ns;       //!< Bytes  44- 47 : mtime nanoseconds
	uint32_t    ctime_ep;       //!< Bytes  48- 51 : ctime epoch seconds
	uint32_t    ctime_ns;       //!< Bytes  52- 55 : ctime nanoseconds

	uint64_t    file_size;      //!< Bytes  56- 63 : File (data fork) size            (ZEROED on delete)
	uint64_t    file_blocks;    //!< Bytes  64- 71 : Number of blocks in data fork    (ZEROED on delete)
	uint32_t    ext_size_hint;  //!< Bytes  72- 75 : Extent size hint
	uint32_t    ext_used;       //!< Bytes  76- 79 : Number of data extents used      (ZEROED on delete)

	uint16_t    num_xattr_exts; //!< Bytes  80- 81 : Number of extended attribute extents
	uint8_t     xattr_off;      //!< Byte   82     : Offset to xattr (* 8 byte )      (ZEROED on delete)
	uint8_t     xattr_type_flg; //!< Byte   83     : Extended attribute type flag     (FORCED 2 on delete)
	uint32_t    DMAPI_evnt_flg; //!< Bytes  84- 87 : DMAPI event mask
	uint16_t    DMAPI_state;    //!< Bytes  88- 89 : DMAPI state
	uint32_t    flags;          //!< Bytes  90- 91 : Flags
	uint32_t    gen_number;     //!< Bytes  92- 95 : Generation number

	uint32_t    nxt_unlnkd_ptr; //!< Bytes  96- 99 : Next unlinked ptr (if inode unlinked) (-1 = NULL in XFS)

	/* v3 inodes (v5 file system) have the following fields */
	uint8_t     in_crc32[4];    //!< Bytes 100-103 : CRC32 checksum for inode
	uint64_t    attr_changes;   //!< Bytes 104-111 : Number of changes to attributes

	uint64_t    last_log_seq;   //!< Bytes 112-119 : Log sequence number of last update
	uint64_t    ext_flags;      //!< Bytes 120-127 : Extended flags

	uint32_t    cow_ext_size;   //!< Bytes 128-131 : Copy on write extent size hint
	uint8_t     padding[12];    //!< Bytes 132-143 : Padding for future use

	uint32_t    btime_ep;       //!< Bytes 144-147 : btime epoch seconds
	uint32_t    btime_ns;       //!< Bytes 148-151 : btime nanoseconds
	uint64_t    inode_id;       //!< Bytes 152-159 : inode number of this inode

	uint8_t     sb_UUID[16];    //!< Bytes 160-175 : UUID of the device, stored in the superblock

	/* Data, directory and xattr information */
	xfs_dir_t*  d_dir_root;     //!< Directory structure if used as local short form dir
	xfs_ex_t*   d_ext_root;     //!< First data extent if used for file storage
	uint8_t*    d_loc_data;     //!< Local data if the file is stored inside the inode
	xfs_ex_t*   x_ext_root;     //!< First xattr extent if used for extended attributes
	xattr_t*    xattr_root;     //!< Root element of the xattr chain

	/* Some helper values for internal use */
	uint32_t    ag_num;       //!< The allocation group number this inode belongs to
	size_t      block;        //!< The absolute block in which the inode resides
	e_file_type ftype;        //!< Detected file type
	bool        is_deleted;   //!< True if this is a deleted inode
	bool        is_directory; //!< True if this is sure to be a directory inode
	size_t      offset;       //!< Offset inside the block
	xfs_sb_t*   sb;           //!< pointer to the superblock for this allocation group
} xfs_in_t;


/** @brief Check whether the given @a data starts an xattr block
  *
  * @param[in] data  pointer to the data to check, must have at least 10 bytes
  * @param[in] data_size  The amount of bytes that can hold xattr values. Needed to check fond size values.
  * @param[out] x_size  If not NULL, this value is set to the size of the xattr local block, including header
  * @param[out] x_count If not Null, this value is set to the number of xattr entries
  * @param[out] padding  If not NULL, this value is set to the number of padding bytes
  * @param[in] log_error  If set to true, issue an error message if any check fails
  * @return true if this starts an xattr block, false otherwise
**/
bool is_xattr_head( uint8_t const* data, size_t data_size, uint16_t* x_size, uint8_t* x_count,
                    uint8_t* padding, bool log_error );


/** @brief Unpack xattr data and create an xattr chain from the findings
  *
  * @param[in] data  pointer to the data to analyze
  * @param[in] data_len size of the data to analyze
  * @param[in] log_error  If set to true, issue an error message if any check fails
  * @return A pointer to the root of an XATTR chain, or NULL on failure
**/
xattr_t* unpack_xattr_data( uint8_t const* data, size_t data_len, bool log_error );


/** @brief Destroy xfs_in structures with this function
  * @param[out] in pointer to the struct pointer of the inode structure to destroy. Sets *in to NULL.
**/
void xfs_free_in( xfs_in_t** in );


/** @brief read inode data from a data block
  *
  * There are no checks. The @a data block must have sb->inode_size bytes. Your responsibility!
  *
  * @param[out] in    The xfs_in inode structure to fill
  * @param[in]  data  Pointer to the data block to interpret.
  * @param[in]  fd    File Descriptor for reading from the source device.
  * @return 0 on success, -1 on error
**/
int xfs_read_in( xfs_in_t* in, uint8_t const* data, int fd );


#endif // PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED
