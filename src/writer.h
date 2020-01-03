#ifndef PWX_XFS_UNDELETE_SRC_WRITER_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_WRITER_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <stdbool.h>
#include <threads.h>


/// @brief Thread control struct
typedef struct _write_data {
	uint32_t            ag_num;      //!< Number of the Allocation Group this thread shall handle
	char const*         device;      //!< Pointer to the device string. No copy, is never changed.
	_Atomic( bool )     do_start;  //!< Initialized with false, set to true when the thread may run.
	_Atomic( bool )     do_stop;   //!< Initialized with false, set to true when the thread shall break off
	_Atomic( bool )     is_finished; //!< Initialized with false, set to true when the thread is finished.
	xfs_sb*             sb_data;     //!< The Superblock data this thread shall handle
	mtx_t               sleep_lock;  //!< Used for conditional sleeping until signaled
	uint32_t            thread_num;  //!< Number of the thread for logging
	_Atomic( uint64_t ) undeleted; //!< Increased by the thread, questioned by main
	cnd_t               wakeup_call; //!< Used by the main thread to signal the thread to continue
} write_data_t;


/** @brief Create and initialize the write_data_t structure array
  * @param[in] ar_size  Size of the array to create
  * @param[in] dev_str  Pointer to the device string
  * @return Pointer to the created and initialized array, NULL on error
**/
write_data_t* create_writer_data( uint32_t ar_size, char const* dev_str );


/** @brief free writer data
  * @param[in,out] data  Pointer to the write_data_t array to free
  * @param[in] ar_size  Size of the array
**/
void free_writer_data( write_data_t** data );


/** @brief Main writer function
  * @param[in] write_data  Pointer to the write_data_t instance this thread handles.
  * @return 0 on success, non-zero otherwise.
**/
int writer( void* write_data );


#endif // PWX_XFS_UNDELETE_SRC_WRITER_H_INCLUDED
