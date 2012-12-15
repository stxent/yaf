/*
 * mmi.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

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
#include "mutex.h"
/*----------------------------------------------------------------------------*/
struct Mmi
{
  struct Interface parent;
  Mutex lock;
  void *data;
  int file;
  struct stat info;
  uint64_t position;
};
/*----------------------------------------------------------------------------*/
static enum result mmiInit(struct Interface *, const void *);
static void mmiDeinit(struct Interface *);
static uint32_t mmiRead(struct Interface *, uint8_t *, uint32_t);
static uint32_t mmiWrite(struct Interface *, const uint8_t *, uint32_t);
static enum result mmiGetOpt(struct Interface *, enum ifOption, void *);
static enum result mmiSetOpt(struct Interface *, enum ifOption, const void *);
/*----------------------------------------------------------------------------*/
static const struct InterfaceClass mmiTable = {
    .size = sizeof(struct Mmi),
    .init = mmiInit,
    .deinit = mmiDeinit,

    .read = mmiRead,
    .write = mmiWrite,
    .getopt = mmiGetOpt,
    .setopt = mmiSetOpt
};
/*----------------------------------------------------------------------------*/
const struct InterfaceClass *Mmi = &mmiTable;
/*----------------------------------------------------------------------------*/
static enum result mmiInit(struct Interface *interface, const void *configPtr)
{
  const char *path = (const char *)configPtr;
  struct Mmi *dev = (struct Mmi *)interface;

  if (!path)
    return E_ERROR;
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
static uint32_t mmiRead(struct Interface *interface, uint8_t *buffer,
    uint32_t length)
{
  struct Mmi *dev = (struct Mmi *)interface;

  mutexLock(&dev->lock);
  memcpy(buffer, (uint8_t *)dev->data + dev->position, length);
  mutexUnlock(&dev->lock);
  return length;
}
/*----------------------------------------------------------------------------*/
static uint32_t mmiWrite(struct Interface *interface, const uint8_t *buffer,
    uint32_t length)
{
  struct Mmi *dev = (struct Mmi *)interface;

  mutexLock(&dev->lock);
  memcpy((uint8_t *)dev->data + dev->position, buffer, length);
  mutexUnlock(&dev->lock);
  return length;
}
/*----------------------------------------------------------------------------*/
static enum result mmiGetOpt(struct Interface *interface, enum ifOption option,
    void *data)
{
  struct Mmi *dev = (struct Mmi *)interface;

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
static enum result mmiSetOpt(struct Interface *interface, enum ifOption option,
    const void *data)
{
  struct Mmi *dev = (struct Mmi *)interface;

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
static void mmiDeinit(struct Interface *interface)
{
  struct Mmi *dev = (struct Mmi *)interface;
  munmap(dev->data, dev->info.st_size);
  close(dev->file);
}
