/*******************************************************************************
 * scanner.c : Main scanner function to be used single- or multi-threaded
 ******************************************************************************/


#include "common.h"
#include "file_type.h"
#include "forensics.h"
#include "globals.h"
#include "inode_queue.h"
#include "log.h"
#include "scanner.h"
#include "utils.h"
#include "xfs_in.h"


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Will be set in main() from argv
uint64_t start_block = 0;


static int init_scan_data( scan_data_t* scan_data, uint32_t thrd_num, char const* dev_str,
                           xfs_sb_t* sb_data, uint32_t ag_num ) {
	RETURN_INT_IF_NULL( scan_data );
	RETURN_INT_IF_NULL( sb_data );
	RETURN_INT_IF_VLEV( sb_ag_count, ag_num );

	scan_data->ag_num       = ag_num;
	scan_data->device       = dev_str;
	scan_data->do_start     = false;
	scan_data->do_stop      = false;
	scan_data->frwrd_dirent = 0;
	scan_data->frwrd_inodes = 0;
	scan_data->is_finished  = false;
	scan_data->is_running   = true;
	scan_data->sb_data      = sb_data;
	scan_data->sec_scanned  = 0;
	scan_data->thread_num   = thrd_num;

	return 0;
}


// Internal inode counter and dumper, only needed in debugging mode for forensic analysis
#if defined(PWX_DEBUG)
#include <linux/limits.h>
#include <stdio.h>
static int dir_btree_found   = 0;
static int dir_extent_found  = 0;
static int dir_local_found   = 0;
static int file_btree_found  = 0;
static int file_extent_found = 0;
static void debug_perform_dump( xfs_in_t* inode, uint8_t* buf ) {
	static char dump_name[PATH_MAX] = { 0x0 };
	snprintf( dump_name, PATH_MAX - 1, "ag_%02u-blk_%010zu-off_%04zu-%s.dmp",
		  inode->ag_num, inode->block, inode->offset,
		  FT_DIR == inode->ftype ? "dirent" :
		  FT_FILE == inode->ftype ? "file": "other");

	int fd = open( dump_name, O_WRONLY | O_CREAT | O_TRUNC);
	if ( -1 < fd ) {
		write(fd, buf, inode->sb->inode_size);
		close(fd);
	} // No logging on errors, please. If it works, it works.
}
static int debug_dump_inode( xfs_in_t* inode, uint8_t* buf ) {
	if ( dir_btree_found && dir_extent_found && dir_local_found && file_btree_found && file_extent_found )
		return -1; // No more dumps

	if ( (FT_DIR == inode->ftype) && (ST_BTREE == inode->data_fork_type) && (dir_btree_found < 3) ) {
		++dir_btree_found;
		debug_perform_dump( inode, buf );
	}
	if ( (FT_DIR == inode->ftype) && (ST_EXTENTS == inode->data_fork_type) && (dir_extent_found < 3) ) {
		++dir_extent_found;
		debug_perform_dump( inode, buf );
	}
	if ( (FT_DIR == inode->ftype) && (ST_LOCAL == inode->data_fork_type) && (dir_local_found < 3) ) {
		++dir_local_found;
		debug_perform_dump( inode, buf );
	}
	if ( (FT_FILE == inode->ftype) && (ST_BTREE == inode->data_fork_type) && (file_btree_found < 3) ) {
		++file_btree_found;
		debug_perform_dump( inode, buf );
	}
	if ( (FT_FILE == inode->ftype) && (ST_EXTENTS == inode->data_fork_type) && (file_extent_found < 3) ) {
		++file_extent_found;
		debug_perform_dump( inode, buf );
	}

	return 0;
}
#endif // DEBUG


scan_data_t* create_scanner_data( uint32_t ar_size, char const* dev_str ) {
	RETURN_NULL_IF_ZERO( ar_size );
	RETURN_NULL_IF_NULL( dev_str );

	scan_data_t* data = ( scan_data_t* )calloc( ar_size, sizeof( scan_data_t ) );
	if ( NULL == data ) {
		log_critical( "Unable to allocate %zu bytes for scannerr data array! %m [%d]",
		              sizeof( scan_data_t ) * ar_size, errno );
		return NULL;
	}

	int res = 0;

	for ( uint32_t i = 0; ( 0 == res ) && ( i < ar_size ); ++i ) {
		res = init_scan_data( &data[i], i + 1, dev_str, &superblocks[i], i );
	}

	if ( -1 == res )
		free_scanner_data( &data );

	return data;
}


void free_scanner_data( scan_data_t** data ) {
	RETURN_VOID_IF_NULL( data );
	FREE_PTR( *data );
}


int scanner( void* scan_data ) {
	RETURN_INT_IF_NULL( scan_data );

	uint8_t*     buf  = NULL;
	scan_data_t* data = ( scan_data_t* )scan_data;
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

	// Set start and stop values
	size_t start_at = data->ag_num * data->sb_data->ag_size;
	size_t stop_at  = start_at + data->sb_data->ag_size;

	// Skip blocks if start_block was set:
	if ( start_block )
		start_at = start_block;
	/* Note: This will cause the full loop to be skipped if the start block lies
	 *       in a higher allocation group. But it is easier to skip right now
	 *       than to carve a few microseconds from the run time by optimizing
	 *       data and thread data plus thread creation.
	 */

	/// ==========================
	/// === Main Scanning Loop ===
	/// ==========================
	int      read_errors = 0; // Allow up to three consecutive read errors
	uint8_t* buf_p;           // Pointer into the buffer for inode searching
	off_t    offset;          // Offset of buf_p inside the block

	for ( size_t cur = start_at; ( false == data->do_stop ) && ( cur < stop_at ); ++cur ) {
		// reset buffer first, in case we don't read a full block for whatever reasons
		memset( buf, 0, sb_block_size );

		if ( -1 == pread( fd, buf, sb_block_size, cur * sb_block_size ) ) {
			log_error( "Read error on AG %u / sector %zu: %m [%d]",
			           data->ag_num, cur, errno );
			if ( ++read_errors > 3 ) {
				log_critical( "Three read errors in a row on AG %u, breaking off!",
				              data->ag_num );
				goto cleanup;
			}
			continue;
		} // End of encountering a read error

		// Reset read error counter, only consecutive errors lead to a break off
		read_errors = 0;

		// reset working values
		offset = 0;
		buf_p  = NULL;

		// Now go through the block and see whether there are inodes inside
		while ( ( false == data->do_stop ) && ( offset < sb_block_size ) ) {
			buf_p = buf + offset;

			if ( is_valid_inode( data->sb_data, buf_p )
			  && (is_deleted_inode( buf_p ) || is_directory_block( buf_p ) ) ) {

				xfs_in_t* inode = xfs_create_in( data->ag_num, cur, offset );
				if ( NULL == inode )
					goto cleanup;

				if ( 0 == xfs_read_in( inode, buf_p, fd ) ) {
					int r = 1;

					// That inode is good, so push or unshift it.
					if ( FT_DIR == inode->ftype ) {
						r = dir_in_push( inode );
						data->frwrd_dirent++;
					} else if ( FT_FILE == inode->ftype ) {
						r = file_in_push( inode );
						data->frwrd_inodes++;
					} else
						// Other types are irrelevant at this time
						xfs_free_in( &inode );

					// Paranoia check against oom
					if ( -1 == r ) {
						log_critical( "Inode queue broken? [%d] Breaking off work!", r );
						goto cleanup;
					}

/// Only scan until enough inodes are dumped.
#if defined(PWX_DEBUG)
					// Note: debug_dump_inode returns -1 if enough inodes have been
					//       Dumped. We don't fail here, just end work early.
					if ( (0 == r) && (-1 == debug_dump_inode(inode, buf)) ) {
						res = 0;
						goto cleanup;
					}
#endif // DEBUG
				}
				// No else, would be nothing of interest. Errors have been logged already
			} // End of having found an inode of interest

			offset += data->sb_data->inode_size;
		} // End of searching inodes inside the block

		data->sec_scanned++;

	} // End of Main Scanning Loop

	// We are here? All is well, then
	res = 0;

cleanup:
	if ( fd > -1 )
		close( fd );
	FREE_PTR( buf );

	data->is_finished = true;
	data->is_running  = false;

	return res;
}
