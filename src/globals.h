#ifndef PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED 1
#pragma once


#include "analyzer.h"
#include "scanner.h"
#include "writer.h"
#include "xfs_sb.h"


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


extern size_t   full_ag_bytes;    //!< sb_ag_size * sb_block_size
extern size_t   full_disk_blocks; //!< fsb_ag_count * sb_ag_size
extern size_t   full_disk_size;   //!< full_disk_blocks * sb_block_size
extern uint32_t sb_ag_count;      //!< Number of allocation groups
extern uint32_t sb_block_size;    //!< Size of the file system sectors
extern bool     src_is_ssd;       //!< If true, we can read multi-threaded
extern xfs_sb*  superblocks;      //!< All AGs are loaded in here
extern bool     tgt_is_ssd;       //!< If true, we can write multi-threaded

// Progress and thread control values
extern uint32_t        ag_scanned;  //!< Every joined scanner thread raises this by one (defined in thrd_ctrl.c)
extern analyze_data_t* analyze_data;
extern scan_data_t*    scan_data;
extern write_data_t*   write_data;

// Magic Codes of the different XFS blocks
uint8_t XFS_DB_MAGIC[4]; //!< Single block long directory block
uint8_t XFS_DD_MAGIC[4]; //!< Multi block long directory block
uint8_t XFS_DT_MAGIC[2]; //!< Multi block long directory tail (hash) block
uint8_t XFS_IN_MAGIC[2]; //!< Inode magic
uint8_t XFS_SB_MAGIC[4]; //!< Superblock magic


// Sizes of the inode cores, aka "where the data starts"
extern size_t DATA_START_V1;
extern size_t DATA_START_V3;


#endif // PWX_XFS_UNDELETE_SRC_GLOBALS_H_INCLUDED
