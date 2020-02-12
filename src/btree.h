#ifndef PWX_XFS_UNDELETE_SRC_BTREE_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_BTREE_H_INCLUDED 1
#pragma once


#include "extent.h"


#include <stdint.h>


typedef struct _btree_root {
	uint16_t  level;     //!< Bytes 0-1 : Level of this btree node
	uint16_t  num_recs;  //!< Bytes 2-3 : Number of records under this node
	/* The rest is split in the keys/pointer arrays */
	uint64_t* node_keys; //!< Array of in-file offsets [keys] (of size this->num_recs)
	uint64_t* node_ptrs; //!< Array of block addresses [ptrs] (of size this->num_recs)
} btree_root_t;


typedef struct _btree_node {
	/* first 4 bytes: magic code "BMAP" */
	uint16_t  level;     //!< Bytes  4- 5 : Level of this btree node
	uint16_t  num_recs;  //!< Bytes  6- 7 : Number of records under this node
	uint64_t  leftsib;   //!< Bytes  8-15 : Absolute block number of the left sibling
	uint64_t  rightsib;  //!< Bytes 16-23 : Absolute block number of the richt sibling
	/* the rest of the block is either split in the keys and the pointer arrays, or an extent list */
	uint64_t* node_keys; //!< Array of in-file offsets (of size this->num_recs) ; if this is a sub-node
	uint64_t* node_ptrs; //!< Array of block addresses (of size this->num_recs) ; if this is a sub-node
	xfs_ex_t* extents;   //!< Array of extents (of size this->num_recs)         ; if this is a leaf
} btree_node_t;

#endif // PWX_XFS_UNDELETE_SRC_BTREE_H_INCLUDED
