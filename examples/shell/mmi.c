#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
#include "mmi.h"
/*----------------------------------------------------------------------------*/
struct MemMapedInterface
{
  void *data;
  int file;
  struct stat info;
  ptrSize position;
};
/*----------------------------------------------------------------------------*/
unsigned int mmiRead(struct Interface *, uint8_t *, unsigned int);
unsigned int mmiWrite(struct Interface *, const uint8_t *, unsigned int);
/*----------------------------------------------------------------------------*/
enum ifResult mmiInit(struct Interface *iface, const void *cdata)
{
  struct MmiConfig *config = (struct MmiConfig *)cdata;
  struct MemMapedInterface *dev = malloc(sizeof(struct MemMapedInterface));
  iface->dev = (void *)dev;
  /* Initialize interface functions */
  iface->start = 0;
  iface->stop = 0;
  iface->read = mmiRead;
  iface->write = mmiWrite;
  iface->getopt = 0;
  iface->setopt = 0;

  dev->position = 0;
  dev->file = open(config->path, O_RDWR);
#ifdef DEBUG
  printf("mmaped_io: opening file: %s\n", config->path);
#endif
  if (!dev->file)
  {
    free(dev);
    return IF_ERROR;
  }
  if (fstat(dev->file, &(dev->info)) == -1 || dev->info.st_size == 0)
  {
    free(dev);
    return IF_ERROR;
  }
  dev->data = mmap(0, dev->info.st_size, PROT_WRITE, MAP_SHARED, dev->file, 0);
  if (dev->data == MAP_FAILED)
  {
    free(dev);
    return IF_ERROR;
  }

  return IF_OK;
}
/*----------------------------------------------------------------------------*/
unsigned int mmiRead(struct Interface *iface, uint8_t *buffer,
    unsigned int length)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface->dev;
  memcpy(buffer, (uint8_t *)dev->data + dev->position, length);
  return length;
}
/*----------------------------------------------------------------------------*/
unsigned int mmiWrite(struct Interface *iface, const uint8_t *buffer,
    unsigned int length)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface->dev;
  if (length == sizeof(ptrSize))
  {
    dev->position = *(ptrSize *)buffer;
#ifdef DEBUG
    printf("mmaped_io: position set to 0x%08X\n", (unsigned int)dev->position);
#endif
    return sizeof(ptrSize);
  }
  else
  {
    memcpy((uint8_t *)dev->data + dev->position, buffer, length);
    return length;
  }
}
/*----------------------------------------------------------------------------*/
void mmiDeinit(struct Interface *iface)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface->dev;
  munmap(dev->data, dev->info.st_size);
  close(dev->file);
  free(iface->dev);
}
