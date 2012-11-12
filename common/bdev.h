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
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512 */
#ifndef SECTOR_POW
#define SECTOR_POW      (9) /* Sector size in power of 2 */
#endif /* SECTOR_POW */
/*----------------------------------------------------------------------------*/
#define SECTOR_SIZE     (1 << SECTOR_POW) /* Sector size in bytes */
/*----------------------------------------------------------------------------*/
struct BlockDevice
{
  struct Interface *iface;
  enum result (*read)(struct BlockDevice *, uint32_t, uint8_t *, uint8_t);
  enum result (*write)(struct BlockDevice *, uint32_t, const uint8_t *,
      uint8_t);
  void (*deinit)(struct BlockDevice *);
  uint8_t *buffer;
  /* Device-specific data */
  void *data;
};
/*------------------------------------------------------------------------------*/
enum result blockRead(struct BlockDevice *, uint32_t, uint8_t *, uint8_t);
enum result blockWrite(struct BlockDevice *, uint32_t, const uint8_t *, uint8_t);
void blockDeinit(struct BlockDevice *);
/*------------------------------------------------------------------------------*/
#endif /* BDEV_H_ */
