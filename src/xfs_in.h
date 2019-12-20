#ifndef PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED 1
#pragma once


#include "xfs_ex.h"
#include "xfs_sb.h"


#include <stdint.h>


/// @brief enum describing the upper nibble of the file type and permissions field
typedef enum _file_type {
	FT_FIFO = 0x1,
	FT_CSD  = 0x2, //!< Character special device
	FT_DIR  = 0x4, //!< Directory
	FT_BSD  = 0x6, //!< Block special device
	FT_FILE = 0x8, //!< Regular file
	FT_SLN  = 0xa, //!< Symlink
	FT_SCK  = 0xc  //!< Socket
} e_file_type;

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
} xattr;


/// @brief structure of an inode
typedef struct _xfs_in {
	char        magic[3];       //!< Bytes   0-  1 : Magic number
	e_file_type file_type;      //!<    Upper nibble of type_mode
	uint8_t     file_modes[4];  //!<    Lower 12 bits of type_mode, stored as SUGO
	uint8_t     version;        //!< Byte    4      : Version (v5 file system uses v3 inodes)
	uint8_t     data_fork_type; //!< Byte    5      : Data fork type flag
	uint16_t    num_links_v1;   //!< Bytes   6-  7 : v1 inode numlinks field (not used in v3)
	uint32_t    uid;            //!< Bytes   8- 11 : File owner UID
	uint32_t    gid;            //!< Bytes  12- 15 : File GID

	uint32_t    num_links_v2;   //!< Bytes  16- 19 : v2+ number of links
	uint16_t    project_id_lo;  //!< Bytes  20- 21 : Project ID (low)
	uint16_t    project_id_hi;  //!< Bytes  22- 23 : Project ID (high)
	uint16_t    inc_on_flush;   //!< Bytes  30- 31 : Increment on flush

	uint32_t    atime_ep;       //!< Bytes  32- 35 : atime epoch seconds
	uint32_t    atime_ns;       //!< Bytes  36- 39 : atime nanoseconds
	uint32_t    mtime_ep;       //!< Bytes  40- 43 : mtime epoch seconds
	uint32_t    mtime_ns;       //!< Bytes  44- 47 : mtime nanoseconds
	uint32_t    ctime_ep;       //!< Bytes  48- 51 : ctime epoch seconds
	uint32_t    ctime_ns;       //!< Bytes  52- 55 : ctime nanoseconds

	uint64_t    file_size;      //!< Bytes  56- 63 : File (data fork) size
	uint64_t    file_blocks;    //!< Bytes  64- 71 : Number of blocks in data fork
	uint32_t    ext_size_hint;  //!< Bytes  72- 75 : Extent size hint
	uint32_t    ext_used;       //!< Bytes  76- 79 : Number of data extents used

	uint16_t    num_xattr_exts; //!< Bytes  80- 81 : Number of extended attribute extents
	uint8_t     xattr_off;      //!< Byte   82     : Inode offset to xattr (8 byte multiples)
	uint8_t     xattr_type_flg; //!< Byte   83     : Extended attribute type flag
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

	/* Data and xattr information */
	xfs_ex*     d_ext_root;     //!< First data extent if used for file storage
	uint8_t*    d_loc_data;     //!< Local data if the file is stored inside the inode
	xfs_ex*     x_ext_root;     //!< First xattr extent if used for extended attributes
	uint8_t*    x_loc_data;     //!< Local xattr if the extended attributes are stored inside the inode
	xattr*      xattr_root;     //!< Root element of the xattr chain
} xfs_in;


/** @brief Unpack in->x_loc_data and extend in->xattr_root with the findings
  *
  * Note  : For extents unpacking simply load a block and put it in x_loc_data
  * Note 2: The function is void, because xattrs aren't mission ciritical. Error? Ignore the stuff!
  *
  * @param[in,out] in  pointer to the inode structure to work with
**/
void unpack_xattr_data( xfs_in* in );


/** @brief Destroy xfs_in structures with this function
  * @param[out] in pointer to the struct pointer of the inode structure to destroy. Sets *in to NULL.
**/
void xfs_free_in(xfs_in** in);


/** @brief read inode data from a data block
  *
  * There are no checks. The @a data block *must* have at least 176 bytes!
  *
  * @param[out] in    The xfs_in inode structure to fill
  * @param[in]  sb    Pointer to the superblock structure this @a data belongs to.
  * @param[in]  data  Pointer to the data block to interpret.
**/
int xfs_read_in(xfs_in* in, xfs_sb const* sb, uint8_t const* data);


#endif // PWX_XFS_UNDELETE_SRC_XFS_IN_H_INCLUDED
