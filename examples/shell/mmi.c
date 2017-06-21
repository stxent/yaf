/*
 * mmi.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <osw/semaphore.h>
#include <xcore/memory.h>
#include <yaf/debug.h>
#include "shell/mmi.h"
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_SECTOR
#define MMI_SECTOR_EXP CONFIG_FAT_SECTOR
#else
#define MMI_SECTOR_EXP 9
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_DEBUG
static void getSizeString(uint64_t, char *);
#endif
/*----------------------------------------------------------------------------*/
static enum Result mmiInit(void *, const void *);
static void mmiDeinit(void *);
static enum Result mmiSetCallback(void *, void (*)(void *), void *);
static enum Result mmiGetParam(void *, enum IfParameter, void *);
static enum Result mmiSetParam(void *, enum IfParameter, const void *);
static size_t mmiRead(void *, void *, size_t);
static size_t mmiWrite(void *, const void *, size_t);
/*----------------------------------------------------------------------------*/
struct Mmi
{
  struct Interface base;

  struct Semaphore semaphore;
  uint64_t position;
  uint64_t offset;
  uint64_t size;

  uint8_t *data;
  int file;
  struct stat info;

#ifdef CONFIG_MMI_STATUS
  uint64_t readCount;
  uint64_t writeCount;
  uint64_t bytesRead;
  uint64_t bytesWritten;
#endif
};
/*----------------------------------------------------------------------------*/
static const struct InterfaceClass mmiTable = {
    .size = sizeof(struct Mmi),
    .init = mmiInit,
    .deinit = mmiDeinit,

    .setCallback = mmiSetCallback,
    .getParam = mmiGetParam,
    .setParam = mmiSetParam,
    .read = mmiRead,
    .write = mmiWrite
};
/*----------------------------------------------------------------------------*/
const struct InterfaceClass * const Mmi = &mmiTable;
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MMI_STATUS
void mmiGetStatus(void *object, uint64_t *results)
{
  struct Mmi *dev = object;

  results[0] = dev->readCount;
  results[1] = dev->bytesRead;
  results[2] = dev->writeCount;
  results[3] = dev->bytesWritten;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_DEBUG
static void getSizeString(uint64_t size, char *str)
{
  static const char *suffix[] = {"KiB", "MiB", "GiB", "TiB"};
  static const unsigned short suffixCount = 4;
  unsigned short selectedSuffix = 0;
  float remainder = (float)size / 1024.0f;

  while (remainder >= 1024.0f)
  {
    ++selectedSuffix;
    remainder /= 1024.0f;
    if (selectedSuffix == suffixCount - 1)
      break;
  }
  sprintf(str, "%.2f %s", remainder, suffix[selectedSuffix]);
}
#endif
/*----------------------------------------------------------------------------*/
static enum Result mmiInit(void *object, const void *configBase)
{
  const char * const path = configBase;
  struct Mmi * const dev = object;

  if (!path)
    return E_ERROR;

  dev->position = 0;
  dev->offset = 0;
  dev->size = 0;

  dev->file = open(path, O_RDWR);
  if (!dev->file)
    return E_ERROR;
  if (fstat(dev->file, &(dev->info)) == -1 || dev->info.st_size == 0)
  {
    close(dev->file);
    return E_ERROR;
  }

  dev->data = mmap(0, dev->info.st_size, PROT_WRITE, MAP_SHARED, dev->file, 0);
  if (dev->data == MAP_FAILED)
  {
    close(dev->file);
    return E_ERROR;
  }

  dev->size = dev->info.st_size;

#ifdef CONFIG_MMI_STATUS
  dev->readCount = dev->writeCount = 0;
  dev->bytesRead = dev->bytesWritten = 0;
#endif

#ifdef CONFIG_DEBUG
  char sizeString[16];
  getSizeString(dev->size, sizeString);
  DEBUG_PRINT(0, "mmaped_io: opened file: %s, size: %s\n", path, sizeString);
#endif

  if (semInit(&dev->semaphore, 1) != E_OK)
  {
    munmap(dev->data, dev->info.st_size);
    close(dev->file);
    return E_ERROR;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void mmiDeinit(void *object)
{
  struct Mmi * const dev = object;

  munmap(dev->data, dev->info.st_size);
  close(dev->file);
  semDeinit(&dev->semaphore);
}
/*----------------------------------------------------------------------------*/
static enum Result mmiSetCallback(void *object __attribute__((unused)),
    void (*callback)(void *) __attribute__((unused)),
    void *argument __attribute__((unused)))
{
  /* Not implemented */
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static enum Result mmiGetParam(void *object, enum IfParameter parameter,
    void *data)
{
  struct Mmi * const dev = object;

  switch (parameter)
  {
    case IF_POSITION:
      *(uint64_t *)data = dev->position;
      return E_OK;

    case IF_SIZE:
      *(uint64_t *)data = dev->size;
      return E_OK;

    default:
      return E_INVALID;
  }
}
/*----------------------------------------------------------------------------*/
static enum Result mmiSetParam(void *object, enum IfParameter parameter,
    const void *data)
{
  struct Mmi * const dev = object;

  switch (parameter)
  {
    case IF_POSITION:
    {
      const uint64_t newPosition = *(const uint64_t *)data;

      if (newPosition + dev->offset < (uint64_t)dev->info.st_size)
      {
        dev->position = newPosition;
        return E_OK;
      }
      else
      {
        DEBUG_PRINT(0, "mmaped_io: address 0x%012"PRIX64" out of bounds\n",
            newPosition + dev->offset);
        return E_ADDRESS;
      }
    }

    case IF_ACQUIRE:
      semWait(&dev->semaphore);
      return E_OK;

    case IF_RELEASE:
      semPost(&dev->semaphore);
      return E_OK;

    default:
      return E_INVALID;
  }
}
/*----------------------------------------------------------------------------*/
static size_t mmiRead(void *object, void *buffer, size_t length)
{
  struct Mmi * const dev = object;

  memcpy(buffer, dev->data + dev->offset + dev->position, length);
  dev->position += length;

#ifdef CONFIG_MMI_STATUS
  dev->readCount++;
  dev->bytesRead += length;
#endif

  DEBUG_PRINT(3, "mmaped_io: read data from 0x%012"PRIX64", length %zu\n",
      dev->position, length);

  return length;
}
/*----------------------------------------------------------------------------*/
static size_t mmiWrite(void *object, const void *buffer, size_t length)
{
  struct Mmi * const dev = object;

  memcpy(dev->data + dev->offset + dev->position, buffer, length);
  dev->position += length;

#ifdef CONFIG_MMI_STATUS
  ++dev->writeCount;
  dev->bytesWritten += length;
#endif

  DEBUG_PRINT(3, "mmaped_io: write data to 0x%012"PRIX64", length %zu\n",
      dev->position, length);

  return length;
}
/*----------------------------------------------------------------------------*/
enum Result mmiSetPartition(void *object, struct MbrDescriptor *desc)
{
  const char validTypes[] = {0x0B, 0x0C, 0x1B, 0x1C, 0x00};
  struct Mmi * const dev = object;

  if (!strchr(validTypes, desc->type))
    return E_ERROR;

  dev->size = desc->size << MMI_SECTOR_EXP;
  dev->offset = desc->offset << MMI_SECTOR_EXP;

  DEBUG_PRINT(0, "mmaped_io: partition type 0x%02X, size %"PRIu32" sectors, "
      "offset %"PRIu32" sectors\n", desc->type, desc->size, desc->offset);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum Result mmiReadTable(void *object, uint32_t sector, uint8_t index,
    struct MbrDescriptor *desc)
{
  struct Mmi * const dev = object;
  uint8_t *ptr;
  uint64_t position = sector << MMI_SECTOR_EXP;
  uint8_t buffer[1 << MMI_SECTOR_EXP];

  dev->offset = 0;

  /* TODO Lock interface during table read */
  if (ifSetParam(object, IF_POSITION, &position) != E_OK)
    return E_INTERFACE;
  if (ifRead(object, buffer, sizeof(buffer)) != sizeof(buffer))
    return E_INTERFACE;
  if (fromBigEndian16(*(uint16_t *)(buffer + 0x01FE)) != 0x55AA)
    return E_ERROR;

  ptr = buffer + 0x01BE + (index << 4); /* Pointer to partition entry */
  if (!*(ptr + 0x04)) /* Empty entry */
    return E_ERROR;

  desc->type = *(ptr + 0x04); /* File system descriptor */
  desc->offset = *(uint32_t *)(ptr + 0x08);
  desc->size = *(uint32_t *)(ptr + 0x0C);

  return E_OK;
}
