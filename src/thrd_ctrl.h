#ifndef PWX_XFS_UNDELETE_SRC_THRD_CTRL_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_THRD_CTRL_H_INCLUDED 1
#pragma once


#include "analyzer.h"
#include "scanner.h"
#include "writer.h"


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <threads.h>


/// @return return the total number of analyzer threads running (aka working)
uint32_t analyzers_running( void );


/** @brief cleanup remaining threads data
  * Do not forget to call this before exiting `main()`!
**/
void cleanup_threads( void );


/** @brief wake all threads up, stop and then join them
  *
  * This is used at the end of main() and should not do anything
  * at all. It is only really needed when we jump out from anywhere
  * else due to an error.
**/
void end_threads( void );


/** @brief Count relevant data of the analyzer threads
  *
  * @param[out] analyzed  Pointer to take the sum of analyzed entries
  * @param[out] dirents   Pointer to take the sum of gathered directory entries
  * @param[out] files     Pointer to take the sum of forwarded file inodes
**/
void get_analyzer_stats( uint64_t* analyzed, uint64_t* dirents, uint64_t* files );


/** @brief Count relevant data of the scanner threads
  *
  * @param[out] scanned  Pointer to take the sum of scanned sectors
  * @param[out] dirents  Pointer to take the sum of forwarded directory inodes
  * @param[out] inodes   Pointer to take the sum of forwarded deleted inodes
**/
void get_scanner_stats( uint64_t* scanned, uint64_t* dirents, uint64_t* inodes );


/** @brief Count relevant data of the writer threads
  *
  * @param[out] undeleted  Pointer to take the sum of undeleted files
**/
void get_writer_stats( uint64_t* undeleted );


/** @brief join all running analyzer threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
**/
void join_analyzers( bool finish_work );


/** @brief join all running scanner threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
  * @param[in] scan_count  Pointer to a counter to which the number of joined threads are added.
**/
void join_scanners( bool finish_work, uint32_t* scan_count );


/** @brief join all running writer threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
**/
void join_writers( bool finish_work );


/** @brief monitor all running threads, write progress twice per second, return when all are finished.
  *
  * @param[in] max_threads  Number of threads that have been started
**/
void monitor_threads( uint32_t max_threads );


/// @return return the total number of scanner threads running (aka working)
uint32_t scanner_running( void );


/** @brief Create and start one analyzer thread
  *
  * This creates and starts one analyzer thread. The thread is then put on hold,
  * and the function `wakeup_threads()` has to be called for it to start working.
  *
  * @param[in] data  Pointer to the analyze_data_t instance this thread shall handle
  * @return 0 on success, -1 on error.
**/
int start_analyzer( analyze_data_t* data );


/** @brief Create and start one scanner thread
  *
  * This creates and starts one scanner thread. The thread is then put on hold,
  * and the function `wakeup_threads()` has to be called for it to start working.
  *
  * @param[in] data  Pointer to the scan_data_t instance this thread shall handle
  * @return 0 on success, -1 on error.
**/
int start_scanner( scan_data_t* data );


/** @brief Create and start one writer thread
  *
  * This creates and starts one writer thread. The thread is then put on hold,
  * and the function `wakeup_threads()` has to be called for it to start working.
  *
  * @param[in] data  Pointer to the write_data_t instance this thread shall handle
  * @return 0 on success, -1 on error.
**/
int start_writer( write_data_t* data );


/// @brief notify all analyzer threads that the scanners are finished.
void unshackle_analyzers( void );


/** @brief convenience function to wake up all present threads at once
  *
  * @param[in] do_work  Set to true if the threads are allowed to work, false to make them end themselves.
**/
void wakeup_threads( bool do_work );


/// @return return the total number of writer threads running (aka working)
uint32_t writers_running( void );


#endif // PWX_XFS_UNDELETE_SRC_THRD_CTRL_H_INCLUDED
