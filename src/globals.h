#ifndef PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <stdint.h>
#include <stdlib.h>


extern size_t  full_ag_size;   /// sb_ag_size * sb_block_size
extern size_t  full_disk_size; /// sb_ag_count * sb_ag_size * sb_block_size
extern xfs_sb* superblocks;    /// All AGs are loaded in here

#endif // PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
