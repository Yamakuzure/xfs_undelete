/*******************************************************************************
 * inode_queue.c : Thread-safe queue for sending scanned inodes to the analyzer
 ******************************************************************************/


#include "common.h"
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


/* To be thread safe we need a central mutex.
 * Any operation must be solitude!
 */
static mtx_t queue_lock;


/* We need to know both head and tail */
static in_queue_t* in_head = NULL;
static in_queue_t* in_tail = NULL;


/// @brief internal in_queue_t creator. Returns -1 on calloc failure.
static int create_in_elem( in_queue_t** elem, xfs_in* in ) {
	in_queue_t* new_elem = calloc( 1, sizeof( struct _in_queue ) );

	if ( new_elem ) {
		new_elem->in = in;
		*elem = new_elem;
		return 0;
	}

	log_critical( "Unable to allocate %zu bytes for in_queue_t element!", sizeof( struct _in_queue ) );
	return -1;
}


xfs_in* in_pop( void ) {
	in_queue_t* elem   = NULL;
	xfs_in*     result = NULL;

	mtx_lock( &queue_lock );
	if ( in_head ) {
		elem  = in_head;
		in_head = elem->next;
		if ( NULL == in_head )
			// Was last element
			in_tail = NULL;
	}
	mtx_unlock( &queue_lock );

	if ( elem ) {
		result = TAKE_PTR( elem->in );
		RELEASE( elem );
		FREE_PTR( elem );
	}

	return result;
}


int in_push( xfs_in* in ) {
	RETURN_INT_IF_NULL( in );

	in_queue_t* elem = NULL;

	if ( -1 == create_in_elem( &elem, in ) )
		return -1;

	mtx_lock( &queue_lock );
	if ( in_tail ) {
		in_tail->next = elem;
		elem->prev    = in_tail;
		in_tail       = elem;
	} else {
		// First element
		in_head = elem;
		in_tail = elem;
	}
	mtx_unlock( &queue_lock );

	return 0;
}


xfs_in* in_shift( void ) {
	in_queue_t* elem   = NULL;
	xfs_in*     result = NULL;

	mtx_lock( &queue_lock );
	if ( in_tail ) {
		elem    = in_tail;
		in_tail = elem->prev;
		if ( NULL == in_tail )
			// Was last element
			in_head = NULL;
	}
	mtx_unlock( &queue_lock );

	if ( elem ) {
		result = TAKE_PTR( elem->in );
		RELEASE( elem );
		FREE_PTR( elem );
	}

	return result;
}


int in_unshift( xfs_in* in ) {
	RETURN_INT_IF_NULL( in );

	in_queue_t* elem = NULL;

	if ( -1 == create_in_elem( &elem, in ) )
		return -1;

	mtx_lock( &queue_lock );
	if ( in_head ) {
		in_head->prev = elem;
		elem->next    = in_head;
		in_head       = elem;
	} else {
		// First element
		in_head = elem;
		in_tail = elem;
	}
	mtx_unlock( &queue_lock );

	return 0;
}


