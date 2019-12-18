#ifndef PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


extern bool     disk_is_ssd;      /// If true, we can go multi-threaded
extern size_t   full_ag_size;     /// sb_ag_size * sb_block_size
extern size_t   full_disk_blocks; /// fsb_ag_count * sb_ag_size
extern size_t   full_disk_size;   /// full_disk_blocks * sb_block_size
extern uint32_t sb_ag_count;      /// Number of allocation groups
extern xfs_sb*  superblocks;      /// All AGs are loaded in here

#endif // PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
