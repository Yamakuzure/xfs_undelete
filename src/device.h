#ifndef PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
#define PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED 1
#pragma once


/** @brief restore the source device mount status and free the internal paths
  *
  * If the source device was remounted read-only, try to restore the rw state.
  * After that the internal pointers to the source and target are freed.
  * If the remounting fails, a message is printed to stdout.
**/
void free_devices( void );


/** @brief scan the superblocks
  *
  * This will scan the superblocks and fill internal data structures.
  * Do not forget to call free_devices() when you are finished.
  *
  * @return 0 on success, -1 on failure.
**/
int scan_superblocks();


/** @brief Set the source device name and remount ro
  *
  * If the device exists and if it is mounted, it will be remounted
  * read-only.
  * Further it is checked whether the source device is an SSD, and
  * the global boolean `src_is_ssd` is set accordingly.
  *
  * When working with the device is finished, call `free_device()`
  * to clean up and remount rw.
  *
  * @param[in] device_path  Full path to the device to work with
  * @return 0 on success, -1 on failure.
**/
int set_source_device( char const* device_path );


/** @brief Set the target directory and check its device
  *
  * If the directory does not exist, create it.
  * Further it is checked whether the target device is an SSD, and
  * the global boolean `tgt_is_ssd` is set accordingly.
  *
  * When working with the device is finished, call `free_device()`
  * to clean up internal pointers.
  *
  * @param[in] dir_path  Full path to the directory to write recovered files into
  * @return 0 on success, -1 on failure.
**/
int set_target_path( char const* dir_path );


#endif // PWX_XML_UNDELETE_SRC_DEVICE_H_INCLUDED
