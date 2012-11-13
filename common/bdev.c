/*
 * bdev.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "bdev.h"
/*----------------------------------------------------------------------------*/
enum result ifBlockRead(struct BlockInterface *iface, uint32_t pos,
    uint8_t *buf, uint8_t cnt)
{
  return iface->type->blockRead ?
      iface->type->blockRead(iface, pos, buf, cnt) : E_ERROR;
}
/*----------------------------------------------------------------------------*/
enum result ifBlockWrite(struct BlockInterface *iface, uint32_t pos,
    const uint8_t *buf, uint8_t cnt)
{
  return iface->type->blockWrite ?
      iface->type->blockWrite(iface, pos, buf, cnt) : E_ERROR;
}
