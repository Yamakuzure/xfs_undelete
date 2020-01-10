#ifndef PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED 1
#pragma once


#include "xfs_in.h"


/** @brief clear the queue
  *
  * All elements are cleared and freed!
**/
void in_clear( void );


/** @brief pop an element from the queue (aka remove head)
  *
  * Note: This also removes the element from the queue.
  *
  * @return The head element of the queue or NULL if the queue is empty
**/
xfs_in_t* in_pop( void );


/** @brief push an element onto the queue (aka push_back)
  * @param[in] Pointer to the element to push
  * return 0 on success, -1 if the new queue element could not be created.
**/
int in_push( xfs_in_t* in );


#endif // PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
