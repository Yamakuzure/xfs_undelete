/*******************************************************************************
 * inode_queue.c : Thread-safe queue for sending scanned inodes to the analyzer
 ******************************************************************************/


#include "inode.h"
#include "inode_queue.h"
#include "log.h"
#include "utils.h"


#include <stdlib.h>
#include <threads.h>


/// @brief Simple plain stupid xfs_in queue
typedef struct _in_queue {
	struct _xfs_in*   in;   //!< The inode this element is about
	struct _in_queue* next; //!< next element in the queue
	struct _in_queue* prev; //!< previous element in the queue
} in_queue_t;


/* To be thread safe we need a central mutex per queue.
 * Any operation must be solitude!
 */
static mtx_t dir_queue_lock;
static mtx_t file_queue_lock;


/* We need to know both queues head and tail */
static in_queue_t* dir_in_head  = NULL;
static in_queue_t* dir_in_tail  = NULL;
static in_queue_t* file_in_head = NULL;
static in_queue_t* file_in_tail = NULL;


/// @brief internal in_queue_t creator. Returns -1 on calloc failure.
static int create_in_elem( in_queue_t** elem, xfs_in_t* in ) {
	in_queue_t* new_elem = calloc( 1, sizeof( struct _in_queue ) );

	if ( new_elem ) {
		new_elem->in = in;
		*elem = new_elem;
		return 0;
	}

	log_critical( "Unable to allocate %zu bytes for in_queue_t element!", sizeof( struct _in_queue ) );
	return -1;
}


void in_clear( void ) {
	xfs_in_t* elem = NULL;

	// Directory queue
	do {
		elem = dir_in_pop();
		xfs_free_in( &elem );
	} while ( elem );

	// File queue
	do {
		elem = file_in_pop();
		xfs_free_in( &elem );
	} while ( elem );
}


xfs_in_t* dir_in_pop( void ) {
	in_queue_t* elem   = NULL;
	xfs_in_t*   result = NULL;

	mtx_lock( &dir_queue_lock );
	if ( dir_in_head ) {
		elem        = dir_in_head;
		dir_in_head = elem->next;
		if ( NULL == dir_in_head )
			// Was last element
			dir_in_tail = NULL;
	}
	mtx_unlock( &dir_queue_lock );

	if ( elem ) {
		result = TAKE_PTR( elem->in );
		RELEASE( elem );
		FREE_PTR( elem );
	}

	return result;
}


int dir_in_push( xfs_in_t* in ) {
	RETURN_INT_IF_NULL( in );

	in_queue_t* elem = NULL;

	if ( -1 == create_in_elem( &elem, in ) )
		return -1;

	mtx_lock( &dir_queue_lock );
	if ( dir_in_tail ) {
		dir_in_tail->next = elem;
		elem->prev        = dir_in_tail;
		dir_in_tail       = elem;
	} else {
		// First element
		dir_in_head = elem;
		dir_in_tail = elem;
	}
	mtx_unlock( &dir_queue_lock );

	return 0;
}


xfs_in_t* file_in_pop( void ) {
	in_queue_t* elem   = NULL;
	xfs_in_t*   result = NULL;

	mtx_lock( &file_queue_lock );
	if ( file_in_head ) {
		elem         = file_in_head;
		file_in_head = elem->next;
		if ( NULL == file_in_head )
			// Was last element
			file_in_tail = NULL;
	}
	mtx_unlock( &file_queue_lock );

	if ( elem ) {
		result = TAKE_PTR( elem->in );
		RELEASE( elem );
		FREE_PTR( elem );
	}

	return result;
}


int file_in_push( xfs_in_t* in ) {
	RETURN_INT_IF_NULL( in );

	in_queue_t* elem = NULL;

	if ( -1 == create_in_elem( &elem, in ) )
		return -1;

	mtx_lock( &file_queue_lock );
	if ( file_in_tail ) {
		file_in_tail->next = elem;
		elem->prev        = file_in_tail;
		file_in_tail       = elem;
	} else {
		// First element
		file_in_head = elem;
		file_in_tail = elem;
	}
	mtx_unlock( &file_queue_lock );

	return 0;
}

