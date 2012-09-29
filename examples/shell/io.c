#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "io.h"
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
static void *dataHandler;
static int fileHandler;
static struct stat fileStat;
long readCount = 0, writeCount = 0;
/*----------------------------------------------------------------------------*/
enum fsResult rawRead(struct FsDevice *dev, uint32_t sector)
{
  memcpy(dev->buffer, ((uint8_t *)dataHandler) + (sector << SECTOR_SIZE),
      (1 << SECTOR_SIZE));
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult rawWrite(struct FsDevice *dev, uint32_t sector)
{
  memcpy(((uint8_t *)dataHandler) + (sector << SECTOR_SIZE), dev->buffer,
      (1 << SECTOR_SIZE));
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult sRead(struct FsDevice *dev, uint8_t *data, uint32_t index,
    uint8_t count)
{
  if (index + count > dev->size)
    return FS_ERROR;
  memcpy(data, ((uint8_t *)dataHandler) +
      ((index + dev->offset) << SECTOR_SIZE), (1 << SECTOR_SIZE) * count);
  readCount++;
  #ifdef DEBUG
    printf("----Fetched sector: %d\n", (long)index);
  #endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult sWrite(struct FsDevice *dev, const uint8_t *data, uint32_t index,
    uint8_t count)
{
  if (index + count > dev->size)
    return FS_ERROR;
  memcpy(((uint8_t *)dataHandler) + ((index + dev->offset) << SECTOR_SIZE),
      data, (1 << SECTOR_SIZE) * count);
  writeCount++;
  #ifdef DEBUG
    printf("----Wrote sector: %d\n", (long)index);
  #endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult sOpen(struct FsDevice *dev, uint8_t *memBuffer,
    const char *fileName)
{
  dev->buffer = memBuffer;
  dev->type = 0;
  dev->offset = 0;
  dev->size = 0xFFFFFFFF;
  fileHandler = open(fileName, O_RDWR);
  if (!fileHandler)
    return FS_ERROR;
  if (fstat(fileHandler, &(fileStat)) == -1 || fileStat.st_size == 0)
    return FS_ERROR;
  dataHandler =
      mmap(0, fileStat.st_size, PROT_WRITE, MAP_SHARED, fileHandler, 0);
  if (dataHandler == MAP_FAILED)
    return FS_ERROR;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult sReadTable(struct FsDevice *dev, uint32_t sector, uint8_t index)
{
  uint8_t *ptr;

  if (rawRead(dev, sector))
    return 1;
  if (*((uint16_t *)(dev->buffer + 0x01FE)) != 0xAA55)
    return 1;

  ptr = dev->buffer + 0x01BE + (index << 4); /* Pointer to partition entry */
  if (!*((uint8_t *)(ptr + 0x04))) /* Empty entry */
    return 1;
  dev->type = *((uint8_t *)(ptr + 0x04)); /* File system descriptor */
  dev->offset = *((uint32_t *)(ptr + 0x08));
  dev->size = *((uint32_t *)(ptr + 0x0C));
  return 0;
}
/*----------------------------------------------------------------------------*/
enum fsResult sClose(struct FsDevice *dev)
{
  if (munmap(dataHandler, fileStat.st_size) == -1)
    return FS_ERROR;
  close(fileHandler);
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
