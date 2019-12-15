/*******************************************************************************
 * main.c : Main program
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "globals.h"
#include "log.h"
#include "utils.h"


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main( int argc, char const* argv[] ) {
	int res = EXIT_SUCCESS;

	/// === Parse command line options. ===
	/// ===================================
	char*  device_path = NULL;
	char*  output_dir  = NULL;
	size_t start_block = 0;

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

	/// === Set the device path and remount read-only if neccessary ===
	/// ===============================================================
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

cleanup:
	free_device();

	if ( device_path )
		free( device_path );
	if ( output_dir )
		free( output_dir );

	return res;
} /* main() */

