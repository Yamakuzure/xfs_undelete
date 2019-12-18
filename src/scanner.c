/*******************************************************************************
 * scanner.c : Main scanner function to be used single- or multi-threaded
 ******************************************************************************/


#include "common.h"
#include "globals.h"
#include "log.h"
#include "scanner.h"


int init_scan_data(scan_data_t* scan_data, xfs_sb* sb_data, uint32_t ag_num) {
	if ( NULL == scan_data ) {
		log_critical("%s", "Bug! Called with NULL scan_data!");
		return -1;
	}
	if ( NULL == sb_data ) {
		log_critical("%s", "Bug! Called with NULL sb_data!");
		return -1;
	}
	if ( sb_ag_count <= ag_num ) {
		log_critical("Bug! Called with ag_num %lu/%lu!", ag_num, sb_ag_count);
		return -1;
	}

	scan_data->ag_num      = ag_num;
	scan_data->do_start    = false;
	scan_data->do_stop     = false;
	scan_data->is_finished = false;
	scan_data->sb_data     = sb_data;
	scan_data->sec_scanned = 0;

	return 0;
}


int scanner( void* scan_data ) {
	if ( NULL == scan_data ) {
		log_critical("%s", "Bug! Called with NULL scan_data!");
		return -1;
	}

	scan_data_t* data = (scan_data_t*)scan_data;

	// Sleep until signaled to start
	mtx_lock(&data->sleep_lock);
	while (! (data->do_start || data->do_stop) ) {
		thrd_yield();
		cnd_wait(&data->wakeup_call, &data->sleep_lock);
	}
	mtx_unlock(&data->sleep_lock);

	// Don't really start if we are told to stop
	if (data->do_stop) {
		data->is_finished = true;
		return 0;
	}



	// Finished and ready
	data->is_finished = true;

	return 0;
}


