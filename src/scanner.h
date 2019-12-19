#ifndef PWX_XFS_UNDELETE_SRC_SCANNER_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_SCANNER_H_INCLUDED 1
#pragma once


#include "xfs_sb.h"


#include <threads.h>


/// @brief Thread control struct
typedef struct _scan_data {
	uint32_t          ag_num;      //!< Number of the Allocation Group this thread shall handle
	char const*       device;      //!< Pointer to the device string. No copy, is never changed.
	_Atomic(bool)     do_start;    //!< Initialized with false, set to true when the thread may run.
	_Atomic(bool)     do_stop;     //!< Initialized with false, set to true when the thread shall break off
	_Atomic(bool)     is_finished; //!< Initialized with false, set to true when the thread is finished.
	xfs_sb*           sb_data;     //!< The Superblock data this thread shall handle
	_Atomic(uint64_t) sec_scanned; //!< Increased by the thread, questioned by main
	mtx_t             sleep_lock;  //!< Used for conditional sleeping until signaled
	uint32_t          thread_num;  //!< Number of the thread for logging
	_Atomic(uint64_t) undeleted;   //!< Increased by the thread, questioned by main
	cnd_t             wakeup_call; //!< Used by the main thread to signal the thread to continue
} scan_data_t;


/** @brief Initialize a scan_data_t structure with xfs_sb and ag number
  * @param[out] scan_data  Pointer to the scan_data_t instance to initialize
  * @param[in] thrd_num Number of the thread this structure is meant for
  * @param[in] dev_str  Pointer to the device string
  * @param[in] sb_data  Pointer to the xfs_sb instance to use
  * @param[in] ag_num  Number of the allocation group the superblock describes
  * @return 0 on success, -1 on failure
**/
int init_scan_data(scan_data_t* scan_data, uint32_t thrd_num, char const* dev_str, xfs_sb* sb_data, uint32_t ag_num);


/** @brief Main scan function
  * @param[in] scan_data  Pointer to the scan_data_t instance this thread handles.
  * @return 0 on success, non-zero otherwise.
**/
int scanner( void* scan_data );


#endif // PWX_XFS_UNDELETE_SRC_SCANNER_H_INCLUDED
