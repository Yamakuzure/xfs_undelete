/*******************************************************************************
 * xfs_sb.c : Functions for working with the xfs superblock structure
 ******************************************************************************/


#include "device.h"
#include "globals.h"
#include "log.h"
#include "superblock.h"
#include "utils.h"


#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


int xfs_read_sb( xfs_sb_t* sb, int fd, uint32_t ag_num, uint32_t ag_size, uint32_t bs ) {
	RETURN_INT_IF_NULL( sb );

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
	if ( memcmp( sb->magic, XFS_SB_MAGIC, 4 ) ) {
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

	sb->block_size        = get_flip32u( buf,   4 );
	sb->total_blocks      = get_flip64u( buf,   8 );
	sb->rt_block_count    = get_flip64u( buf,  16 );
	sb->rt_extent_count   = get_flip64u( buf,  24 );
	sb->journal_start     = get_flip64u( buf,  48 );
	sb->root_inode        = get_flip64u( buf,  56 );
	sb->rt_extent_inode   = get_flip64u( buf,  64 );
	sb->rt_summary_inode  = get_flip64u( buf,  72 );
	sb->rt_extent_size    = get_flip32u( buf,  80 );
	sb->ag_size           = get_flip32u( buf,  84 );
	sb->ag_count          = get_flip32u( buf,  88 );
	sb->rt_bitmap_count   = get_flip32u( buf,  92 );
	sb->journal_count     = get_flip32u( buf,  96 );
	sb->fs_version        = get_flip16u( buf, 100 );
	sb->sector_size       = get_flip16u( buf, 102 );
	sb->inode_size        = get_flip16u( buf, 104 );
	sb->inodes_per_block  = get_flip16u( buf, 106 );
	sb->log2_block_size   = get_flip8u(  buf, 120 );
	sb->log2_sector_size  = get_flip8u(  buf, 121 );
	sb->log2_inode_size   = get_flip8u(  buf, 122 );
	sb->log2_inode_block  = get_flip8u(  buf, 123 );
	sb->log2_ag_size      = get_flip8u(  buf, 124 );
	sb->log2_rt_extents   = get_flip8u(  buf, 125 );
	sb->fs_created_flag   = get_flip8u(  buf, 126 );
	sb->max_inode_perc    = get_flip8u(  buf, 127 );
	sb->allocated_inodes  = get_flip64u( buf, 128 );
	sb->free_inodes       = get_flip64u( buf, 136 );
	sb->free_blocks       = get_flip64u( buf, 144 );
	sb->free_rt_Extents   = get_flip64u( buf, 152 );
	sb->user_quota_inode  = get_flip64s( buf, 160 );
	sb->group_quota_inode = get_flip64s( buf, 168 );
	sb->quota_flags       = get_flip16u( buf, 176 );
	sb->misc_flags        = get_flip8u(  buf, 178 );
	sb->reserved_1        = get_flip8u(  buf, 179 );
	sb->inode_alignment   = get_flip32u( buf, 180 );
	sb->raid_unit         = get_flip32u( buf, 184 );
	sb->raid_stripe       = get_flip32u( buf, 188 );
	sb->log2_dir_blk_ag   = get_flip8u(  buf, 192 );
	sb->log2_ext_jrnl_ss  = get_flip8u(  buf, 193 );
	sb->ext_jrnl_ss       = get_flip16u( buf, 194 );
	sb->ext_jrnl_unit_s   = get_flip32u( buf, 196 );
	sb->add_flags         = get_flip32u( buf, 200 );
	sb->add_flags_repeat  = get_flip32u( buf, 204 );
	sb->rw_feat_flags     = get_flip32u( buf, 208 );
	sb->ro_feat_flags     = get_flip32u( buf, 212 );
	sb->rw_inco_flags     = get_flip32u( buf, 216 );
	sb->rw_inco_flags_log = get_flip32u( buf, 220 );
	sb->sprs_inode_align  = get_flip32u( buf, 228 );
	sb->prj_quota_inode   = get_flip64s( buf, 232 );
	sb->last_sb_upd_lsn   = get_flip64u( buf, 240 );
	sb->inco_rm_btree     = get_flip64u( buf, 264 );

	/// @todo : We should check CRC32 here!

	return 0;
}

