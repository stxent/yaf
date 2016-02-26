/*
 * libyaf/fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBYAF_FAT32_H_
#define LIBYAF_FAT32_H_
/*----------------------------------------------------------------------------*/
#include <fs.h>
#include <interface.h>
#include <realtime.h>
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
  struct RtClock *clock;
  /**
   * Optional: number of node descriptors in node pool. This option is used
   * only when support for object pools is enabled.
   */
  size_t nodes;
  /**
   * Optional: number of threads that can use the same handle simultaneously.
   * This option is used only when support for multiple threads is enabled. */
  size_t threads;
};
/*----------------------------------------------------------------------------*/
#endif /* LIBYAF_FAT32_H_ */
