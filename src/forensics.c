/*******************************************************************************
 * forensics.c : functions for finding intel on unknown data
 ******************************************************************************/


#include "common.h"
#include "device.h"
#include "forensics.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "xfs_dir.h"
#include "xfs_ex.h"


#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


int is_deleted_inode( uint8_t const* data ) {
	RETURN_ZERO_IF_NULL( data );

	// Check magic first
	if ( memcmp( data, XFS_IN_MAGIC, 2 ) )
		return 0;

	// We need the version first, as some features need version v3
	uint8_t version = data[4];

	// Now copy what is zeroed/force on deletion (No flip needed, everything is fixed anyway)
	uint16_t type_mode       = *( ( uint16_t* )( data +  2 ) ); // <-- (ZEROED on delete)
	uint8_t  data_fork_type  = *( ( uint16_t* )( data +  5 ) ); // <-- (FORCED 2 on delete)
	uint16_t num_links_v1    = *( ( uint16_t* )( data +  6 ) ); // <-- (ZEROED on delete)
	uint32_t num_links_v2    = *( ( uint16_t* )( data + 16 ) ); // <-- (ZEROED on delete)
	uint64_t file_size       = *( ( uint16_t* )( data + 56 ) ); // <-- (ZEROED on delete)
	uint64_t file_blocks     = *( ( uint16_t* )( data + 64 ) ); // <-- (ZEROED on delete)
	uint32_t ext_used        = *( ( uint16_t* )( data + 76 ) ); // <-- (ZEROED on delete)
	uint8_t  xattr_off       = *( ( uint16_t* )( data + 82 ) ); // <-- (ZEROED on delete)
	uint8_t  xattr_type_flg  = *( ( uint16_t* )( data + 83 ) ); // <-- (FORCED 2 on delete)

	// There are some forced values (see above) on deleted inodes.
	// These can be used to identify an inode that got deleted.
	if ( type_mode                           // <-- (ZEROED on delete)
	  || ( data_fork_type != 2 )             // <-- (FORCED 2 on delete)
	  || ( ( version < 3 ) && num_links_v1 ) // <-- (ZEROED on delete)
	  || ( ( version > 2 ) && num_links_v2 ) // <-- (ZEROED on delete)
	  || file_size                           // <-- (ZEROED on delete)
	  || file_blocks                         // <-- (ZEROED on delete)
	  || ext_used                            // <-- (ZEROED on delete)
	  || xattr_off                           // <-- (ZEROED on delete)
	  || ( xattr_type_flg != 2 ) )           // <-- (FORCED 2 on delete)
		// Not a deleted inode, so it is uninteresting for us.
		return 0;

	// Found one
	return 1;
}


int is_directory_block( uint8_t const* data ) {
	RETURN_ZERO_IF_NULL( data );

	// --- Is this a regular directory inode ? ---
	if ( ( 0 == memcmp( data, XFS_IN_MAGIC, 2 ) )
	                && ( FT_DIR == ( ( data[2] & 0xf0 ) >> 4 ) ) )
		return 1;

	// --- It might be a long directory block ---
	if ( ( 0 == memcmp( data, XFS_DB_MAGIC, 4 ) )
	  || ( 0 == memcmp( data, XFS_DD_MAGIC, 4 ) )
	  || ( 0 == memcmp( data, XFS_DT_MAGIC, 2 ) ) )
		return 1;

	return 0;
}


typedef enum _recover_part {
	RP_DATA = 1, //!< Right after the core, extents or the B-Tree root of the data are located
	RP_GAP,      //!< There is (or might be) a zeroed gap between data and xattr
	RP_XATTR,    //!< Extended attributes are local or in extents
	RP_END       //!< xattrs are aligned to the end, but maybe we find strips of zero?
} e_recover_part;


int restore_inode( xfs_in* in, uint16_t inode_size, uint8_t const* data, int fd ) {
	RETURN_INT_IF_NULL( in   );
	RETURN_INT_IF_NULL( data );

	size_t         start  = in->version > 2 ? DATA_START_V3 : DATA_START_V1;
	e_recover_part e_part = RP_DATA;

	/* The theory is as follows:
	 * Most but very fragmented files are stored as ST_EXTENTS, thus, in extents
	 * which are listed right here in the inode.
	 * Most but long lists of extended attributes are stored as ST_LOCAL, thus
	 * right here in the inode.
	 * Short form directories are stored local, and we can test for them. Longer
	 * directories also use extents, so we'll have to check the first extent found
	 * so we know whether this is a data or directory inode.
	 * We go through the space left after the inode core and see whether we find
	 * extent information. These can be counted. 16 bytes of zeroes imply the end
	 * of a 1-n extent list. The next are then either extents to xattrs, or the
	 * attributes themselves. We try to read both and see whether either makes sense.
	 */
	size_t   strips         = ( inode_size - start ) / 16;
	uint64_t file_size      = 0; // <-- (ZEROED on delete)
	uint64_t file_blocks    = 0; // <-- (ZEROED on delete)
	uint32_t ext_used       = 0; // <-- (ZEROED on delete)
	bool     is_directory   = false;
	bool     d_is_extent    = false;
	bool     x_is_extent    = false;

	for ( size_t i = 0, offset = start           ;
	                ( RP_END != e_part ) && ( i < strips ) ;
	                ++i, offset += 16                      ) {

		uint8_t const* strip        = data + offset;
/// @todo : Break The Spaghetti!
		// ------------------------------------------------------------
		// Check 1: If the strip is zeroed, we go forward
		// ------------------------------------------------------------
		bool is_zero = true;
		for ( size_t j = 0; is_zero && ( j < 16 ); ++j ) {
			if ( strip[j] ) is_zero = false;
		}
		if ( is_zero ) {
			if ( RP_XATTR == e_part )
				e_part = RP_END;
			if ( RP_DATA == e_part )
				e_part = RP_GAP;
			continue;
		}

		// If we have data, but were in RP_GAP, we are in RP_XATTR now.
		if ( RP_GAP == e_part )
			e_part = RP_XATTR;
		// We do not know the xattr offset, yet, it might have another 8 bytes.

		// ------------------------------------------------------------
		// Check 2: Is this the start of a short form directory?
		// ------------------------------------------------------------
		if ( ( RP_DATA == e_part ) && !d_is_extent && !is_directory ) {
			xfs_dir_t test_dir;
			int res = xfs_init_packed_dir( &test_dir, strip );
			if ( 0 == res ) {
				// Well... looks good so far. And is there a first entry?
				size_t e_off = test_dir.entries_64bit ? 10 : 6;
				xfs_entry_t test_entry;

				res = xfs_read_packed_dir_entry( &test_entry, &test_dir, strip + e_off );
				xfs_free_entry( &test_entry ); // The name might be strdup'd

				if ( 0 == res ) {
					// SUCCESS! This is it!
					is_directory       = true;
					in->ftype          = FT_DIR;
					in->data_fork_type = ST_LOCAL;
					continue;
				}
			}
		} // End of checking for short form dirctory


		// ------------------------------------------------------------
		// Check 3: An extent that's on the disk?
		// ------------------------------------------------------------
		/* Note: We test every strip here. The reason is, that
		 *       there might be no gap between data extents, or a local
		 *       short form directory, and their extended attributes. In
		 *       such a case there are no means to detect the switch
		 *       from data to xattr otherwise.
		 */
		xfs_ex test_ex;
		xfs_read_ex( &test_ex, strip );
		if ( test_ex.block && test_ex.length
		                && ( ( test_ex.block + test_ex.length ) < full_disk_blocks ) ) {
			if ( is_directory && ( in->data_fork_type == ST_LOCAL ) ) {
				// Must be XATTR extents
				x_is_extent        = true;
				in->xattr_off      = offset;
				in->xattr_type_flg = ST_EXTENTS;
				e_part             = RP_XATTR; // Still need to count!
				in->num_xattr_exts = 1;
				continue;
			}
			// In any other case this is not that clear...
			uint8_t buf[10] = { 0x0 };
			ssize_t res     = pread( fd, buf, 10, test_ex.block * sb_block_size );
			if ( res > -1 ) {
				if ( is_directory_block( buf ) ) {
					// Alright, this case is clear.
					in->ftype          = FT_DIR;
					in->data_fork_type = ST_EXTENTS;
					d_is_extent        = true;
					is_directory       = true;
					continue;
				}
				// If we are here, there is no reason to not assume this
				// to be a regular data/xatrr extent
				if ( RP_XATTR == e_part ) {
					in->num_xattr_exts++;
					continue;
				}
				if ( ext_used ) {
					ext_used++;
					file_blocks += test_ex.length;
					file_size   += sb_block_size * test_ex.length;
					continue;
				}
				// Ok. So being here means that we are looking at the very first
				// extent in the inode and have no clue what we have here. The only
				// viable thing to do, as this is no directory entry or extent, obviously,
				// is to check whether there is a valid xattr block header behind this
				// extent. Then it is either xattr or data.
				if ( is_xattr_head( buf, inode_size - offset, NULL, NULL, NULL ) ) {
					// Thankfully we can check that.
					x_is_extent        = true;
					e_part             = RP_XATTR;
					in->num_xattr_exts = 1;
					in->xattr_off      = offset;
					in->xattr_type_flg = ST_EXTENTS;
					continue;
				}

				// As directories and xattr extents are now checked, only data remains.
				d_is_extent        = true;
				e_part             = RP_DATA;
				ext_used           = 1;
				file_blocks        = test_ex.length;
				file_size          = sb_block_size * test_ex.length;
				in->data_fork_type = ST_EXTENTS;
				continue;
			} // End of having read 10 bytes to check

			// Ouch... being here means the data is corrupted...
			log_debug( "Unable to read extent from 0x%08x: %m [%d]",
			           test_ex.block * sb_block_size, errno );
			return -1;
		} // End of checking extents

		// ------------------------------------------------------------
		// Check 4: Check whether this is a directory or data B-Tree
		// ------------------------------------------------------------
		/// @todo

		// ------------------------------------------------------------
		// Check 5: See whether this might be an XATTR local block
		// ------------------------------------------------------------
		if ( !x_is_extent ) {
			xattr* xattr_test = NULL;
			size_t x_off      = 0;
			for ( ; ( x_off < 16 ) && ( NULL == xattr_test ) ; x_off += 8 )
				// Note: Local xattrs always have an offset that is dividable by 8
				xattr_test = unpack_xattr_data( strip + x_off, inode_size - ( offset + x_off ) );

			if ( xattr_test ) {
				// This is obviously just fine like that.
				in->xattr_type_flg = ST_LOCAL;
				in->num_xattr_exts = 0;
				in->xattr_root     = xattr_test; // *yay* got it!
				in->xattr_off      = ( offset + x_off ) / 8;
				e_part             = RP_END; // Finished!
				continue;
			}
		}

		// Being here is bad. We have a non-zero strip that fits nowhere.
		// I know this can get noisy, but let's debug dump that, but don't give up.
		log_debug( "Unknown Strip: Inode %zu, stage %s", in->inode_id,
		           e_part == RP_DATA  ? "data " :
		           e_part == RP_END   ? "?END?" : /* should not be possible */
		           e_part == RP_GAP   ? "?GAP?" : /* should not be possible */
		           e_part == RP_XATTR ? "xattr" : "WTF??" );
		DUMP_STRIP( offset, strip );
	} // End of scanning strips

	if ( file_blocks && file_size ) {
		// Data extents are easily transfered
		in->file_blocks    = file_blocks;
		in->file_size      = file_size;
		in->ext_used       = ext_used;
		return 0;
	} else if ( is_directory )
		// Also in order
		return 0;

	// *meh*
	return -1;
} // end of restore_inode()

