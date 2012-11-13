#include <stdlib.h>
/*----------------------------------------------------------------------------*/
#include "interface.h"
#include "io.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
// #undef DEBUG
/*----------------------------------------------------------------------------*/
/* Memory mapped device handle */
struct Mmd
{
  struct BlockInterface parent;

  struct Interface *stream;
  uint8_t type; /* Partition type */
  uint32_t offset; /* Partition offset from start */
  uint32_t size; /* Number of blocks in partition */
  uint32_t current; /* Current block, value -1 is reserved */
  long readCount, writeCount, readExcess;
};
/*----------------------------------------------------------------------------*/
enum result mmdInit(struct Interface *, const void *);
void mmdDeinit(struct Interface *);
enum result mmdRead(struct BlockInterface *, uint32_t, uint8_t *, uint8_t);
enum result mmdWrite(struct BlockInterface *, uint32_t, const uint8_t *, uint8_t);
/*----------------------------------------------------------------------------*/
static const struct BlockInterfaceClass mmdTable = {
    .parent = {
        .size = sizeof(struct Mmd),
        .init = mmdInit,
        .deinit = mmdDeinit,

        .start = 0,
        .stop = 0,
        .read = 0,
        .write = 0,
        .getopt = 0,
        .setopt = 0
    },
    .blockRead = mmdRead,
    .blockWrite = mmdWrite
};
/*----------------------------------------------------------------------------*/
const struct BlockInterfaceClass *Mmd = &mmdTable;
/*----------------------------------------------------------------------------*/
enum result mmdInit(struct Interface *iface, const void *data)
{
  const struct mmdConfig *config = (const struct mmdConfig *)data;
  struct Mmd *pdata = (struct Mmd *)iface;

  pdata->stream = config->stream;

  pdata->current = (uint32_t)-1;
  pdata->offset = 0;
  pdata->type = 0;
  pdata->size = 0xFFFFFFFF;
  pdata->readCount = 0;
  pdata->writeCount = 0;
  pdata->readExcess = 0;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
void mmdDeinit(struct Interface *iface)
{
//   iface->iface = 0;
}
/*----------------------------------------------------------------------------*/
enum result mmdRead(struct BlockInterface *iface, uint32_t pos, uint8_t *data,
    uint8_t cnt)
{
  struct Mmd *pdata = (struct Mmd *)iface;
  uint64_t memPtr;

  if (pdata->current == pos && cnt == 1)
  {
    pdata->readExcess++;
    return E_OK;
  }
  if (pos + cnt > pdata->size)
    return E_ERROR;
  memPtr = (pos + pdata->offset) << SECTOR_POW;
  ifSetOpt(pdata->stream, IF_ADDRESS, &memPtr);
  ifRead(pdata->stream, data, SECTOR_SIZE * cnt);
#ifdef DEBUG
  printf("mmaped_dev: read sector: %u\n", pos);
#endif
  if (cnt == 1)
    pdata->current = pos;
  pdata->readCount++;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum result mmdWrite(struct BlockInterface *iface, uint32_t pos,
    const uint8_t *data, uint8_t cnt)
{
  struct Mmd *pdata = (struct Mmd *)iface;
  uint64_t memPtr;

  if (pos + cnt > pdata->size)
    return E_ERROR;
  memPtr = (pos + pdata->offset) << SECTOR_POW;
  ifSetOpt(pdata->stream, IF_ADDRESS, &memPtr);
  ifWrite(pdata->stream, data, SECTOR_SIZE * cnt);
#ifdef DEBUG
  printf("mmaped_dev: write sector: %u\n", pos);
#endif
  pdata->writeCount++;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum result mmdReadTable(struct BlockInterface *iface, uint32_t sector,
    uint8_t index)
{
  struct Mmd *pdata = (struct Mmd *)iface;
  return E_ERROR;
//   uint8_t *ptr;
// //   uint32_t prevOffset = pdata->offset; /* Save previous value */
// 
//   pdata->offset = 0;
//   if (dev->read(dev, sector, dev->buffer, 1))
//     return E_ERROR;
//   if (*(uint16_t *)(dev->buffer + 0x01FE) != 0xAA55)
//   {
// #ifdef DEBUG
//     printf("mmaped_dev: sector is not MBR\n");
// #endif
//     return E_ERROR;
//   }
// 
//   ptr = dev->buffer + 0x01BE + (index << 4); /* Pointer to partition entry */
//   if (!*(uint8_t *)(ptr + 0x04)) /* Empty entry */
//   {
// #ifdef DEBUG
//     printf("mmaped_dev: MBR entry on position %u is empty\n",
//         (unsigned int)index);
// #endif
//     return E_ERROR;
//   }
//   pdata->type = *(uint8_t *)(ptr + 0x04); /* File system descriptor */
//   pdata->offset = *(uint32_t *)(ptr + 0x08);
//   pdata->size = *(uint32_t *)(ptr + 0x0C);
//   pdata->current = (uint32_t)-1;
// #ifdef DEBUG
//   printf("mmaped_dev: MBR entry found: type 0x%02X, offset %u, size %u\n",
//       (unsigned int)pdata->type, pdata->offset, pdata->size);
// #endif
//   return E_OK;
}
/*----------------------------------------------------------------------------*/
uint8_t mmdGetType(struct BlockInterface *dev)
{
//   struct Mmd *pdata = (struct Mmd *)dev->data;
//   return pdata->type;
  return 0x00;
}
