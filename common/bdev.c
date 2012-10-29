/*
 * bdev.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "bdev.h"
/*------------------------------------------------------------------------------*/
enum ifResult blockRead(struct BlockDevice *dev, uint32_t pos, uint8_t *buf,
    uint8_t cnt, enum blockPriority priority)
{
//   buf = (!buf) ? dev->buffer : buf;
  if (dev->read)
    return dev->read(dev, pos, buf, cnt, priority);
  else
    return IF_ERROR;
}
/*------------------------------------------------------------------------------*/
enum ifResult blockWrite(struct BlockDevice *dev, uint32_t pos,
    const uint8_t *buf, uint8_t cnt, enum blockPriority priority)
{
//   buf = (!buf) ? dev->buffer : buf;
  if (dev->write)
    return dev->write(dev, pos, buf, cnt, priority);
  else
    return IF_ERROR;
}
/*------------------------------------------------------------------------------*/
void blockDeinit(struct BlockDevice *dev)
{
  if (dev->deinit)
    dev->deinit(dev);
}
