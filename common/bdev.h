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
struct BlockInterface;
/*----------------------------------------------------------------------------*/
struct BlockInterfaceClass
{
  struct InterfaceClass parent;

  enum result (*blockRead)(struct BlockInterface *, uint32_t, uint8_t *,
      uint8_t);
  enum result (*blockWrite)(struct BlockInterface *, uint32_t, const uint8_t *,
      uint8_t);
};
/*------------------------------------------------------------------------------*/
struct BlockInterface
{
  const struct BlockInterfaceClass *type;
};
/*------------------------------------------------------------------------------*/
enum result ifBlockRead(struct BlockInterface *, uint32_t, uint8_t *, uint8_t);
enum result ifBlockWrite(struct BlockInterface *, uint32_t, const uint8_t *,
    uint8_t);
/*------------------------------------------------------------------------------*/
#endif /* BDEV_H_ */
