/*******************************************************************************
 * xfs_sb.c : Functions for working with the xfs superblock structure
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "log.h"
#include "utils.h"
#include "xfs_sb.h"


#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


int xfs_read_sb( xfs_sb* sb, int fd, uint32_t ag_num, uint32_t ag_size, uint32_t bs ) {
	if ( NULL == sb ) {
		log_critical( "%s", "BUG! Called with NULL sb!" );
		return -1;
	}

	uint8_t buf[271] = { 0x0 };
	int     res      = 0;
	off_t   offset   = ( off_t )ag_num * ( off_t )ag_size * ( off_t )bs;

	log_debug( "Reading AG %u at 0x%08x ...", ag_num, offset );

	off_t   pos      = lseek( fd, offset, SEEK_SET );

	if ( offset != pos ) {
		log_critical( "Could not seek to 0x%08x, ended up at 0x%08x!", offset, pos );
		log_critical( "%m [%d]", errno );
		return -1;
	}

	res = read( fd, buf, 271 );

	if ( -1 == res ) {
		log_critical( "Can not read 271 bytes from 0x%08x : %m [%d]", offset, errno );
		return -1;
	}

	// Now that we are here, get the data!
	memcpy( sb->magic,     buf,        4 );
	if ( strncmp( sb->magic, XFS_SB_MAGIC, 4 ) ) {
		log_critical( "Wrong magic: 0x%02x%02x%02x%02x instead of 0x%02x%02x%02x%02x",
		              sb->magic[0],    sb->magic[1],    sb->magic[2],    sb->magic[3],
		              XFS_SB_MAGIC[0], XFS_SB_MAGIC[1], XFS_SB_MAGIC[2], XFS_SB_MAGIC[3] );
		return -1;
	}

	memcpy( sb->UUID,      buf +  32, 16 );
	memcpy( sb->fs_name,   buf + 108, 12 );
	memcpy( sb->sb_crc32,  buf + 224,  4 );
	memcpy( sb->inco_UUID, buf + 248, 16 );

	log_debug( " ==> FS Name: \"%s\"", sb->fs_name[0] ? sb->fs_name : "(none set)" );

	sb->block_size        = flip32( *( ( uint32_t* )( buf +   4 ) ) );
	sb->total_blocks      = flip64( *( ( uint64_t* )( buf +   8 ) ) );
	sb->rt_block_count    = flip64( *( ( uint64_t* )( buf +  16 ) ) );
	sb->rt_extent_count   = flip64( *( ( uint64_t* )( buf +  24 ) ) );
	sb->journal_start     = flip64( *( ( uint64_t* )( buf +  48 ) ) );
	sb->root_inode        = flip64( *( ( uint64_t* )( buf +  56 ) ) );
	sb->rt_extent_inode   = flip64( *( ( uint64_t* )( buf +  64 ) ) );
	sb->rt_summary_inode  = flip64( *( ( uint64_t* )( buf +  72 ) ) );
	sb->rt_extent_size    = flip32( *( ( uint32_t* )( buf +  80 ) ) );
	sb->ag_size           = flip32( *( ( uint32_t* )( buf +  84 ) ) );
	sb->ag_count          = flip32( *( ( uint32_t* )( buf +  88 ) ) );
	sb->rt_bitmap_count   = flip32( *( ( uint32_t* )( buf +  92 ) ) );
	sb->journal_count     = flip32( *( ( uint32_t* )( buf +  96 ) ) );
	sb->fs_version        = flip16( *( ( uint16_t* )( buf + 100 ) ) );
	sb->sector_size       = flip16( *( ( uint16_t* )( buf + 102 ) ) );
	sb->inode_size        = flip16( *( ( uint16_t* )( buf + 104 ) ) );
	sb->inodes_per_block  = flip16( *( ( uint16_t* )( buf + 106 ) ) );
	sb->log2_block_size   =         *( ( uint8_t * )( buf + 120 ) )  ;
	sb->log2_sector_size  =         *( ( uint8_t * )( buf + 121 ) )  ;
	sb->log2_inode_size   =         *( ( uint8_t * )( buf + 122 ) )  ;
	sb->log2_inode_block  =         *( ( uint8_t * )( buf + 123 ) )  ;
	sb->log2_ag_size      =         *( ( uint8_t * )( buf + 124 ) )  ;
	sb->log2_rt_extents   =         *( ( uint8_t * )( buf + 125 ) )  ;
	sb->fs_created_flag   =         *( ( uint8_t * )( buf + 126 ) )  ;
	sb->max_inode_perc    =         *( ( uint8_t * )( buf + 127 ) )  ;
	sb->allocated_inodes  = flip64( *( ( uint64_t* )( buf + 128 ) ) );
	sb->free_inodes       = flip64( *( ( uint64_t* )( buf + 136 ) ) );
	sb->free_blocks       = flip64( *( ( uint64_t* )( buf + 144 ) ) );
	sb->free_rt_Extents   = flip64( *( ( uint64_t* )( buf + 152 ) ) );
	sb->user_quota_inode  = flip64( *( (  int64_t* )( buf + 160 ) ) );
	sb->group_quota_inode = flip64( *( (  int64_t* )( buf + 168 ) ) );
	sb->quota_flags       = flip16( *( ( uint16_t* )( buf + 176 ) ) );
	sb->misc_flags        =         *( ( uint8_t * )( buf + 178 ) )  ;
	sb->reserved_1        =         *( ( uint8_t * )( buf + 179 ) )  ;
	sb->inode_alignment   = flip32( *( ( uint32_t* )( buf + 180 ) ) );
	sb->raid_unit         = flip32( *( ( uint32_t* )( buf + 184 ) ) );
	sb->raid_stripe       = flip32( *( ( uint32_t* )( buf + 188 ) ) );
	sb->log2_dir_blk_ag   =         *( ( uint8_t * )( buf + 192 ) )  ;
	sb->log2_ext_jrnl_ss  =         *( ( uint8_t * )( buf + 193 ) )  ;
	sb->ext_jrnl_ss       = flip16( *( ( uint16_t* )( buf + 194 ) ) );
	sb->ext_jrnl_unit_s   = flip32( *( ( uint32_t* )( buf + 196 ) ) );
	sb->add_flags         = flip32( *( ( uint32_t* )( buf + 200 ) ) );
	sb->add_flags_repeat  = flip32( *( ( uint32_t* )( buf + 204 ) ) );
	sb->rw_feat_flags     = flip32( *( ( uint32_t* )( buf + 208 ) ) );
	sb->ro_feat_flags     = flip32( *( ( uint32_t* )( buf + 212 ) ) );
	sb->rw_inco_flags     = flip32( *( ( uint32_t* )( buf + 216 ) ) );
	sb->rw_inco_flags_log = flip32( *( ( uint32_t* )( buf + 220 ) ) );
	sb->sprs_inode_align  = flip32( *( ( uint32_t* )( buf + 228 ) ) );
	sb->prj_quota_inode   = flip64( *( (  int64_t* )( buf + 232 ) ) );
	sb->last_sb_upd_lsn   = flip64( *( ( uint64_t* )( buf + 240 ) ) );
	sb->inco_rm_btree     = flip64( *( ( uint64_t* )( buf + 264 ) ) );

	/// @todo : We should check CRC32 here!

	return 0;
}

