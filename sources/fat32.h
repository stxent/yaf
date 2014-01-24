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
extern const struct FsHandleClass *FatHandle;
/*----------------------------------------------------------------------------*/
struct Fat32Config
{
  struct Interface *interface;
};
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
struct FatMetadata
{
  struct FsMetadata parent;

  uint32_t cluster;
  uint32_t pcluster;
  uint16_t pindex;
};
#endif
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(void *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* FAT32_H_ */
