#ifndef PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


extern size_t   full_ag_bytes;    /// sb_ag_size * sb_block_size
extern size_t   full_disk_blocks; /// fsb_ag_count * sb_ag_size
extern size_t   full_disk_size;   /// full_disk_blocks * sb_block_size
extern uint32_t sb_ag_count;      /// Number of allocation groups
extern uint32_t sb_block_size;    /// Size of the file system sectors
extern bool     src_is_ssd;       /// If true, we can read multi-threaded
extern xfs_sb*  superblocks;      /// All AGs are loaded in here
extern bool     tgt_is_ssd;       /// If true, we can write multi-threaded

#endif // PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
