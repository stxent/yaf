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
  struct Interface parent;
  void *data;
  int file;
  struct stat info;
  uint64_t position;
};
/*----------------------------------------------------------------------------*/
static enum result mmiInit(struct Interface *, const void *);
static void mmiDeinit(struct Interface *);
static unsigned int mmiRead(struct Interface *, uint8_t *, unsigned int);
static unsigned int mmiWrite(struct Interface *, const uint8_t *, unsigned int);
static enum result mmiGetOpt(struct Interface *, enum ifOption, void *);
static enum result mmiSetOpt(struct Interface *, enum ifOption, const void *);
/*----------------------------------------------------------------------------*/
static const struct InterfaceClass mmiTable = {
    .size = sizeof(struct MemMapedInterface),
    .init = mmiInit,
    .deinit = mmiDeinit,

    .start = 0,
    .stop = 0,
    .read = mmiRead,
    .write = mmiWrite,
    .getopt = mmiGetOpt,
    .setopt = mmiSetOpt
};
/*----------------------------------------------------------------------------*/
const struct InterfaceClass *Mmi = &mmiTable;
/*----------------------------------------------------------------------------*/
static enum result mmiInit(struct Interface *iface, const void *cdata)
{
  const char *path = (const char *)cdata;
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;

  dev->position = 0;
  dev->file = open(path, O_RDWR);
#ifdef DEBUG
  printf("mmaped_io: opening file: %s\n", path);
#endif
  if (!dev->file)
    return E_ERROR;
  if (fstat(dev->file, &(dev->info)) == -1 || dev->info.st_size == 0)
    return E_ERROR;
  dev->data = mmap(0, dev->info.st_size, PROT_WRITE, MAP_SHARED, dev->file, 0);
  if (dev->data == MAP_FAILED)
    return E_ERROR;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static unsigned int mmiRead(struct Interface *iface, uint8_t *buffer,
    unsigned int length)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;

  memcpy(buffer, (uint8_t *)dev->data + dev->position, length);
  return length;
}
/*----------------------------------------------------------------------------*/
static unsigned int mmiWrite(struct Interface *iface, const uint8_t *buffer,
    unsigned int length)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;

  memcpy((uint8_t *)dev->data + dev->position, buffer, length);
  return length;
}
/*----------------------------------------------------------------------------*/
static enum result mmiGetOpt(struct Interface *iface, enum ifOption option,
    void *data)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;

  switch (option)
  {
    case IF_ADDRESS:
      *(uint64_t *)data = dev->position;
      return E_OK;
    default:
      return E_ERROR;
  }
}
/*----------------------------------------------------------------------------*/
static enum result mmiSetOpt(struct Interface *iface, enum ifOption option,
    const void *data)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;

  switch (option)
  {
    case IF_ADDRESS:
      dev->position = *(uint64_t *)data;
#ifdef DEBUG
      printf("mmaped_io: position set to 0x%08X\n",
          (unsigned int)dev->position);
#endif
      return E_OK;
    default:
      return E_ERROR;
  }
}
/*----------------------------------------------------------------------------*/
static void mmiDeinit(struct Interface *iface)
{
  struct MemMapedInterface *dev = (struct MemMapedInterface *)iface;
  munmap(dev->data, dev->info.st_size);
  close(dev->file);
}
