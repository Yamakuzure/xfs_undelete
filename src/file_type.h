#ifndef PWX_XFS_UNDELETE_SRC_FILE_TYPE_H_INCLUDED
#define PWX_XFS_UNDELETE_SRC_FILE_TYPE_H_INCLUDED 1
#pragma once


/// @brief enum showing the type of an inode
typedef enum _file_type {
	FT_FIFO    = 0x01, //!< FIFO
	FT_CHAR    = 0x02, //!< Character special device
	FT_DIR     = 0x04, //!< Directory
	FT_BLK     = 0x06, //!< Block special device
	FT_FILE    = 0x08, //!< Regular file
	FT_SYM     = 0x0a, //!< Symlink
	FT_SOCK    = 0x0c, //!< Socket
	FT_INVALID = 0xff  //!< Whatever was questioned is not listened above.
} e_file_type;


/** @brief give back the e_file_type to @a ftype_num
  * @param[in] ftype_num  The numerical file type
  * @return  The corresponding e_file_type or FT_INVALID if @a ftype_num is unknown
**/
e_file_type get_file_type(uint8_t ftype_num);


#endif // PWX_XFS_UNDELETE_SRC_FILE_TYPE_H_INCLUDED
