/*
 * yaf/tests/shared/virtual_mem.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "virtual_mem.h"
#include <xcore/containers/tg_list.h>
#include <xcore/memory.h>
#include <yaf/fat32_defs.h>
#include <assert.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
DEFINE_LIST(struct VirtualMemRegion, VmemRegion, vmemRegion)
/*----------------------------------------------------------------------------*/
struct VirtualMem
{
  struct Interface base;

  sem_t semaphore;

  unsigned int counter;
  uint8_t *data;
  size_t position;
  size_t size;

  VmemRegionList regions;
};
/*----------------------------------------------------------------------------*/
static bool inForbiddenRegion(struct VirtualMem *, uint64_t, uint8_t);

static enum Result vmemInit(void *, const void *);
static void vmemDeinit(void *);
static enum Result vmemGetParam(void *, enum IfParameter, void *);
static enum Result vmemSetParam(void *, enum IfParameter, const void *);
static size_t vmemRead(void *, void *, size_t);
static size_t vmemWrite(void *, const void *, size_t);
/*----------------------------------------------------------------------------*/
const struct InterfaceClass * const VirtualMem =
    &(const struct InterfaceClass){
    .size = sizeof(struct VirtualMem),
    .init = vmemInit,
    .deinit = vmemDeinit,

    .setCallback = 0,
    .getParam = vmemGetParam,
    .setParam = vmemSetParam,
    .read = vmemRead,
    .write = vmemWrite
};
/*----------------------------------------------------------------------------*/
static bool inForbiddenRegion(struct VirtualMem *dev, uint64_t position,
    uint8_t flag)
{
  VmemRegionListNode *current = vmemRegionListFront(&dev->regions);

  while (current)
  {
    const struct VirtualMemRegion * const entry = vmemRegionListData(current);

    if (position >= entry->begin && position <= entry->end
        && !(flag & entry->flags))
    {
      return true;
    }
    current = vmemRegionListNext(current);
  }

  return false;
}
/*----------------------------------------------------------------------------*/
static enum Result vmemInit(void *object, const void *configBase)
{
  const struct VirtualMemConfig * const config = configBase;
  assert(config);

  struct VirtualMem * const dev = object;
  enum Result res = E_MEMORY;

  dev->counter = 0;
  dev->position = 0;
  dev->size = config->size;

  vmemRegionListInit(&dev->regions);

  if (sem_init(&dev->semaphore, 0, 1) == 0)
  {
    if (dev->size)
    {
      if ((dev->data = malloc(dev->size)))
      {
        memset(dev->data, 0, dev->size);
        res = E_OK;
      }
      else
        sem_destroy(&dev->semaphore);
    }
    else
    {
      dev->data = 0;
      res = E_OK;
    }
  }

  return res;
}
/*----------------------------------------------------------------------------*/
static void vmemDeinit(void *object)
{
  struct VirtualMem * const dev = object;

  free(dev->data);
  sem_destroy(&dev->semaphore);
  vmemRegionListDeinit(&dev->regions);
}
/*----------------------------------------------------------------------------*/
static enum Result vmemGetParam(void *object, enum IfParameter parameter,
    void *data)
{
  struct VirtualMem * const dev = object;

  switch (parameter)
  {
    case IF_POSITION_64:
      *(uint64_t *)data = (uint64_t)dev->position;
      return E_OK;

    case IF_SIZE_64:
      if (dev->size > 0)
      {
        *(uint64_t *)data = (uint64_t)dev->size;
        return E_OK;
      }
      else
        return E_MEMORY;

    case IF_STATUS:
      return inForbiddenRegion(dev, dev->position, 0) ? E_INTERFACE : E_OK;

    default:
      return E_INVALID;
  }
}
/*----------------------------------------------------------------------------*/
static enum Result vmemSetParam(void *object, enum IfParameter parameter,
    const void *data)
{
  struct VirtualMem * const dev = object;

  switch (parameter)
  {
    case IF_POSITION_64:
    {
      const size_t position = (size_t)(*(const uint64_t *)data);

      if (position < dev->size)
      {
        const bool match = inForbiddenRegion(dev, position, VMEM_A);

        if (match && dev->counter == 0)
        {
          return E_ADDRESS;
        }
        else
        {
          if (match && dev->counter > 0)
            --dev->counter;

          dev->position = position;
          return E_OK;
        }
      }
      else
        return E_ADDRESS;
    }

    case IF_ACQUIRE:
      sem_wait(&dev->semaphore);
      return E_OK;

    case IF_RELEASE:
      sem_post(&dev->semaphore);
      return E_OK;

    default:
      return E_INVALID;
  }
}
/*----------------------------------------------------------------------------*/
static size_t vmemRead(void *object, void *buffer, size_t length)
{
  struct VirtualMem * const dev = object;
  const bool match = inForbiddenRegion(dev, dev->position, VMEM_R);

  if (match && dev->counter == 0)
    return 0;

  if (match && dev->counter > 0)
    --dev->counter;

  memcpy(buffer, dev->data + dev->position, length);
  dev->position += length;

  return length;
}
/*----------------------------------------------------------------------------*/
static size_t vmemWrite(void *object, const void *buffer, size_t length)
{
  struct VirtualMem * const dev = object;
  const bool match = inForbiddenRegion(dev, dev->position, VMEM_W);

  if (match && dev->counter == 0)
    return 0;

  if (match && dev->counter > 0)
    --dev->counter;

  memcpy(dev->data + dev->position, buffer, length);
  dev->position += length;

  return length;
}
/*----------------------------------------------------------------------------*/
uint8_t *vmemGetAddress(void *object)
{
  struct VirtualMem * const dev = object;
  return dev->data;
}
/*----------------------------------------------------------------------------*/
void vmemAddMarkedRegion(void *object, struct VirtualMemRegion region,
    bool readable, bool writable, bool addressable)
{
  struct VirtualMem * const dev = object;

  region.flags = 0;
  if (readable)
    region.flags |= VMEM_R;
  if (writable)
    region.flags |= VMEM_W;
  if (addressable)
    region.flags |= VMEM_A;

  vmemRegionListPushFront(&dev->regions, region);
}
/*----------------------------------------------------------------------------*/
void vmemAddRegion(void *object, struct VirtualMemRegion region)
{
  struct VirtualMem * const dev = object;

  region.flags = 0;
  vmemRegionListPushFront(&dev->regions, region);
}
/*----------------------------------------------------------------------------*/
void vmemClearRegions(void *object)
{
  struct VirtualMem * const dev = object;
  vmemRegionListClear(&dev->regions);
}
/*----------------------------------------------------------------------------*/
void vmemSetMatchCounter(void *object, unsigned int counter)
{
  struct VirtualMem * const dev = object;
  dev->counter = counter;
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractBootRegion(void)
{
  return (struct VirtualMemRegion){
      .begin = 0,
      .end = CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractDataRegion(const void *object)
{
  const struct VirtualMem * const dev = object;
  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)dev->data;

  const uint32_t tableSector = fromLittleEndian16(boot->reservedSectors);
  const uint32_t dataSector =
      tableSector + boot->tableCount * fromLittleEndian32(boot->tableSize);
  const uint32_t totalSectors = fromLittleEndian32(boot->partitionSize);

  return (struct VirtualMemRegion){
      .begin = (uint64_t)dataSector * CONFIG_SECTOR_SIZE,
      .end = (uint64_t)totalSectors * CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractInfoRegion(void)
{
  return (struct VirtualMemRegion){
      .begin = CONFIG_SECTOR_SIZE,
      .end = 2 * CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractNodeDataRegion(const void *object,
    const void *target, size_t index)
{
  const struct VirtualMem * const dev = object;
  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)dev->data;
  const struct FatNode * const node = target;

  const uint32_t tableSector = fromLittleEndian16(boot->reservedSectors);
  const uint32_t dataSector = tableSector
      + boot->tableCount * fromLittleEndian32(boot->tableSize);
  const uint32_t nodeSector = dataSector
      + ((node->payloadCluster) - 2) * boot->sectorsPerCluster;

  return (struct VirtualMemRegion){
      .begin = (uint64_t)(nodeSector + index) * CONFIG_SECTOR_SIZE,
      .end = (uint64_t)(nodeSector + index + 1) * CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractRootDataRegion(const void *object)
{
  const struct VirtualMem * const dev = object;
  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)dev->data;

  struct VirtualMemRegion region = vmemExtractTableRegion(dev,
      boot->tableCount);
  region.end = region.begin + boot->sectorsPerCluster * CONFIG_SECTOR_SIZE;

  return region;
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractTableRegion(const void *object, size_t table)
{
  const struct VirtualMem * const dev = object;
  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)dev->data;

  const uint32_t tableBegin = fromLittleEndian16(boot->reservedSectors);
  const uint32_t tableSectorBegin = tableBegin
      + (uint32_t)table * fromLittleEndian32(boot->tableSize);
  const uint32_t tableSectorEnd = tableBegin
      + (uint32_t)(table + 1) * fromLittleEndian32(boot->tableSize);

  return (struct VirtualMemRegion){
      .begin = (uint64_t)tableSectorBegin * CONFIG_SECTOR_SIZE,
      .end = (uint64_t)tableSectorEnd * CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
/*----------------------------------------------------------------------------*/
struct VirtualMemRegion vmemExtractTableSectorRegion(const void *object,
    size_t table, size_t index)
{
  const struct VirtualMem * const dev = object;
  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)dev->data;

  const uint32_t tableBegin = fromLittleEndian16(boot->reservedSectors);
  const uint32_t tableSectorBegin = tableBegin
      + (uint32_t)(table * fromLittleEndian32(boot->tableSize) + index);
  const uint32_t tableSectorEnd = tableBegin
      + (uint32_t)(table * fromLittleEndian32(boot->tableSize) + index + 1);

  return (struct VirtualMemRegion){
      .begin = (uint64_t)tableSectorBegin * CONFIG_SECTOR_SIZE,
      .end = (uint64_t)tableSectorEnd * CONFIG_SECTOR_SIZE - 1,
      .flags = 0
  };
}
