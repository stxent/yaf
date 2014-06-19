/*
 * fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef FAT32_H_
#define FAT32_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <interface.h>
#include <fs.h>
/*----------------------------------------------------------------------------*/
extern const struct FsHandleClass * const FatHandle;
/*----------------------------------------------------------------------------*/
struct Fat32Config
{
  /** Pointer to previously opened interface. */
  struct Interface *interface;

#ifdef CONFIG_FAT_POOLS
  /** Number of descriptors in file pools. */
  uint16_t files;
  /** Number of node descriptors in node pool. */
  uint16_t nodes;
  /** Number of directory descriptors in directory entry pool. */
  uint16_t directories;
#endif

#ifdef CONFIG_FAT_THREADS
  /** Number of threads that can use the same handle simultaneously. */
  uint16_t threads;
#endif
};
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(void *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* FAT32_H_ */
