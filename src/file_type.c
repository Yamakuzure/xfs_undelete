/*******************************************************************************
 * file_type.c : Helpers for getting and examining file types
 ******************************************************************************/


#include "file_type.h"


e_file_type get_file_type( uint8_t ftype_num ) {
	switch( ftype_num ) {
		case FT_FIFO:
			return FT_FIFO;
		case FT_CHAR:
			return FT_CHAR;
		case FT_DIR:
			return FT_DIR;
		case FT_BLK:
			return FT_BLK;
		case FT_FILE:
			return FT_FILE;
		case FT_SYM:
			return FT_SYM;
		case FT_SOCK:
			return FT_SOCK;
	}
	return FT_INVALID;
}


e_file_type get_file_type_from_dirent( uint8_t ftype_num ) {
	switch( ftype_num ) {
		case 1:
			return FT_FILE;
		case 2:
			return FT_DIR;
		case 3:
			return FT_CHAR;
		case 4:
			return FT_BLK;
		case 5:
			return FT_FIFO;
		case 6:
			return FT_SOCK;
		case 7:
			return FT_SYM;
	}
	return FT_INVALID;
}
