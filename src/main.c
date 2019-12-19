/*******************************************************************************
 * main.c : Main program
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "globals.h"
#include "log.h"
#include "scanner.h"
#include "utils.h"


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>


int main( int argc, char const* argv[] ) {
	int res = EXIT_SUCCESS;

	/// === Parse command line options. ===
	/// ===================================
	char*        device_path = NULL;
	char*        output_dir  = NULL;
	scan_data_t* scan_data   = NULL;
	size_t       start_block = 0;
	thrd_t*      threads     = NULL;

	for ( int i = 1; i < argc; ++i ) {
		if ( 0 == strcmp( "-s", argv[i] ) ) {
			if ( ( i + 1 ) < argc )
				start_block = strtoull( argv[++i], NULL, 10 );
			else {
				fprintf( stderr, "ERROR: -s option needs a block number!\n" );
				return EXIT_FAILURE;
			}
		} else if ( device_path )
			output_dir = strdup( argv[i] );
		else
			device_path = strdup( argv[i] );
	}

	if ( output_dir ) {
		log_info( " -> Scanning device  : %s",  device_path );
		log_info( " -> into directory   : %s",  output_dir );
		log_info( " -> starting at block: %zu", start_block );
	} else {
		fprintf( stdout, "Usage: %s [-s start block] <device> <output dir>\n", argv[0] );
		return res;
	}

	/// === Create the recovery directory if it doesn't exist ===
	/// =========================================================
	if ( mkdirs( output_dir ) ) {
		res = EXIT_FAILURE;
		goto cleanup;
	}

	/// === Set the device path and remount read-only if necessary ===
	/// ==============================================================
	if ( set_device( device_path ) ) {
		res = EXIT_FAILURE;
		goto cleanup;
	}

	/// === Scan the Superblocks, we'll need them to get started. ===
	/// =============================================================
	if ( scan_superblocks() ) {
		res = EXIT_FAILURE;
		goto cleanup;
	}

	/// === Start one scanner for each AG if this is an SSD, or go     ===
	/// === through the AGs sequentially if this is a rotational disk. ===
	/// ==================================================================
	uint32_t max_threads = disk_is_ssd ? sb_ag_count : 1;
	uint32_t threads_ran = 0;

	// First we need scan data for each scanner (aka allocation group)
	scan_data = calloc(sb_ag_count, sizeof(scan_data_t));
	if (NULL == scan_data) {
		log_critical("Unable to allocate %zu bytes for %lu instances of scan_data_t: %m [%d]",
			(size_t)sb_ag_count * sizeof(scan_data_t), sb_ag_count, errno);
		goto cleanup;
	}
	for (uint32_t i = 0; i < sb_ag_count; ++i) {
		if (init_scan_data(&scan_data[i], i, device_path, &superblocks[i], i))
			goto cleanup; // Note: Already logged
	}

	// And we need enough thrd_t instances of course
	threads = calloc(sb_ag_count, sizeof(thrd_t));
	if (NULL == threads) {
		log_critical("Unable to allocate %zu bytes for %lu instances of thrd_t: %m [%d]",
			(size_t)sb_ag_count * sizeof(thrd_t), sb_ag_count, errno);
		goto cleanup;
	}

	// Now we can fire up our threads. Sequentially or parallel, depending on disk_is_ssd
	while (threads_ran < sb_ag_count) {
		uint32_t next_thread_num = threads_ran;

		// --- Create the threads ---
		// --------------------------
		for ( int i = next_thread_num; (i - threads_ran) < max_threads; ++i) {
			log_debug("Creating thread %lu/%lu (%lu/%lu)",
				i + 1, sb_ag_count, (i - threads_ran + 1), max_threads);
			thrd_create( &threads[i], scanner, &scan_data[i] );
		}

		// --- Start the threads ---
		// -------------------------
		for ( int i = next_thread_num; (i - threads_ran) < max_threads; ++i) {
			log_debug("Starting thread %lu/%lu (%lu/%lu)",
				i + 1, sb_ag_count, (i - threads_ran + 1), max_threads);
			scan_data[i].do_start = true;
			cnd_signal(&scan_data[i].wakeup_call);
		}

		// --- Watchdog the threads ---
		// ----------------------------
		uint32_t threads_running   = 1;
		struct timespec sleep_time = { .tv_nsec = 500000000 };

		while (threads_running) {
			uint64_t files_undeleted = 0;
			uint64_t sectors_scanned = 0;
			threads_running = 0;

			// Here we can go through the full set of data
			for ( uint32_t i = 0; i < max_threads; ++i) {
				if ( !scan_data[i].is_finished )
					threads_running++;
				files_undeleted += scan_data[i].undeleted;
				sectors_scanned += scan_data[i].sec_scanned;
			}

			// Get the stats out:
			show_progress( "Scanned % 10llu/% 10llu sectors (%6.2f%%);"
			               " %llu files restored;"
			               " % 2lu/% 2lu threads",
					sectors_scanned, full_disk_blocks,
					(double)sectors_scanned / (double)full_disk_blocks * 100.,
					files_undeleted, threads_running, max_threads);

			// Let's sleep for half a second
			thrd_sleep( &sleep_time, NULL );
		} // end of watch-dogging threads

		// --- Join the threads ---
		// ------------------------
		for ( int i = next_thread_num; (i - threads_ran) < max_threads; ++i) {
			int t_res = 0;
			log_debug("Joining thread %lu/%lu (%lu/%lu)",
				i + 1, sb_ag_count, (i - threads_ran) + 1, max_threads);
			thrd_join( threads[i], &t_res );
			if (t_res) {
				log_warning("Thread &lu reported a problem! [%d]", i, t_res);
				res = t_res;
			}
		}

		// Okay, these are finished.
		threads_ran = next_thread_num + max_threads;
	} // End of counting threads versus allocation groups


cleanup:
	free_device();

	if ( device_path )
		free( device_path );
	if ( output_dir )
		free( output_dir );
	if ( scan_data )
		free( scan_data );
	if ( threads )
		free( threads );

	return res;
} /* main() */

