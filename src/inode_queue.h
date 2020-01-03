#ifndef PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED 1
#pragma once


#include "xfs_in.h"


/** @brief pop an element from the queue (aka remove head)
  *
  * push/pop is regular FIFO behavior, unshift/pop would be LIFO (stack)
  *
  * @return The head element of the queue or NULL if the queue is empty
**/
xfs_in* in_pop( void );


/** @brief push an element onto the queue (aka push_back)
  *
  * push/pop is regular FIFO behavior, push/shift would be LIFO (stack)
  *
  * @param[in] Pointer to the element to push
  * return 0 on success, -1 if the new queue element could not be created.
**/
int in_push( xfs_in* in );


/** @brief shift an element from the queue (aka remove tail)
  *
  * unshift/shift is regular FIFO behavior, push/shift would be LIFO (stack)
  *
  * @return The head element of the queue or NULL if the queue is empty
**/
xfs_in* in_shift( void );


/** @brief unshift an element under the queue (aka push_front)
  *
  * unshift/shift is regular FIFO behavior, unshift/pop would be LIFO (stack)
  *
  * @param[in] Pointer to the element to push
  * return 0 on success, -1 if the new queue element could not be created.
**/
int in_unshift( xfs_in* in );


#endif // PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
