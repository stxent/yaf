#include "interface.h"
#include "io.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
enum fsResult mmdRead(struct FsDevice *, uint32_t, uint8_t *, uint8_t);
enum fsResult mmdWrite(struct FsDevice *, uint32_t, const uint8_t *, uint8_t);
/*----------------------------------------------------------------------------*/
enum fsResult mmdOpen(struct FsDevice *dev, struct Interface *iface,
    uint8_t *buffer)
{
  dev->iface = iface;
  dev->read = mmdRead;
  dev->write = mmdWrite;
  dev->buffer = buffer;
  dev->type = 0;
  dev->offset = 0;
  dev->size = 0xFFFFFFFF;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
void mmdClose(struct FsDevice *dev)
{
  dev->iface = 0;
  dev->buffer = 0;
}
/*----------------------------------------------------------------------------*/
enum fsResult mmdRead(struct FsDevice *dev, uint32_t index, uint8_t *data,
    uint8_t count)
{
  ptrSize pos;
  if (index + count > dev->size)
    return FS_ERROR;
  pos = (index + dev->offset) << SECTOR_POW;
  ifWrite(dev->iface, (const uint8_t *)&pos, sizeof(ptrSize));
  ifRead(dev->iface, data, SECTOR_SIZE * count);
  #ifdef DEBUG
    printf("mmaped_dev: fetched sector: %d\n", (unsigned int)index);
  #endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult mmdWrite(struct FsDevice *dev, uint32_t index,
    const uint8_t *data, uint8_t count)
{
  ptrSize pos;
  if (index + count > dev->size)
    return FS_ERROR;
  pos = (index + dev->offset) << SECTOR_POW;
  ifWrite(dev->iface, (uint8_t *)&pos, sizeof(ptrSize));
  ifWrite(dev->iface, data, SECTOR_SIZE * count);
  #ifdef DEBUG
    printf("mmaped_dev: written sector: %d\n", (unsigned int)index);
  #endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult mmdReadTable(struct FsDevice *dev, uint32_t sector, uint8_t index)
{
//   uint8_t *ptr;
// 
//   if (rawRead(dev, sector))
    return 1;
//   if (*(uint16_t *)(dev->buffer + 0x01FE) != 0xAA55)
//     return 1;
// 
//   ptr = dev->buffer + 0x01BE + (index << 4); /* Pointer to partition entry */
//   if (!*(uint8_t *)(ptr + 0x04)) /* Empty entry */
//     return 1;
//   dev->type = *(uint8_t *)(ptr + 0x04); /* File system descriptor */
//   dev->offset = *(uint32_t *)(ptr + 0x08);
//   dev->size = *(uint32_t *)(ptr + 0x0C);
//   return 0;
}
