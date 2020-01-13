#ifndef PWX_XFS_UNDELETE_SRC_SUPERBLOCK_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_SUPERBLOCK_H_INCLUDED 1
#pragma once


#include <stdint.h>


/// @brief structure that represents one XFS SuperBlock
typedef struct _xfs_sb {
	char     magic[5];         //!< Bytes 0-3      Magic Number
	uint32_t block_size;       //!< Bytes 4-7      Block Size (in bytes)
	uint64_t total_blocks;     //!< Bytes 8-15     Total blocks in file system

	uint64_t rt_block_count;   //!< Bytes 16-23    Num blocks in real-time device
	uint64_t rt_extent_count;  //!< Bytes 24-31    Num extents in real-time device

	uint8_t  UUID[16];         //!< Bytes 32-47    UUID (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)

	uint64_t journal_start;    //!< Bytes 48-55    First block of journal
	uint64_t root_inode;       //!< Bytes 56-63    Root directory's inode

	uint64_t rt_extent_inode;  //!< Bytes 64-71    Real-time extents bitmap inode
	uint64_t rt_summary_inode; //!< Bytes 72-79    Real-time bitmap summary inode

	uint32_t rt_extent_size;   //!< Bytes 80-83    Real-time extent size (in blocks)
	uint32_t ag_size;          //!< Bytes 84-87    AG size (in blocks)
	uint32_t ag_count;         //!< Bytes 88-91    Number of AGs
	uint32_t rt_bitmap_count;  //!< Bytes 92-95    Num of real-time bitmap blocks

	uint32_t journal_count;    //!< Bytes 96-99    Num of journal blocks
	uint16_t fs_version;       //!< Bytes 100-101  File system version and flags (low nibble is version)
	uint16_t sector_size;      //!< Bytes 102-103  Sector size
	uint16_t inode_size;       //!< Bytes 104-105  Inode size
	uint16_t inodes_per_block; //!< Bytes 106-107  Inodes/block
	char     fs_name[13];      //!< Bytes 108-119  File system name (if set)
	uint8_t  log2_block_size;  //!< Byte  120      log2(block size)
	uint8_t  log2_sector_size; //!< Byte  121      log2(sector size)
	uint8_t  log2_inode_size;  //!< Byte  122      log2(inode size)
	uint8_t  log2_inode_block; //!< Byte  123      log2(inode/block)
	uint8_t  log2_ag_size;     //!< Byte  124      log2(AG size) rounded up
	uint8_t  log2_rt_extents;  //!< Byte  125      log2(real-time extents)
	uint8_t  fs_created_flag;  //!< Byte  126      File system being created flag
	uint8_t  max_inode_perc;   //!< Byte  127      Max inode percentage

	uint64_t allocated_inodes; //!< Bytes 128-135  Number of allocated inodes
	uint64_t free_inodes;      //!< Bytes 136-143  Number of free inodes

	uint64_t free_blocks;      //!< Bytes 144-151  Number of free blocks
	uint64_t free_rt_Extents;  //!< Bytes 152-159  Number of free real-time extents

	int64_t  user_quota_inode; //!< Bytes 160-167  User quota inode
	int64_t  group_quota_inode;//!< Bytes 168-175  Group quota inode

	uint16_t quota_flags;      //!< Bytes 176-177  Quota flags
	uint8_t  misc_flags;       //!< Byte  178      Misc flags
	uint8_t  reserved_1;       //!< Byte  179      Reserved (Must be zero)
	uint32_t inode_alignment;  //!< Bytes 180-183  Inode alignment (in blocks)
	uint32_t raid_unit;        //!< Bytes 184-187  RAID unit (in blocks)
	uint32_t raid_stripe;      //!< Bytes 188-191  RAID stripe (in blocks)

	uint8_t  log2_dir_blk_ag;  //!< Bytes 192      log2(dir blk allocation granularity)
	uint8_t  log2_ext_jrnl_ss; //!< Bytes 193      log2(sector size of external journal device)
	uint16_t ext_jrnl_ss;      //!< Bytes 194-195  Sector size of external journal device
	uint32_t ext_jrnl_unit_s;  //!< Bytes 196-199  Stripe/unit size of external journal device
	uint32_t add_flags;        //!< Bytes 200-203  Additional flags
	uint32_t add_flags_repeat; //!< Bytes 204-207  Repeat additional flags (for alignment)

	/* Version 5 only */
	uint32_t rw_feat_flags;    //!< Bytes 208-211  Read-write feature flags (not used)
	uint32_t ro_feat_flags;    //!< Bytes 212-215  Read-only feature flags
	uint32_t rw_inco_flags;    //!< Bytes 216-219  Read-write incompatibility flags
	uint32_t rw_inco_flags_log;//!< Bytes 220-223  Read-write incompat flags for log (unused)

	uint8_t  sb_crc32[4];      //!< Bytes 224-227  CRC32 checksum for superblock
	uint32_t sprs_inode_align; //!< Bytes 228-231  Sparse inode alignment
	int64_t  prj_quota_inode;  //!< Bytes 232-239  Project quota inode

	uint64_t last_sb_upd_lsn;  //!< Bytes 240-247  Log seq number of last superblock update
	uint8_t  inco_UUID[16];    //!< Bytes 248-263  UUID used if INCOMPAT_META_UUID feature
	uint64_t inco_rm_btree;    //!< Bytes 264-271  If INCOMPAT_META_RMAPBT, inode of RM btree
} xfs_sb_t;


/** @brief read a superblock from a file descriptor
  *
  * @param[out] sb  Pointer to the xfs_sb structure to fill
  * @param[in]  fd  file descriptor to the device to read
  * @param[in]  ag_num  Number of the allocation block to read
  * @param[in]  ag_size Number of blocks of each allocation group
  * @param[in]  bs  Block size of the file system
  * @return 0 on success, -1 if anything went wrong.
**/
int xfs_read_sb( xfs_sb_t* sb, int fd, uint32_t ag_num, uint32_t ag_size, uint32_t bs );


#endif // PWX_XFS_UNDELETE_SRC_SUPERBLOCK_H_INCLUDED
