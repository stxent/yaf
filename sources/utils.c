/*
 * utils.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include <yaf/fat32_defs.h>
#include <yaf/fat32_helpers.h>
#include <yaf/utils.h>
#include <xcore/interface.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
static enum Result readSector(void *, uint32_t, uint8_t *, size_t);
/*----------------------------------------------------------------------------*/
static enum Result readSector(void *interface, uint32_t sector,
    uint8_t *buffer, size_t length)
{
  const uint64_t position = (uint64_t)sector << SECTOR_EXP;
  enum Result res;

  ifSetParam(interface, IF_ACQUIRE, NULL);

  res = ifSetParam(interface, IF_POSITION_64, &position);
  if (res == E_OK)
  {
    if (ifRead(interface, buffer, length) != length)
      res = ifGetParam(interface, IF_STATUS, NULL);
  }

  ifSetParam(interface, IF_RELEASE, NULL);
  return res;
}
/*----------------------------------------------------------------------------*/
FsCapacity fat32GetCapacity(const void *object)
{
  const struct FatHandle * const handle = object;
  const uint32_t clusterSize = SECTOR_SIZE * (1 << handle->clusterSize);
  const uint32_t clustersTotal = handle->clusterCount - CLUSTER_OFFSET;

  return (FsCapacity)clusterSize * clustersTotal;
}
/*----------------------------------------------------------------------------*/
size_t fat32GetClusterSize(const void *object)
{
  const struct FatHandle * const handle = object;
  return (size_t)(SECTOR_SIZE * (1 << handle->clusterSize));
}
/*----------------------------------------------------------------------------*/
enum Result fat32GetUsage(void *object, void *arena, size_t size,
    FsCapacity *result)
{
  const struct FatHandle * const handle = object;
  uint8_t * const buffer = arena;
  uint32_t cluster = 0;
  uint32_t used = 0;
  enum Result res = E_OK;

  while (cluster < handle->clusterCount)
  {
    if ((cluster & (size / sizeof(uint32_t) - 1)) == 0)
    {
      if (cluster == 0)
        cluster = CLUSTER_OFFSET;

      const uint32_t sector = handle->tableSector + (cluster >> CELL_COUNT_EXP);

      res = readSector(handle->interface, sector, buffer, size);
      if (res != E_OK)
        break;
    }

    uint32_t offset = (cluster & (size / sizeof(uint32_t) - 1)) << 2;
    uint32_t value;

    memcpy(&value, buffer + offset, sizeof(value));
    value = fromLittleEndian32(value);

    if (!isClusterFree(value))
      ++used;

    ++cluster;
  }

  if (res == E_OK)
  {
    const uint32_t count = 1U << (handle->clusterSize + SECTOR_EXP);
    *result = (FsCapacity)count * used;
  }

  return res;
}
/*----------------------------------------------------------------------------*/
enum Result fat32MakeFs(void *interface, const struct Fat32FsConfig *config,
    void *arena, size_t size)
{
  static const size_t clustersPerSector = 1 << CELL_COUNT_EXP;
  static const uint32_t reservedSectorsDefault = 32;

  if (arena != NULL && size < (1 << SECTOR_EXP))
    return E_MEMORY;

  uint64_t partitionSize;
  enum Result res;

  if ((res = ifGetParam(interface, IF_SIZE_64, &partitionSize)) != E_OK)
    return res;
  partitionSize >>= SECTOR_EXP;

  const uint32_t reservedSectors = config->reserved ?
      config->reserved : reservedSectorsDefault;
  const uint32_t sectorsPerCluster = config->cluster >> SECTOR_EXP;
  const uint32_t sectorCount = MIN(partitionSize, UINT32_MAX);

  if (sectorCount < reservedSectors + sectorsPerCluster + config->tables)
    return E_VALUE;

  const uint32_t clusterCount =
      ((sectorCount - reservedSectors) * clustersPerSector
          - config->tables * (clustersPerSector - 1))
      / (config->tables + sectorsPerCluster * clustersPerSector);

  static const uint64_t bootPosition = 0;
  struct BootSectorImage bImage;

  memset(&bImage, 0, sizeof(bImage));

  /* Magic numbers */
  bImage.mediaDescriptor = 0xF8;
  bImage.logicalNumber = 0x80;
  bImage.extendedSignature = 0x29;

  bImage.bytesPerSector = 1 << SECTOR_EXP;
  bImage.sectorsPerCluster = sectorsPerCluster;

  /* Jump code and NOP */
  memcpy(bImage.jumpCode, "\xEB\x58\x90", 3);
  /* OEM identifier */
  memcpy(bImage.oemName, "libyaf", 6);
  /* Reserved sectors in front of the FAT */
  bImage.reservedSectors = reservedSectors;
  /* Number of FAT copies */
  bImage.tableCount = config->tables;
  /* Sectors per partition */
  bImage.sectorsPerPartition = sectorCount;
  /* Sectors per FAT record */
  bImage.sectorsPerTable =
      (clusterCount + (clustersPerSector - 1)) / clustersPerSector;
  /* Root directory cluster */
  bImage.rootCluster = CLUSTER_OFFSET;
  /* Information sector number */
  bImage.infoSector = 1;
  /* Backup boot sector */
  bImage.backupSector = 0;
  /* Volume name */
  memcpy(bImage.volumeName, "NO NAME", 7);
  /* FAT name */
  memcpy(bImage.fatName, "FAT32", 5);
  /* Boot Record signature */
  bImage.bootSignature = TO_BIG_ENDIAN_16(0x55AA);

  if ((res = ifSetParam(interface, IF_POSITION_64, &bootPosition)) != E_OK)
    return res;
  if (ifWrite(interface, &bImage, sizeof(bImage)) != sizeof(bImage))
    return ifGetParam(interface, IF_STATUS, NULL);

  static const uint64_t infoPosition = 1 << SECTOR_EXP;
  struct InfoSectorImage iImage;

  memset(&iImage, 0, sizeof(iImage));

  iImage.firstSignature = TO_BIG_ENDIAN_32(0x52526141UL);
  iImage.infoSignature = TO_BIG_ENDIAN_32(0x72724161UL);
  iImage.freeClusters = clusterCount - 3;
  iImage.lastAllocated = bImage.rootCluster;
  iImage.bootSignature = TO_BIG_ENDIAN_16(0x55AA);

  if ((res = ifSetParam(interface, IF_POSITION_64, &infoPosition)) != E_OK)
    return res;
  if (ifWrite(interface, &iImage, sizeof(iImage)) != sizeof(iImage))
    return ifGetParam(interface, IF_STATUS, NULL);

  uint8_t buffer[1 << SECTOR_EXP];

  /* Format FAT tables */
  for (size_t fat = 0; fat < bImage.tableCount; ++fat)
  {
    size_t tableArenaSize;
    uint8_t *tableArena;

    if (arena != NULL)
    {
      tableArena = arena;
      tableArenaSize = size & ~((1 << SECTOR_EXP) - 1);
    }
    else
    {
      tableArena = buffer;
      tableArenaSize = 1 << SECTOR_EXP;
    }

    for (uint32_t i = 0; i < bImage.sectorsPerTable;)
    {
      if (i == 0)
      {
        static const uint32_t pattern[] = {
            TO_LITTLE_ENDIAN_32(CLUSTER_EOC_VAL),
            TO_LITTLE_ENDIAN_32(CLUSTER_RES_VAL),
            TO_LITTLE_ENDIAN_32(CLUSTER_EOC_VAL)
        };

        memcpy(tableArena, pattern, sizeof(pattern));
        memset(tableArena + sizeof(pattern), 0,
            tableArenaSize - sizeof(pattern));
      }

      const uint32_t sectors = i + bImage.reservedSectors
          + bImage.sectorsPerTable * fat;
      const uint64_t position = (uint64_t)sectors << SECTOR_EXP;

      if ((res = ifSetParam(interface, IF_POSITION_64, &position)) != E_OK)
        return res;
      if (ifWrite(interface, tableArena, tableArenaSize) != tableArenaSize)
        return ifGetParam(interface, IF_STATUS, NULL);

      if (i == 0)
        memset(tableArena, 0, tableArenaSize);

      i += tableArenaSize >> SECTOR_EXP;
    }
  }

  /* Clear root cluster */
  memset(buffer, 0, sizeof(buffer));

  for (size_t i = 0; i < sectorsPerCluster; ++i)
  {
    /* Add volume label */
    if (i == 0 && config->label != NULL)
    {
      struct DirEntryImage * const entry = (struct DirEntryImage *)buffer;
      memcpy(entry->filename, config->label, strlen(config->label));
      entry->flags = FLAG_VOLUME;
    }

    const uint32_t sectors = i + bImage.reservedSectors
        + bImage.sectorsPerTable * bImage.tableCount;
    const uint64_t position = (uint64_t)sectors << SECTOR_EXP;

    if ((res = ifSetParam(interface, IF_POSITION_64, &position)) != E_OK)
      return res;
    if (ifWrite(interface, buffer, sizeof(buffer)) != sizeof(buffer))
      return ifGetParam(interface, IF_STATUS, NULL);
  }

  return E_OK;
}
