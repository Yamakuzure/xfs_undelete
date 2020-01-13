/*******************************************************************************
 * writer.c : Write away restored data; to be used single- or multi-threaded
 ******************************************************************************/


#include "globals.h"
#include "log.h"
#include "superblock.h"
#include "writer.h"


#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


static int init_write_data( write_data_t* write_data, uint32_t thrd_num, char const* dev_str,
                            xfs_sb_t* sb_data, uint32_t ag_num ) {
	RETURN_INT_IF_NULL( write_data );
	RETURN_INT_IF_NULL( sb_data );
	RETURN_INT_IF_VLEV( sb_ag_count, ag_num );

	write_data->ag_num      = ag_num;
	write_data->device      = dev_str;
	write_data->do_start    = false;
	write_data->do_stop     = false;
	write_data->is_finished = false;
	write_data->is_running  = false;
	write_data->sb_data     = sb_data;
	write_data->thread_num  = thrd_num;
	write_data->undeleted   = 0;

	return 0;
}


write_data_t* create_writer_data( uint32_t ar_size, char const* dev_str ) {
	RETURN_NULL_IF_ZERO( ar_size );
	RETURN_NULL_IF_NULL( dev_str );

	write_data_t* data = (write_data_t*)calloc( ar_size, sizeof(write_data_t) );
	if ( NULL == data ) {
		log_critical( "Unable to allocate %zu bytes for writerr data array! %m [%d]",
		              sizeof(write_data_t) * ar_size, errno );
		return NULL;
	}

	int res = 0;

	for ( uint32_t i = 0; (0 == res) && (i < ar_size); ++i ) {
		res = init_write_data( &data[i], (2 * sb_ag_count) + i + 1, dev_str, &superblocks[i], i );
	}

	if ( -1 == res )
		free_writer_data( &data );

	return data;
}


void free_writer_data( write_data_t** data ) {
	RETURN_VOID_IF_NULL( data );
	FREE_PTR(*data);
}


int writer( void* write_data ) {
	RETURN_INT_IF_NULL( write_data );

	uint8_t*     buf  = NULL;
	write_data_t* data = ( write_data_t* )write_data;
	int          fd   = -1;
	int          res  = -1;


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
	data->is_running = true;

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
	data->is_running  = false;

	return res;
}

