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


static uint32_t threads_running( void ) {
	return analyzers_running() + scanner_running() + writers_running();
}


// ========================================
// --- Public functions implementations ---
// ========================================
uint32_t analyzers_running( void ) {
	uint32_t res = 0;

	if ( analyze_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( analyze_data[i].is_running && ( false == analyze_data[i].is_finished ) )
				++res;
		}
	}

	return res;
}


void cleanup_threads( void ) {
	FREE_PTR( threads );
}


void end_threads( void ) {

	/// 1) Wake them all up
	wakeup_threads( false );

	/// 2) Join all running threads
	join_scanners(  false, NULL ); // Order ...
	join_analyzers( false );       // ... matters
	join_writers(   false );

	/// 3) Destroy threads array
	cleanup_threads();

	/// 4) Destroy data arrays
	free_analyze_data( &analyze_data );
	free_scanner_data( &scan_data    );
	free_writer_data(  &write_data   );

	/* done */
}


void get_analyzer_stats( uint64_t* analyzed, uint64_t* dirents, uint64_t* files ) {
	RETURN_VOID_IF_NULL( analyzed );
	RETURN_VOID_IF_NULL( dirents  );
	RETURN_VOID_IF_NULL( files    );

	uint64_t a = 0, d = 0, f = 0;

	if ( analyze_data ) {
		for (uint32_t i = 0; i < sb_ag_count; ++i) {
			a += analyze_data[i].analyzed;
			d += analyze_data[i].found_dirent;
			f += analyze_data[i].found_files;
		}
	}

	if ( analyzed ) *analyzed = a;
	if ( dirents  ) *dirents  = d;
	if ( files    ) *files    = f;
}


void get_scanner_stats( uint64_t* scanned, uint64_t* dirents, uint64_t* inodes ) {
	RETURN_VOID_IF_NULL( scanned );
	RETURN_VOID_IF_NULL( dirents );
	RETURN_VOID_IF_NULL( inodes  );

	uint64_t s = 0, d = 0, i = 0;

	if ( scan_data ) {
		for (uint32_t j = 0; j < sb_ag_count; ++j) {
			s += scan_data[j].sec_scanned;
			d += scan_data[j].frwrd_dirent;
			i += scan_data[j].frwrd_inodes;
		}
	}

	if ( scanned ) *scanned = s;
	if ( dirents ) *dirents = d;
	if ( inodes  ) *inodes  = i;
}


void get_writer_stats( uint64_t* undeleted ) {
	RETURN_VOID_IF_NULL( undeleted );

	uint64_t u = 0;

	if ( write_data ) {
		for (uint32_t i = 0; i < sb_ag_count; ++i) {
			u += write_data[i].undeleted;
		}
	}

	if ( undeleted ) *undeleted = u;
}


void join_analyzers( bool finish_work ) {
	if ( analyze_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			int t_res = 0;

			if ( (analyze_data[i].thread_num > -1)
			  && threads[ analyze_data[i].thread_num ] ) {
				analyze_data[i].do_stop  = !finish_work;
				analyze_data[i].do_start =  finish_work;

				log_debug( "Joining analyzer thread %lu/%lu [%lu]",
				           i + 1, sb_ag_count, analyze_data[i].thread_num );

				thrd_join( threads[ analyze_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Analyzer thread &lu reported a problem! %s [%d]",
					             analyze_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ analyze_data[i].thread_num ] = 0;
				analyze_data[i].thread_num = -1;
			}
		}
	}
}


void join_scanners( bool finish_work, uint32_t* scan_count ) {
	if ( scan_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			int t_res = 0;

			if ( (scan_data[i].thread_num > -1)
			  && threads[ scan_data[i].thread_num ] ) {
				scan_data[i].do_stop  = !finish_work;
				scan_data[i].do_start =  finish_work;

				log_debug( "Joining scanner thread %lu/%lu [%lu]",
				           i + 1, sb_ag_count, scan_data[i].thread_num );

				thrd_join( threads[ scan_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Scanner thread &lu reported a problem! %s [%d]",
					             scan_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ scan_data[i].thread_num ] = 0;
				scan_data[i].thread_num = -1;

				if ( scan_count )
					(*scan_count)++;
			}
		}
	}
}



void join_writers( bool finish_work ) {
	if ( write_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			int t_res = 0;

			if ( (write_data[i].thread_num > -1)
			  && threads[ write_data[i].thread_num ] ) {
				write_data[i].do_stop  = !finish_work;
				write_data[i].do_start =  finish_work;

				log_debug( "Joining writer thread %lu/%lu [%lu]",
				           i + 1, sb_ag_count, write_data[i].thread_num );

				thrd_join( threads[ write_data[i].thread_num ], &t_res );
				if ( t_res )
					log_warning( "Writer thread &lu reported a problem! %s [%d]",
					             write_data[i].thread_num, get_thrd_err( t_res ), t_res );
				threads[ write_data[i].thread_num ] = 0;
				write_data[i].thread_num = -1;
			}
		}
	}
}


void monitor_threads( uint32_t max_threads ) {
	uint64_t analyzed          = 0;
	uint64_t found_dirent      = 0;
	uint64_t found_files       = 0;
	uint64_t frwrd_dirent      = 0;
	uint64_t frwrd_inodes      = 0;
	uint32_t running           = threads_running();
	uint64_t sec_scanned       = 0;
	struct timespec sleep_time = { .tv_nsec = 500000000 };
	uint64_t undeleted         = 0;

	while ( running ) {
		get_analyzer_stats( &analyzed,    &found_dirent, &found_files  );
		get_scanner_stats(  &sec_scanned, &frwrd_dirent, &frwrd_inodes );
		get_writer_stats(   &undeleted );

		show_progress( "[% 2lu/ 2%lu] % 10llu/% 10llu sec (%6.2f%%);"
		               " % 9llu/% 9llu found; % 9llu restored",
		               running, max_threads, sec_scanned, full_disk_blocks,
		               ( double )sec_scanned / ( double )full_disk_blocks * 100.,
		               found_files, frwrd_inodes, undeleted);

		// Let's sleep for half a second
		thrd_sleep( &sleep_time, NULL );

		running = threads_running();
	}

	// When all are finished, log the result out:
	log_info( "Scanned % 10llu/% 10llu sectors (%6.2f%%)",
	          sec_scanned, full_disk_blocks,
		( double )sec_scanned / ( double )full_disk_blocks * 100.);
	log_info( "Found   % 10llu/% 10llu directory entries", found_dirent, frwrd_dirent);
	log_info( "Found   % 10llu/% 10llu file inodes", found_files, frwrd_dirent);
	log_info( "Total   % 10llu files restored", undeleted);
}

uint32_t scanner_running( void ) {
	uint32_t res = 0;

	if ( scan_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( scan_data[i].is_running && ( false == scan_data[i].is_finished ) )
				++res;
		}
	}

	return res;
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


void unshackle_analyzers( void ) {
	if ( analyze_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			analyze_data[i].is_shackled = false;
		}
	}
}


void wakeup_threads( bool do_work ) {
	if ( analyze_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( analyze_data[i].thread_num > -1 ) {
				analyze_data[i].do_stop  = !do_work;
				analyze_data[i].do_start =  do_work;
				cnd_signal( &analyze_data[i].wakeup_call );
			}
		}
	}
	if ( scan_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( scan_data[i].thread_num > -1 ) {
				scan_data[i].do_stop  = !do_work;
				scan_data[i].do_start =  do_work;
				cnd_signal( &scan_data[i].wakeup_call );
			}
		}
	}
	if ( write_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( write_data[i].thread_num > -1 ) {
				write_data[i].do_stop  = !do_work;
				write_data[i].do_start =  do_work;
				cnd_signal( &write_data[i].wakeup_call );
			}
		}
	}
}


uint32_t writers_running( void ) {
	uint32_t res = 0;

	if ( write_data ) {
		for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
			if ( write_data[i].is_running && ( false == write_data[i].is_finished ) )
				++res;
		}
	}

	return res;
}
