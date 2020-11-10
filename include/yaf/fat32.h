/*
 * yaf/fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_FAT32_H_
#define YAF_FAT32_H_
/*----------------------------------------------------------------------------*/
#include <xcore/fs/fs.h>
#include <xcore/interface.h>
/*----------------------------------------------------------------------------*/
extern const struct FsHandleClass * const FatHandle;
/*----------------------------------------------------------------------------*/
struct Fat32Config
{
  /**
   * Mandatory: pointer to an initialized interface.
   */
  struct Interface *interface;
  /**
   * Optional: number of node descriptors in node pool. This option is used
   * only when support for object pools is enabled.
   */
  size_t nodes;
  /**
   * Optional: number of threads that can use the same handle simultaneously.
   * This option is used only when support for multiple threads is enabled.
   */
  size_t threads;
};
/*----------------------------------------------------------------------------*/
#endif /* YAF_FAT32_H_ */
