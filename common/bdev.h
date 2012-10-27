/*
 * bdev.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BDEV_H_
#define BDEV_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#include "interface.h"
/*----------------------------------------------------------------------------*/
#define SECTOR_POW      9 /* Sector size in power of 2 */
#define SECTOR_SIZE     (1 << SECTOR_POW) /* Sector size in bytes */
#define FS_BUFFER       (SECTOR_SIZE * 1) /* TODO add buffering */
/*----------------------------------------------------------------------------*/
enum blockPriority
{
    B_PRIORITY_LOWEST = 0,
    B_PRIORITY_LOW,
    B_PRIORITY_MEDIUM,
    B_PRIORITY_HIGH,
    B_PRIORITY_HIGHEST
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
struct BlockDevice
{
  struct Interface *iface;
  enum ifResult (*read)(struct BlockDevice *, uint32_t, uint8_t *, uint8_t,
      enum blockPriority);
  enum ifResult (*write)(struct BlockDevice *, uint32_t, const uint8_t *,
      uint8_t, enum blockPriority);
  uint8_t *buffer;
  /* Device-specific data */
  void *data;

  uint8_t type;
  uint32_t offset;
  uint32_t size;
};
/*------------------------------------------------------------------------------*/
enum ifResult blockRead(struct BlockDevice *, uint32_t, uint8_t *, uint8_t,
    enum blockPriority);
enum ifResult blockWrite(struct BlockDevice *, uint32_t, const uint8_t *, uint8_t,
    enum blockPriority);
/*------------------------------------------------------------------------------*/
#endif /* BDEV_H_ */
