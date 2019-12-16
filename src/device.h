#ifndef PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
#define PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED 1
#pragma once


#define XFS_SB_MAGIC "XFSB"


/** @brief restore the devices mount status and free the internal path
  *
  * If the device was remounted read-only, try to restore the rw state.
  * After that the internal pointer is freed.
  * If the remounting fails, a message is printed to stdout.
**/
void free_device( void );


/** @brief scan the superblocks
  *
  * This will scan the superblocks and fill internal data structures.
  * Do not forget to call free_device() when you are finished.
  *
  * @return 0 on success, -1 on failure.
**/
int scan_superblocks();


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
int set_device( char const* device_path );


#endif // PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
