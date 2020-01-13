/*******************************************************************************
 * device.c : Functions for working with the source device
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "log.h"
#include "utils.h"
#include "superblock.h"


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mntent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>


// General local variables
static char*     mntDir           = NULL;
static char*     mntOpts          = NULL;
static char*     source_device    = NULL;
static char*     target_path      = NULL;
static bool      was_remounted_ro = false;

// XFS core information we need right here.
static char     sb_magic[5]     = { 0x0 }; /// Bytes  0- 3 : Magic Number, must be "XFSB"
static uint8_t  sb_uuid[16]     = { 0x0 }; /// Bytes 32-47 : UUID
static char     sb_uuid_str[37] = { 0x0 }; /// String representation of the UUID
static uint32_t sb_ag_size      = 0;       /// Bytes 84-87 : AG size (in blocks)

// Global XFS core information we also need elsewhere
uint64_t  full_ag_bytes    = 0;    //!< sb_ag_size * sb_block_size
uint64_t  full_disk_blocks = 0;    //!< sb_ag_count * sb_ag_size
uint64_t  full_disk_size   = 0;    //!< full_disk_blocks * sb_block_size
uint32_t  sb_ag_count      = 0;    //!< Bytes 88-91 : Number of AGs (normally 0x04)
uint32_t  sb_block_size    = 0;    //!< Bytes  4- 7 : Block Size (in bytes)
xfs_sb_t* superblocks      = NULL; //!< All AGs are loaded in here

// Global disk information
bool src_is_ssd = false;
bool tgt_is_ssd = false;

// Magic Codes of the different XFS blocks
uint8_t XFS_DB_MAGIC[4] = { 0x58, 0x44, 0x42, 0x33 }; // "XDB3" ; Single block long directory block
uint8_t XFS_DD_MAGIC[4] = { 0x58, 0x44, 0x44, 0x33 }; // "XDD3" ; Multi block long directory block
uint8_t XFS_DT_MAGIC[2] = { 0x3d, 0xf1};              //          Multi block long directory tail (hash) block
uint8_t XFS_IN_MAGIC[2] = { 0x49, 0x4e};              // "IN"   ; Inode magic
uint8_t XFS_SB_MAGIC[4] = { 0x58, 0x46, 0x53, 0x42 }; // "XFSB" ; Superblock magic


void free_devices( void ) {
	if ( source_device ) {
		if ( was_remounted_ro ) {
			log_info( "Restoring mount opts on %s ...", source_device );
			if ( mount( NULL, mntDir, NULL, MS_REMOUNT, NULL ) ) {
				log_error( "Remount rw %s failed, %m (%d)", source_device, errno );
				log_error( "Mount options: %s", mntOpts );
			} else
				log_info( "Mount options on %s restored!", source_device );
		}
		FREE_PTR( source_device );
	}
	FREE_PTR( mntDir );
	FREE_PTR( mntOpts );
	FREE_PTR( superblocks );
	FREE_PTR( target_path );
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
	int     fd      = open( source_device, O_RDONLY | O_NOFOLLOW );
	int     res     = fd;

	if ( -1 == res ) {
		log_error( "Can not open %s for reading: %m [%d]", source_device, errno );
		return -1;
	}

	res = read( fd, buf, 92 );
	close( fd );

	if ( -1 == res ) {
		log_error( "Can not read 92 bytes from %s : %m [%d]", source_device, errno );
		return -1;
	}

	// Now that we are here, get the data!
	memcpy(  sb_magic, buf,       4 );
	if ( memcmp( sb_magic, XFS_SB_MAGIC, 4 ) ) {
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
#if defined(PWX_DEBUG)
	if ( NULL == source_device ) {
		log_critical( "%s", "BUG! Call set_device() FIRST!" );
		return -1;
	}
#endif // debug

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

	// Now we need the source_device to be opened.
	int fd  = open( source_device, O_RDONLY | O_NOFOLLOW );
	if ( -1 == fd ) {
		log_error( "Can not open %s for reading: %m [%d]", source_device, errno );
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


/// @brief little helper, because we need this twice.
int is_device_ssd( bool* tgt, char const* dev ) {
	char        disk_part[6] = { 0x0 };
	char const* cur_p        = strrchr( dev, '/' );

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
					*tgt = true;
				else
					*tgt = false;
				fclose( rot );
			} else {
				log_error( "Unable to open %s: %m [%d]", rot_file, errno );
				return -1;
			}
		} else {
			log_error( "Bug (?) device disk part \"%s\" too large?", disk_part );
			return -1;
		}
	}

	return 0;
}


int set_source_device( char const* device_path ) {

	if ( source_device )
		free_devices();

	RETURN_INT_IF_NULL( device_path );

	source_device = strdup( device_path );


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
		if ( 0 == strcmp( ent->mnt_fsname, source_device ) ) {
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
			log_info( "%s mounted rw at %s, trying to remount ro...", source_device, mntDir );
			if ( mount( NULL, mntDir, NULL, MS_REMOUNT | MS_RDONLY, NULL ) ) {
				log_critical( "Remount ro %s failed, %m (%d)\n", source_device, errno );
				return -1;
			}
			was_remounted_ro = true;
			log_info( "%s remounted read-only.", source_device );
		} else
			log_info( "%s mounted ro at %s", source_device, mntDir );
	}

	/* === Check whether the device is an SSD. We do not    ===
	 * === want to got multi-threaded on a rotational disk! ===
	 * ========================================================
	 */
	if ( -1 == is_device_ssd( &src_is_ssd, source_device ) ) {
		log_error( "Can not determine whether %s is rotational", source_device );
		log_warning( " Assuming %s is a spinning disk and going to read single-threaded.", source_device );
		src_is_ssd = false;
	} else if ( src_is_ssd )
		log_info( "%s seems to be an SSD -> Reading multi-threaded!", source_device );
	else
		log_info( "%s assumed to be a rotating disk -> Reading single-threaded", source_device );

	return 0;
}


int set_target_path( char const* dir_path ) {
	if ( target_path )
		free_devices();

	RETURN_INT_IF_NULL( dir_path );

	target_path = strdup( dir_path );

	/* === Create the target path if needed ===
	 * ========================================
	 */
	if ( mkdirs( target_path ) )
		return -1;

	/* === See on which device the target directory is mounted ===
	 * ===========================================================
	 */
	char  full_path[PATH_MAX] = { 0x0 };
	char* target_device       = NULL;
	if ( NULL == realpath( target_path, full_path ) ) {
		log_critical( "Unable to resolve %s into a real path!", target_path );
		return -1;
	}
	log_debug( "Searching device for %s", full_path );

	struct mntent* ent     = NULL;
	FILE*          entFile = setmntent( "/proc/mounts", "r" );

	if ( NULL == entFile ) {
		log_error( "Could not open /proc/mounts: %m ($d)", errno );
		return -1;
	}
	while ( NULL != ( ent = getmntent( entFile ) ) ) {
		if ( full_path == strstr( full_path, ent->mnt_dir ) ) {
			// Note: This means that 'full_path' begins with 'ent->mnt_dir'
			if ( target_device && ( strlen( ent->mnt_fsname ) > strlen( target_device ) ) ) {
				FREE_PTR( target_device );
			}
			// Note: This re-assignment solves problems with sub-mounts
			if ( NULL == target_device ) {
				target_device = strdup( ent->mnt_fsname );
				log_debug( " ==> %s [%s] matches", ent->mnt_dir, ent->mnt_fsname );
			}
		}
	}
	endmntent( entFile );

	if ( NULL == target_device ) {
		log_critical( "Unable to determine on which device %s is mounted!", target_path );
		return -1;
	}

	/* === Some systems, like ZFS, can result in their own  ===
	 * === names like 'shared/data'. Let's try to decrypt   ===
	 * === that and get a device name                       ===
	 */
	if ( strchr( target_device, '/' ) ) {
		char*  tmp_device = strdup( target_device );
		size_t tmp_size   = strlen( tmp_device );
		int    off        = 'A' - 'a';

		log_debug(" ==> %s has a slash... investigating", target_device);

		for ( size_t i = 0; i < tmp_size; ++i ) {
			if ( '/' == tmp_device[i] ) {
				tmp_device[i] = 0x0;
				break;
			}
			if ( isupper( tmp_device[i] ) )
				tmp_device[i] -= off;
		}

		char tmp_full[PATH_MAX] = { 0x0 };
		snprintf( tmp_full, PATH_MAX - 1, "/dev/disk/by-label/%s", tmp_device );
		log_debug(" ==> Looking at %s ...", tmp_full);

		// Is our assumption correct?
		if ( realpath( tmp_full, full_path ) ) {
			// it is. :-)
			FREE_PTR( target_device );
			target_device = strdup( full_path );
		}
	}

	/* === Check whether the device is an SSD. We do not    ===
	 * === want to got multi-threaded on a rotational disk! ===
	 * ========================================================
	 */
	if ( -1 == is_device_ssd( &tgt_is_ssd, target_device ) ) {
		log_error( "Can not determine whether %s is rotational", target_device );
		log_warning( " Assuming %s is a spinning disk and going to write single-threaded.", target_device );
		tgt_is_ssd = false;
	} else if ( tgt_is_ssd )
		log_info( "%s seems to be an SSD -> Writing multi-threaded!", target_device );
	else
		log_info( "%s assumed to be a rotating disk -> Writing single-threaded", target_device );

	FREE_PTR( target_device );

	return 0;
}
