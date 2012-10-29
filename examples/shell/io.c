#include <stdlib.h>
/*----------------------------------------------------------------------------*/
#include "interface.h"
#include "io.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
/* Memory mapped device handle */
struct mmdHandle
{
  uint8_t type; /* Partition type */
  uint32_t offset; /* Partition offset from start */
  uint32_t size; /* Number of blocks in partition */
  uint32_t current; /* Current block, value -1 is reserved */
  long readCount, writeCount, readExcess;
};
/*----------------------------------------------------------------------------*/
enum ifResult mmdRead(struct BlockDevice *, uint32_t, uint8_t *, uint8_t,
    enum blockPriority);
enum ifResult mmdWrite(struct BlockDevice *, uint32_t, const uint8_t *, uint8_t,
    enum blockPriority);
void mmdDeinit(struct BlockDevice *);
/*----------------------------------------------------------------------------*/
enum ifResult mmdInit(struct BlockDevice *dev, struct Interface *iface)
{
  struct mmdHandle *pdata;
  dev->iface = iface;
  dev->read = mmdRead;
  dev->write = mmdWrite;
  dev->deinit = mmdDeinit;
#ifndef MMD_STATIC_ALLOC
  dev->buffer = malloc(SECTOR_SIZE);
#ifdef DEBUG
  printf("mmaped_dev: dynamically allocated buffer, size %u, address %08X\n",
      SECTOR_SIZE, (unsigned int)dev->buffer);
#endif /* DEBUG */
#endif /* MMD_STATIC_ALLOC */
  if (!dev->buffer)
    return IF_ERROR;

  pdata = malloc(sizeof(struct mmdHandle));
  if (!pdata)
  {
    free(dev->buffer);
    return IF_ERROR;
  }
  pdata->current = (uint32_t)-1;
  pdata->offset = 0;
  pdata->type = 0;
  pdata->size = 0xFFFFFFFF;
  pdata->readCount = 0;
  pdata->writeCount = 0;
  pdata->readExcess = 0;

  dev->data = (void *)pdata;
  return IF_OK;
}
/*----------------------------------------------------------------------------*/
void mmdDeinit(struct BlockDevice *dev)
{
  free(dev->data);
  dev->iface = 0;
  dev->buffer = 0;
}
/*----------------------------------------------------------------------------*/
enum ifResult mmdRead(struct BlockDevice *dev, uint32_t pos, uint8_t *data,
    uint8_t cnt, enum blockPriority priority)
{
  struct mmdHandle *pdata = (struct mmdHandle *)dev->data;
  ptrSize memPtr;

  if (pdata->current == pos && cnt == 1)
  {
    pdata->readExcess++;
    return IF_OK;
  }
  if (pos + cnt > pdata->size)
    return IF_ERROR;
  memPtr = (pos + pdata->offset) << SECTOR_POW;
  ifWrite(dev->iface, (const uint8_t *)&memPtr, sizeof(ptrSize));
  ifRead(dev->iface, data, SECTOR_SIZE * cnt);
#ifdef DEBUG
  printf("mmaped_dev: fetched sector: %u\n", pos);
#endif
  if (cnt == 1)
    pdata->current = pos;
  pdata->readCount++;
  return IF_OK;
}
/*----------------------------------------------------------------------------*/
enum ifResult mmdWrite(struct BlockDevice *dev, uint32_t pos,
    const uint8_t *data, uint8_t cnt, enum blockPriority priority)
{
  struct mmdHandle *pdata = (struct mmdHandle *)dev->data;
  ptrSize memPtr;

  if (pos + cnt > pdata->size)
    return IF_ERROR;
  memPtr = (pos + pdata->offset) << SECTOR_POW;
  ifWrite(dev->iface, (uint8_t *)&memPtr, sizeof(ptrSize));
  ifWrite(dev->iface, data, SECTOR_SIZE * cnt);
#ifdef DEBUG
  printf("mmaped_dev: written sector: %u\n", pos);
#endif
  pdata->writeCount++;
  return IF_OK;
}
/*----------------------------------------------------------------------------*/
enum ifResult mmdReadTable(struct BlockDevice *dev, uint32_t sector,
    uint8_t index)
{
  struct mmdHandle *pdata = (struct mmdHandle *)dev->data;
  uint8_t *ptr;
//   uint32_t prevOffset = pdata->offset; /* Save previous value */

  pdata->offset = 0;
  if (dev->read(dev, sector, dev->buffer, 1, B_PRIORITY_LOW))
    return IF_ERROR;
  if (*(uint16_t *)(dev->buffer + 0x01FE) != 0xAA55)
  {
#ifdef DEBUG
    printf("mmaped_dev: sector is not MBR\n");
#endif
    return IF_ERROR;
  }

  ptr = dev->buffer + 0x01BE + (index << 4); /* Pointer to partition entry */
  if (!*(uint8_t *)(ptr + 0x04)) /* Empty entry */
  {
#ifdef DEBUG
    printf("mmaped_dev: MBR entry on position %u is empty\n",
        (unsigned int)index);
#endif
    return IF_ERROR;
  }
  pdata->type = *(uint8_t *)(ptr + 0x04); /* File system descriptor */
  pdata->offset = *(uint32_t *)(ptr + 0x08);
  pdata->size = *(uint32_t *)(ptr + 0x0C);
  pdata->current = (uint32_t)-1;
#ifdef DEBUG
  printf("mmaped_dev: MBR entry found: type 0x%02X, offset %u, size %u\n",
      (unsigned int)pdata->type, pdata->offset, pdata->size);
#endif
  return IF_OK;
}
/*----------------------------------------------------------------------------*/
uint8_t mmdGetType(struct BlockDevice *dev)
{
  struct mmdHandle *pdata = (struct mmdHandle *)dev->data;
  return pdata->type;
}
