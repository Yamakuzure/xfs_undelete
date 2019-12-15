/*******************************************************************************
 * device.c : Functions for working with the source device
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "log.h"
#include "utils.h"


#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>


// General global variables
static char** block_buf        = NULL;
static char*  device           = NULL;
static char*  mntDir           = NULL;
static char*  mntOpts          = NULL;
static bool   was_remounted_ro = false;

// XFS core information we need right here.
static char     sb_magic[5]     = { 0x0 }; /// Bytes  0- 3 : Magic Number, must be "XFSB"
static uint32_t sb_block_size   = 0;       /// Bytes  4- 7 : Block Size (in bytes)
static uint8_t  sb_uuid[16]     = { 0x0 }; /// Bytes 32-47 : UUID
static char     sb_uuid_str[37] = { 0x0 }; /// String representation of the UUID
static uint32_t sb_ag_size      = 0;       /// Bytes 84-87 : AG size (in blocks)
static uint32_t sb_ag_count     = 0;       /// Bytes 88-91 : Number of AGs (normally 0x04)


/** @brief restore the devices mount status and free the internal path
  *
  * If the device was remounted read-only, try to restore the rw state.
  * After that the internal pointer is freed.
  * If the remounting fails, a message is printed to stdout.
**/
void free_device( void ) {
	if ( device ) {
		if ( was_remounted_ro ) {
			log_info( "Restoring mount opts on %s ...", device );
			if ( mount( NULL, mntDir, NULL, MS_REMOUNT, NULL ) ) {
				log_error( "Remount rw %s failed, %m (%d)", device, errno );
				log_error( "Mount options: %s", mntOpts );
			} else
				log_info( "Mount options on %s restored!", device );
		}
		free( device );
		device = NULL;
	}
	if ( mntDir )
		free( mntDir );
	if ( mntOpts )
		free( mntOpts );
}


/// @internal
static int get_ag_base_info() {
	// Note: We need just the first 92 bytes here, so low-level open/read/close is good to go.
	/* Before we can really start reading the superblocks, we need to know a few details beforehand:
	 * Bytes  0- 3 : Magic Number, should be "XFSB" or this is no XFS
	 * Bytes  4- 7 : Block Size (in bytes)
	 * Bytes 32-47 : UUID (right now only for a fance message.)
	 * Bytes 84-87 : AG size (in blocks)
	 * Bytes 88-91 : Number of AGs (normally 0x04)
	*/
	uint8_t buf[92] = { 0x0 };
	int     fd      = open( device, O_RDONLY | O_NOFOLLOW );
	int     res     = fd;

	if ( -1 == res ) {
		log_error( "Can not open %s for reading: %m [%d]", device, errno );
		return -1;
	}

	res = read( fd, buf, 92 );
	close( fd );

	if ( -1 == res ) {
		log_error( "Can not read 92 bytes from %s : %m [%d]", device, errno );
		return -1;
	}

	// Now that we are here, get the data!
	memcpy(  sb_magic, buf,       4 );
	memcpy(  sb_uuid,  buf + 32, 16 );
	sb_block_size = flip32( *( ( uint32_t* )( buf +  4 ) ) );
	sb_ag_size    = flip32( *( ( uint32_t* )( buf + 84 ) ) );
	sb_ag_count   = flip32( *( ( uint32_t* )( buf + 88 ) ) );
	snprintf( sb_uuid_str, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	          sb_uuid[ 0], sb_uuid[ 1], sb_uuid[ 2], sb_uuid[ 3],
	          sb_uuid[ 4], sb_uuid[ 5],
	          sb_uuid[ 6], sb_uuid[ 6],
	          sb_uuid[ 8], sb_uuid[ 7],
	          sb_uuid[10], sb_uuid[11], sb_uuid[12], sb_uuid[13], sb_uuid[14], sb_uuid[15] );

	return 0;
}

/** @brief scan the superblocks
  *
  * This will scan the superblocks and fill internal data structures.
  * Do not forget to call free_device() when you are finished.
  *
  * @return 0 on success, -1 on failure.
**/
int scan_superblocks() {
	if ( NULL == device ) {
		log_critical( "BUG! %s called before calling set_device()!", __func__ );
		return -1;
	}

	// First we need to find out how many allocation Groups (AG) there are and where to find them.
	if ( -1 == get_ag_base_info() )
		return -1;

	return 0;
}


/** @brief Set the device name and remount ro
  *
  * If the device exists and if it is mounted, it will be remounted
  * read-only.
  * When working with the device is finished, call `free_device()`
  * to clean up and remount rw.
  *
  * @param[in] device_path  Full path to the device to work with
  * @return 0 on success, -1 on failure.
**/
int set_device( char const* device_path ) {

	if ( device )
		free_device();

	if ( device_path )
		device = strdup( device_path );
	else {
		log_critical( "BUG! %s called without device path!", __func__ );
		return -1;
	}


	/* === See where the device is mounted ===
	 * =======================================
	 */
	struct mntent* ent     = NULL;
	FILE*          entFile = setmntent( "/proc/mounts", "r" );

	if ( NULL == entFile ) {
		log_error( "Could not open /proc/mounts: %m ($d)", errno );
		return -1;
	}
	while ( ( NULL == mntDir ) && ( NULL != ( ent = getmntent( entFile ) ) ) ) {
		if ( 0 == strcmp( ent->mnt_fsname, device ) ) {
			mntDir  = strdup( ent->mnt_dir );
			mntOpts = strdup( ent->mnt_opts );
		}
	}
	endmntent( entFile );

	/* === Remount the target filesystem read-only if mounted ===
	 * ==========================================================
	 */
	if ( mntDir ) {
		char* has_rw = strstr( mntOpts, MNTOPT_RW );
		if ( has_rw ) {
			log_info( "%s mounted rw at %s, trying to remount ro...", device, mntDir );
			if ( mount( NULL, mntDir, NULL, MS_REMOUNT | MS_RDONLY, NULL ) ) {
				log_critical( "Remount ro %s failed, %m (%d)\n", device, errno );
				return -1;
			}
			was_remounted_ro = true;
			log_info( "%s remounted read-only.", device );
		} else
			log_info( "%s mounted ro at %s", device, mntDir );
	}

	return 0;
}
