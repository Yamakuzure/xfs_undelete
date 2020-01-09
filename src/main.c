/*******************************************************************************
 * main.c : Main program
 ******************************************************************************/


#include "analyzer.h"
#include "common.h"
#include "device.h"
#include "globals.h"
#include "log.h"
#include "scanner.h"
#include "thrd_ctrl.h"
#include "utils.h"
#include "writer.h"


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Just a tiny shortcut to make all the possible points of failure not too messy
#define BREAK_OFF { res = EXIT_FAILURE ; goto cleanup; }

// Another two shortcuts to make one-func-tests easier on the eye
#define EXEC_OR_FAIL( func ) if (   -1 == ( func ) ) BREAK_OFF
#define SET_OR_FAIL(  val  ) if ( NULL == (  val ) ) BREAK_OFF


int main( int argc, char const* argv[] ) {
	char*           device_path  = NULL;
	char*           output_dir   = NULL;
	int             res          = EXIT_SUCCESS;

	/// === Parse command line options. ===
	/// ===================================
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

	/// === Set the source device, remount ro and check whether it is an SSD ===
	/// ========================================================================
	EXEC_OR_FAIL( set_source_device( device_path ) );

	/// === Create the target path and check whether its device is an SSD ===
	/// =====================================================================
	EXEC_OR_FAIL( set_target_path( output_dir ) );

	/// === Scan the Superblocks, we'll need them to get started. ===
	/// =============================================================
	EXEC_OR_FAIL( scan_superblocks() )

	/// ===  ----------------------
	/// ===  --- Main Work Loop ---
	/// ===  ----------------------
	/// === Until all of the source disk is scanned, do the following routine: ===
	/// ===  (( If the source disk is rotational ))
	/// ===  1) Start one scanner thread on the next AG
	/// ===  2) Monitor the scanner and wait it to finish
	/// ===  3) Finish the thread and continue with 1) until all AGs are scanned
	/// ===  4) Start the analyzer thread
	/// ===  5) Monitor the analyzer and wait for it to finish
	/// ===  6) Start the writer thread
	/// ===  7) Monitor the writer and wait for it to finish
	/// ===  (( If the source disk is *not* rotational ))
	/// ===  1) Start one scanner and one analyzer thread per AG
	/// ===  2) If the target disk is also *not* rotational, start the writer thread
	/// ===  3) Monitor the running threads and wait for them to finish
	/// ===   ( 4) If the target disk is rotational, start the writer thread now.
	/// ===   ( 5) Monitor the writer thread and wait for it to finish.
	/// ==================================================================

	// --- Pre) Prepare the thread data structures ---
	uint32_t max_threads = src_is_ssd ? ( 2 * sb_ag_count ) + ( tgt_is_ssd ? sb_ag_count : 1 ) : 1;

	SET_OR_FAIL( analyze_data = create_analyze_data( sb_ag_count, device_path ) );
	SET_OR_FAIL( scan_data    = create_scanner_data( sb_ag_count, device_path ) );
	SET_OR_FAIL( write_data   = create_writer_data(  sb_ag_count, device_path ) );

	while ( ag_scanned < sb_ag_count ) {
		// ---------------------------------------------------------------------
		// --- 1) Start one scanner total or one scanner and analyzer per ag ---
		// ---------------------------------------------------------------------
		if ( src_is_ssd ) {
			for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
				EXEC_OR_FAIL( start_analyzer( &analyze_data[i] ) );
				EXEC_OR_FAIL( start_scanner(  &scan_data[i] ) );
				if ( tgt_is_ssd || ( 0 == i ) ) {
					// If tgt is rotational, only one writer thread is allowed.
					EXEC_OR_FAIL( start_writer( &write_data[i] ) );
				}
			}
		} else
			EXEC_OR_FAIL( start_scanner( &scan_data[ag_scanned] ) );

		// ------------------------------------------------------------
		// --- 2) Wake up all threads                               ---
		// ------------------------------------------------------------
		wakeup_threads( true );

		// -----------------------------------------------------------------------------------
		// --- 3) Monitor the thread(s) and issue progress messages until all are finished ---
		// -----------------------------------------------------------------------------------
		monitor_threads( max_threads );

		// ----------------------------------------------
		// --- 4) Join all threads that have finished ---
		// ----------------------------------------------
		join_scanners( true );

		if ( src_is_ssd ) {
			join_analyzers( true );
			join_writers( true );
			ag_scanned = sb_ag_count;
			continue; // done!
		}

		/* We could be finished now. But if the source disk is rotational, we only
		 * have had one scanner thread. We need the analyzer, and then the writer.
		 * As this is basically the above, only sequential, the following should
		 * be fine without chapter-style comments.
		 */

		// First analyze ...
		EXEC_OR_FAIL( start_analyzer( &analyze_data[ag_scanned] ) );
		wakeup_threads( true );
		monitor_threads( max_threads );
		join_analyzers( true );

		// Then write...
		EXEC_OR_FAIL( start_writer( &write_data[ag_scanned] ) );
		wakeup_threads( true );
		monitor_threads( max_threads );
		join_writers( true );

		// ... and done with this allocation group
		++ag_scanned;
	} // End of waiting for everything to be scanned

	/// === Cleanup threads array ===
	/// =============================
	cleanup_threads();

	/// === Cleanup data arrays ===
	/// ===========================
	free_analyze_data( &analyze_data );
	free_scanner_data( &scan_data    );
	free_writer_data(  &write_data   );

cleanup:
	if ( EXIT_SUCCESS != res )
		// Sledge Hammer on error.
		end_threads();

	free_devices();
	FREE_PTR( device_path );
	FREE_PTR( output_dir );

	return res;
} /* main() */

