/*******************************************************************************
 * device.c : Functions for working with the source device
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "log.h"
#include "utils.h"
#include "xfs_sb.h"


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>


// General global variables
static char*     device           = NULL;
static char*     mntDir           = NULL;
static char*     mntOpts          = NULL;
static bool      was_remounted_ro = false;

// XFS core information we need right here.
static char     sb_magic[5]     = { 0x0 }; /// Bytes  0- 3 : Magic Number, must be "XFSB"
static uint8_t  sb_uuid[16]     = { 0x0 }; /// Bytes 32-47 : UUID
static char     sb_uuid_str[37] = { 0x0 }; /// String representation of the UUID
static uint32_t sb_ag_size      = 0;       /// Bytes 84-87 : AG size (in blocks)

// Global XFS core information we also need elsewhere
size_t   full_ag_bytes    = 0;    //!< sb_ag_size * sb_block_size
size_t   full_disk_blocks = 0;    //!< sb_ag_count * sb_ag_size
size_t   full_disk_size   = 0;    //!< full_disk_blocks * sb_block_size
uint32_t sb_ag_count      = 0;    //!< Bytes 88-91 : Number of AGs (normally 0x04)
uint32_t sb_block_size    = 0;    //!< Bytes  4- 7 : Block Size (in bytes)
xfs_sb*  superblocks      = NULL; //!< All AGs are loaded in here

// Global disk information
bool disk_is_ssd = false;


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
	if ( superblocks )
		free( superblocks );
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
	if ( strncmp( sb_magic, XFS_SB_MAGIC, 4 ) ) {
		log_critical( "Wrong magic: 0x%02x%02x%02x%02x instead of 0x%02x%02x%02x%02x",
		              sb_magic[0],     sb_magic[1],     sb_magic[2],     sb_magic[3],
		              XFS_SB_MAGIC[0], XFS_SB_MAGIC[1], XFS_SB_MAGIC[2], XFS_SB_MAGIC[3] );
		return -1;
	}

	memcpy(  sb_uuid,  buf + 32, 16 );
	sb_block_size = flip32( *( ( uint32_t* )( buf +  4 ) ) );
	sb_ag_size    = flip32( *( ( uint32_t* )( buf + 84 ) ) );
	sb_ag_count   = flip32( *( ( uint32_t* )( buf + 88 ) ) );
	format_uuid_str( sb_uuid_str, sb_uuid );

	full_ag_bytes     = ( size_t )sb_ag_size    * ( size_t )sb_block_size;
	full_disk_blocks = ( size_t )sb_ag_count   * ( size_t )sb_ag_size;
	full_disk_size   = ( size_t )sb_block_size * full_disk_blocks;

	log_debug( "Magic     : %s", sb_magic );
	log_debug( "UUID      : %s", sb_uuid_str );
	log_debug( "AG Count  : %u", sb_ag_count );
	log_debug( "AG Size   : %u (%s)", sb_ag_size, get_human_size( full_ag_bytes ) );
	log_debug( "Block Size: %u", sb_block_size );
	log_debug( "Disk Size : %s", get_human_size( full_disk_size ) );

	return 0;
}

int scan_superblocks() {
	if ( NULL == device ) {
		log_critical( "BUG! %s called before calling set_device()!", __func__ );
		return -1;
	}

	// First we need to find out how many allocation Groups (AG) there are and where to find them.
	if ( -1 == get_ag_base_info() )
		return -1;

	// Second we allocate our superblock structures
	superblocks = calloc( sb_ag_count, sizeof( struct _xfs_sb ) );
	if ( NULL == superblocks ) {
		log_critical( "Unable to allocate %zu bytes for superblock structures: %m [%d]",
		              sb_ag_count * sizeof( struct _xfs_sb ), errno );
		return -1;
	}

	// Now we need the device to be opened.
	int fd  = open( device, O_RDONLY | O_NOFOLLOW );
	if ( -1 == fd ) {
		log_error( "Can not open %s for reading: %m [%d]", device, errno );
		return -1;
	}

	// Now that we have the structures and the file descriptor, let's fill the structs:
	for ( uint32_t i = 0; i < sb_ag_count; ++i ) {
		if ( -1 == xfs_read_sb( &superblocks[i], fd, i, sb_ag_size, sb_block_size ) ) {
			log_critical( "Reading AG %lu/%lu FAILED!", i + 1, sb_ag_count );
			close( fd );
			return -1;
		}
	}

	close( fd );

	return 0;
}


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

	/* === Check whether the device is an SSD. We do not    ===
	 * === want to got multi-threaded on a rotational disk! ===
	 * ========================================================
	 */
	char  disk_part[6] = { 0x0 };
	char* cur_p        = strrchr( device, '/' );

	if ( cur_p ) {
		++cur_p;
		for ( int i = 0; *cur_p && !isdigit( *cur_p ) && ( i < 5 ); ++i, ++cur_p ) {
			disk_part[i] = *cur_p;
		}
	}

	if ( strlen( disk_part ) ) {
		char rot_file[40] = { 0x0 };
		int  written      = snprintf( rot_file, 40, "/sys/block/%s/queue/rotational", disk_part );
		if ( written < 40 ) {
			FILE* rot = fopen( rot_file, "r" );
			char  res = 0x0;
			if ( rot ) {
				if ( ( 1 == fread( &res, 1, 1, rot ) ) && ( res == '0' ) )
					disk_is_ssd = true;
				fclose( rot );
			} else
				log_error( "Unable to open %s: %m [%d]", rot_file, errno );
		} else
			log_error( "Bug (?) device disk part \"%s\" too large?", disk_part );
	}

	if ( disk_is_ssd )
		log_info( "/dev/%s seems to be an SSD -> Going multi-threaded!", disk_part );
	else
		log_info( "/dev/%s assumed to be a rotating disk -> Going single-threaded", disk_part );

	return 0;
}
