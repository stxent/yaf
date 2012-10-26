/*
 * bdev.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BDEV_H_
#define BDEV_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
#define SECTOR_POW      9 /* Sector size in power of 2 */
#define SECTOR_SIZE     (1 << SECTOR_POW) /* Sector size in bytes */
#define FS_BUFFER       (SECTOR_SIZE * 1) /* TODO add buffering */
/*----------------------------------------------------------------------------*/
//FIXME remove
enum fsResult
{
    FS_OK = 0,
    FS_ERROR,
    FS_DEVICE_ERROR,
    FS_WRITE_ERROR,
    FS_READ_ERROR,
    FS_EOF,
    FS_NOT_FOUND
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
struct FsDevice
{
  struct Interface *iface;
  enum fsResult (*read)(struct FsDevice *, uint32_t, uint8_t *, uint8_t);
  enum fsResult (*write)(struct FsDevice *, uint32_t, const uint8_t *, uint8_t);
  uint8_t *buffer;

  uint8_t type;
  uint32_t offset;
  uint32_t size;
};
/*------------------------------------------------------------------------------*/
#endif /* BDEV_H_ */
