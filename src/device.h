#ifndef PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
#define PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED 1
#pragma once


void free_device     ( void );
int  scan_superblocks();
int  set_device      ( char const* device_path );


#endif // PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
