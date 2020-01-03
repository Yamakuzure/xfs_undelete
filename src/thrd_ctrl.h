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


/** @brief join all running analyzer threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
**/
void join_analyzers( bool finish_work );


/** @brief join all running scanner threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
**/
void join_scanners( bool finish_work );


/** @brief join all running writer threads
  *
  * @param[in] finish_work  If set to true, let the threads finish their work. Set to false to end them asap.
**/
void join_writers( bool finish_work );


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


/** @brief convenience function to wake up all present threads at once
  *
  * @param[in] do_work  Set to true if the threads are allowed to work, false to make them end themselves.
**/
void wakeup_threads( bool do_work );


#endif // PWX_XFS_UNDELETE_SRC_THRD_CTRL_H_INCLUDED
