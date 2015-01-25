/*
 * libyaf/fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBYAF_FAT32_H_
#define LIBYAF_FAT32_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <interface.h>
#include <fs.h>
#include <rtc.h>
/*----------------------------------------------------------------------------*/
extern const struct FsHandleClass * const FatHandle;
/*----------------------------------------------------------------------------*/
struct Fat32Config
{
  /** Mandatory: pointer to an initialized interface. */
  struct Interface *interface;
  /**
   * Optional: pointer to a real-time clock. This option is used
   * only when time support is enabled.
   */
  struct Rtc *timer;
  /**
   * Optional: number of file descriptors in file pool. This option is used
   * only when support for object pools is enabled.
   */
  uint16_t files;
  /**
   * Optional: number of node descriptors in node pool. This option is used
   * only when support for object pools is enabled.
   */
  uint16_t nodes;
  /**
   * Optional: number of directory descriptors in directory pool.
   * This option is used only when support for object pools is enabled.
   */
  uint16_t directories;
  /**
   * Optional: number of threads that can use the same handle simultaneously.
   * This option is used only when support for multiple threads is enabled. */
  uint16_t threads;
};
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_WRITE) && defined(CONFIG_FAT_DEBUG)
uint32_t fat32CountFree(void *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* LIBYAF_FAT32_H_ */
