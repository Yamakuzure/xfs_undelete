#ifndef PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED 1
#pragma once


#include "inode.h"


/** @brief clear the queue
  *
  * All elements are cleared and freed!
**/
void in_clear( void );


/** @brief pop an element from the directory inode queue (aka remove head)
  *
  * Note: This also removes the element from the queue.
  *
  * @return The head element of the directory inode queue or NULL if the queue is empty
**/
xfs_in_t* dir_in_pop( void );


/** @brief push an element onto the directory inode queue (aka push_back)
  * @param[in] Pointer to the element to push
  * return 0 on success, -1 if the new queue element could not be created.
**/
int dir_in_push( xfs_in_t* in );


/** @brief pop an element from the file inode queue (aka remove head)
  *
  * Note: This also removes the element from the queue.
  *
  * @return The head element of the file inode queue or NULL if the queue is empty
**/
xfs_in_t* file_in_pop( void );


/** @brief push an element onto the file inode queue (aka push_back)
  * @param[in] Pointer to the element to push
  * return 0 on success, -1 if the new queue element could not be created.
**/
int file_in_push( xfs_in_t* in );


#endif // PWX_XFS_UNDELETE_SRC_INODE_QUEUE_H_INCLUDED
