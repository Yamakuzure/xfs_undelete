/*******************************************************************************
 * thrd_ctrl.c : functions and data for thread control, according to environment
 ******************************************************************************/


#include "analyzer.h"
#include "common.h"
#include "globals.h"
#include "log.h"
#include "scanner.h"
#include "thrd_ctrl.h"
#include "utils.h"
#include "writer.h"


#include <errno.h>
#include <threads.h>


// The control values needed to work with the many threads we might fire up
uint32_t        ag_scanned        = 0;
analyze_data_t* analyze_data      = NULL;
uint32_t        max_read_threads  = 1;
uint32_t        max_write_threads = 1;
scan_data_t*    scan_data         = NULL;
thrd_t*         threads           = NULL;
write_data_t*   write_data        = NULL;


static int create_threads_array() {
	if ( NULL == threads ) {
		/* The maximum number of threads is 3 * sb_ag_count, and we can just create that array.
		 * It would be an enormous undertaking to optimize this array size, and still get the
		 * numbers of the threads right everywhere. It is much simpler to create the full array,
		 * which will have a size of 12 in most cases anyway.
		 */
		threads = ( thrd_t* )calloc( sb_ag_count * 3, sizeof( thrd_t ) );
		if ( NULL == threads ) {
			log_critical( "Unable to allocate %zu bytes for threads array! %m [%d]",
			              sizeof( thrd_t ) * sb_ag_count, errno );
			return -1;
		}
	}
	return 0;
}

static char const* get_thrd_err( int err ) {
	switch ( err ) {
		case thrd_success:
			return "Successful";
			break;
		case thrd_timedout:
			return "Thread timed out";
			break;
		case thrd_busy:
			return "thread or resource temporary unavailable (busy)";
			break;
		case thrd_nomem:
			return "Out of memory";
			break;
		case thrd_error:
			return "Error!";
	}
	return "Invalid thread error code";
}


// ========================================
// --- Public functions implementations ---
// ========================================
void cleanup_threads( void ) {
	FREE_PTR( threads );
}


void end_threads( void ) {

	/// 1) Wake them all up
	wakeup_threads( false );

	/// 2) Join all running threads
	join_scanners(  false ); // Order ...
	join_analyzers( false ); // ... matters
	join_writers(   false );

	/// 3) Destroy threads array
	cleanup_threads();

	/// 4) Destroy data arrays
	free_analyze_data( &analyze_data );
	free_scanner_data( &scan_data    );
	free_writer_data(  &write_data   );

	/* done */
}


void join_analyzers( bool finish_work ) {
	if ( analyze_data ) {
		for ( uint32_t i = 0; i < max_read_threads; ++i ) {
			int      t_res      = 0;

			if ( threads[ analyze_data[i].thread_num ] ) {
				analyze_data[i].do_stop  = !finish_work;
				analyze_data[i].do_start =  finish_work;

				log_debug( "Joining analyzer thread %lu/%lu [%lu]",
				           i + 1, max_read_threads, analyze_data[i].thread_num );

				thrd_join( threads[ analyze_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Analyzer thread &lu reported a problem! %s [%d]",
					             analyze_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ analyze_data[i].thread_num ] = 0;
			}
		}
	}
}


void join_scanners( bool finish_work ) {
	if ( scan_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			int      t_res      = 0;

			if ( threads[ scan_data[i].thread_num ] ) {
				analyze_data[i].do_stop  = !finish_work;
				analyze_data[i].do_start =  finish_work;

				log_debug( "Joining scanner thread %lu/%lu [%lu]",
				           i + 1, sb_ag_count, scan_data[i].thread_num );

				thrd_join( threads[ scan_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Scanner thread &lu reported a problem! %s [%d]",
					             scan_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ scan_data[i].thread_num ] = 0;
			}
		}
	}
}



void join_writers( bool finish_work ) {
	if ( write_data ) {
		for ( uint32_t i = 0; i < max_write_threads; ++i ) {
			int      t_res      = 0;

			if ( threads[ write_data[i].thread_num ] ) {
				write_data[i].do_stop  = !finish_work;
				write_data[i].do_start =  finish_work;

				log_debug( "Joining writer thread %lu/%lu [%lu]",
				           i + 1, max_write_threads, write_data[i].thread_num );

				thrd_join( threads[ write_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Writer thread &lu reported a problem! %s [%d]",
					             write_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ write_data[i].thread_num ] = 0;
			}
		}
	}
}


int start_analyzer( analyze_data_t* data ) {
	RETURN_INT_IF_NULL( data );

	int res = create_threads_array();
	if ( -1 == res )
		return res;

	res = thrd_create( &threads[data->thread_num], analyzer, data );
	if ( thrd_success != res ) {
		log_critical( "Creation of analyzer thread %u failed! %s",
		              data->thread_num, get_thrd_err( res ) );
		return -1;
	}

	return 0;
}


int start_scanner( scan_data_t* data ) {
	RETURN_INT_IF_NULL( data );

	int res = create_threads_array();
	if ( -1 == res )
		return -1;

	res = thrd_create( &threads[data->thread_num], scanner, data );
	if ( thrd_success != res ) {
		log_critical( "Creation of scanner thread %u failed! %s",
		              data->thread_num, get_thrd_err( res ) );
		return -1;
	}

	return 0;
}


int start_writer( write_data_t* data ) {
	RETURN_INT_IF_NULL( data );

	int res = create_threads_array();
	if ( -1 == res )
		return -1;

	res = thrd_create( &threads[data->thread_num], writer, data );
	if ( thrd_success != res ) {
		log_critical( "Creation of writer thread %u failed! %s",
		              data->thread_num, get_thrd_err( res ) );
		return -1;
	}

	return 0;
}


void wakeup_threads( bool do_work ) {
	if ( analyze_data ) {
		for ( uint32_t i = 0; i < max_read_threads; ++i ) {
			if ( analyze_data[i].thread_num ) {
				analyze_data[i].do_stop  = !do_work;
				analyze_data[i].do_start =  do_work;
				cnd_signal( &analyze_data[i].wakeup_call );
			}
		}
	}
	if ( scan_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( scan_data[i].thread_num ) {
				scan_data[i].do_stop  = !do_work;
				scan_data[i].do_start =  do_work;
				cnd_signal( &scan_data[i].wakeup_call );
			}
		}
	}
	if ( write_data ) {
		for ( uint32_t i = 0; i < max_write_threads; ++i ) {
			if ( write_data[i].thread_num ) {
				write_data[i].do_stop  = !do_work;
				write_data[i].do_start =  do_work;
				cnd_signal( &write_data[i].wakeup_call );
			}
		}
	}
}
