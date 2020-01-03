/*******************************************************************************
 * analyzer.c : Inode/dir node analyzer; to be used single- or multi-threaded
 ******************************************************************************/


#include "analyzer.h"
#include "common.h"
#include "globals.h"
#include "inode_queue.h"
#include "log.h"
#include "utils.h"


#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


static int init_analyze_data( analyze_data_t* analyze_data, uint32_t thrd_num, char const* dev_str,
                              xfs_sb* sb_data, uint32_t ag_num ) {
	if ( NULL == analyze_data ) {
		log_critical( "%s", "Bug! Called with NULL analyze_data!" );
		return -1;
	}
	if ( NULL == sb_data ) {
		log_critical( "%s", "Bug! Called with NULL sb_data!" );
		return -1;
	}
	if ( sb_ag_count <= ag_num ) {
		log_critical( "Bug! Called with ag_num %lu/%lu!", ag_num, sb_ag_count );
		return -1;
	}

	analyze_data->ag_num       = ag_num;
	analyze_data->analyzed     = 0;
	analyze_data->device       = dev_str;
	analyze_data->do_start     = false;
	analyze_data->do_stop      = false;
	analyze_data->found_dirent = 0;
	analyze_data->found_files  = 0;
	analyze_data->is_finished  = false;
	analyze_data->sb_data      = sb_data;
	analyze_data->thread_num   = thrd_num;

	return 0;
}


int analyzer( void* analyze_data ) {
	if ( NULL == analyze_data ) {
		log_critical( "%s", "Bug! Called with NULL analyze_data!" );
		return -1;
	}


	uint8_t*        buf  = NULL;
	analyze_data_t* data = ( analyze_data_t* )analyze_data;
	int             fd   = -1;
	int             res  = -1;


	// Sleep until signaled to start
	mtx_lock( &data->sleep_lock );
	while ( ! ( data->do_start || data->do_stop ) ) {
		thrd_yield();
		cnd_wait( &data->wakeup_call, &data->sleep_lock );
	}
	mtx_unlock( &data->sleep_lock );

	// Don't really start if we are told to stop
	if ( data->do_stop )
		goto cleanup;

	// First we need a buffer:
	buf = malloc( sb_block_size );
	if ( NULL == buf ) {
		log_critical( "Unable to allocate %lu bytes for block buffer!", sb_block_size );
		goto cleanup;
	}

	// Let's open the device, then.
	fd  = open( data->device, O_RDONLY | O_NOFOLLOW );
	if ( -1 == fd ) {
		log_error( "[Thread %lu] Can not open %s for reading: %m [%d]",
		           data->thread_num, data->device, errno );
		goto cleanup;
	}

	// We are here? All is well, then
	res = 0;

cleanup:
	if ( fd > -1 )
		close( fd );
	if ( buf )
		free( buf );

	data->is_finished = true;

	return res;
}


analyze_data_t* create_analyze_data( uint32_t ar_size, char const* dev_str ) {
	if ( 0 == ar_size ) {
		log_critical("%s", "BUG! Called with zero array size!");
		return NULL;
	}
	if ( NULL == dev_str ) {
		log_critical("%s", "BUG! Called with NULL device string!");
		return NULL;
	}

	analyze_data_t* data = (analyze_data_t*)calloc( ar_size, sizeof(analyze_data_t) );
	if ( NULL == data ) {
		log_critical( "Unable to allocate %zu bytes for analyzer data array! %m [%d]",
		              sizeof(analyze_data_t) * ar_size, errno );
		return NULL;
	}

	int res = 0;

	for ( uint32_t i = 0; (0 == res) && (i < ar_size); ++i ) {
		res = init_analyze_data( &data[i], sb_ag_count + i, dev_str, &superblocks[i], i );
	}

	if ( -1 == res )
		free_analyze_data( &data );

	return data;
}


void free_analyze_data( analyze_data_t** data ) {
	if ( NULL == data ) {
		log_critical("%s", "BUG! Called with NULL data!");
		return;
	}

	FREE_PTR(*data);
}
