#include "mem.h"
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
//---------------------------------------------------------------------------
#undef DEBUG
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
char devicePath[255];
static void *dataHandler;
static int fileHandler;
static struct stat fileStat;
long readCount = 0, writeCount = 0;
//---------------------------------------------------------------------------
uint8_t rawRead(struct sDevice *dev, uint32_t sector)
{
  memcpy(dev->buffer, ((uint8_t *)dataHandler) + (sector << BLOCK_SIZE_POW), (1 << BLOCK_SIZE_POW));
  return 0;
}
//---------------------------------------------------------------------------
uint8_t rawWrite(struct sDevice *dev, uint32_t sector)
{
  memcpy(((uint8_t *)dataHandler) + (sector << BLOCK_SIZE_POW), dev->buffer, (1 << BLOCK_SIZE_POW));
  return 0;
}
//---------------------------------------------------------------------------
uint8_t sRead(struct sDevice *dev, uint8_t *data, uint32_t index)
{
  if (index >= dev->size)
    return 1;
  memcpy(data, ((uint8_t *)dataHandler) + ((index + dev->offset) << BLOCK_SIZE_POW), (1 << BLOCK_SIZE_POW));
  readCount++;
  #ifdef DEBUG
    cout << "--------Fetched sector: " << dec << (long)index << endl;
  #endif
  return 0;
}
//---------------------------------------------------------------------------
uint8_t sWrite(struct sDevice *dev, const uint8_t *data, uint32_t index)
{
  if (index >= dev->size)
    return 1;
  memcpy(((uint8_t *)dataHandler) + ((index + dev->offset) << BLOCK_SIZE_POW), data, (1 << BLOCK_SIZE_POW));
  writeCount++;
  #ifdef DEBUG
    cout << "--------Wrote sector: " << dec << (long)index << endl;
  #endif
  return 0;
}
//---------------------------------------------------------------------------
uint8_t sOpen(struct sDevice *dev, uint8_t *memBuffer, const char *fileName)
{
  dev->buffer = memBuffer;
  dev->type = 0;
  dev->offset = 0;
  dev->size = 0xFFFFFFFF;
  fileHandler = open(fileName, O_RDWR);
  if (!fileHandler)
    return 1;
  if (fstat(fileHandler, &(fileStat)) == -1 || fileStat.st_size == 0)
    return 1;
  dataHandler = mmap(0, fileStat.st_size, PROT_WRITE, MAP_SHARED, fileHandler, 0);
  if (dataHandler == MAP_FAILED)
    return 1;
  return 0;
}
//---------------------------------------------------------------------------
uint8_t sReadTable(struct sDevice *dev, uint32_t sector, uint8_t index)
{
  uint8_t *ptr;

  if (rawRead(dev, sector))
    return 1;
  if (*((uint16_t *)(dev->buffer + 0x01FE)) != 0xAA55)
    return 1;

  ptr = dev->buffer + 0x01BE + (index << 4); //Pointer to partition entry
  if (!*((uint8_t *)(ptr + 0x04))) //Empty entry
    return 1;
  dev->type = *((uint8_t *)(ptr + 0x04)); //File system descriptor
  dev->offset = *((uint32_t *)(ptr + 0x08));
  dev->size = *((uint32_t *)(ptr + 0x0C));
  return 0;
}
//---------------------------------------------------------------------------
uint8_t sClose(struct sDevice *dev)
{
  if (munmap(dataHandler, fileStat.st_size) == -1)
    return 1;
  close(fileHandler);
  return 0;
}
//---------------------------------------------------------------------------
