#ifndef PWX_XFS_UNDELETE_SRC_ANALYZER_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_ANALYZER_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <stdbool.h>
#include <threads.h>


/// @brief Thread control struct
typedef struct _analyze_data {
	uint32_t            ag_num;       //!< Number of the Allocation Group this thread shall handle
	_Atomic( uint64_t ) analyzed;     //!< Increased by the thread, questioned by main
	char const*         device;       //!< Pointer to the device string. No copy, is never changed.
	_Atomic( bool )     do_start;     //!< Initialized with false, set to true when the thread may run.
	_Atomic( bool )     do_stop;      //!< Initialized with false, set to true when the thread shall break off
	_Atomic( uint64_t ) found_dirent; //!< Increased by the thread, questioned by main
	_Atomic( uint64_t ) found_files;  //!< Increased by the thread, questioned by main
	_Atomic( bool )     is_finished;  //!< Initialized with false, set to true when the thread is finished.
	xfs_sb*             sb_data;      //!< The Superblock data this thread shall use
	mtx_t               sleep_lock;   //!< Used for conditional sleeping until signaled
	uint32_t            thread_num;   //!< Number of the thread for logging
	cnd_t               wakeup_call;  //!< Used by the main thread to signal the thread to continue
} analyze_data_t;


/** @brief Main analyzer function
  * @param[in] analyze_data  Pointer to the analyze_data_t instance this thread handles.
  * @return 0 on success, non-zero otherwise.
**/
int analyzer( void* analyze_data );


/** @brief Create and initialize the analyze_data_t structure array
  * @param[in] ar_size  Size of the array to create
  * @param[in] dev_str  Pointer to the device string
  * @return Pointer to the created and initialized array, NULL on error
**/
analyze_data_t* create_analyze_data( uint32_t ar_size, char const* dev_str );


/** @brief free analyzer data
  * @param[in,out] data  Pointer to the analyze_data_t array to free
  * @param[in] ar_size  Size of the array
**/
void free_analyze_data( analyze_data_t** data );


#endif // PWX_XFS_UNDELETE_SRC_ANALYZER_H_INCLUDED
