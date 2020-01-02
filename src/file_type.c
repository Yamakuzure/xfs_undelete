/*******************************************************************************
 * file_type.c : Helpers for getting and examining file types
 ******************************************************************************/


#include "common.h"
#include "file_type.h"


e_file_type get_file_type(uint8_t ftype_num) {
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
