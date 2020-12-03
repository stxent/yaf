/*
 * utils.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include <yaf/fat32_defs.h>
#include <yaf/utils.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
enum Result fat32MakeFs(struct Interface *interface,
    const struct Fat32FsConfig *config)
{
  uint64_t partitionSize;
  enum Result res;

  if ((res = ifGetParam(interface, IF_SIZE_64, &partitionSize)) != E_OK)
    return res;
  partitionSize >>= SECTOR_EXP;

  const uint32_t sectorsPerCluster = config->clusterSize >> SECTOR_EXP;
  const uint32_t sectorCount = MIN(partitionSize, UINT32_MAX);

  if (sectorCount < 2 + sectorsPerCluster + config->tableCount)
    return E_VALUE;

  const uint32_t clusterCount =
      ((sectorCount - 2) * CELL_COUNT - config->tableCount * (CELL_COUNT - 1))
          / (config->tableCount + sectorsPerCluster * CELL_COUNT);

  static const uint64_t bootPosition = 0;
  struct BootSectorImage bImage;

  memset(bImage.unused0, 0, sizeof(bImage.unused0));
  memset(bImage.oemIdentifier, 0, sizeof(bImage.oemIdentifier));
  bImage.bytesPerSector = 1 << SECTOR_EXP;
  bImage.sectorsPerCluster = sectorsPerCluster;

  /* Reserved sectors in front of the FAT */
  bImage.reservedSectors = 2;
  /* Number of FAT copies */
  bImage.tableCount = config->tableCount;
  memset(bImage.unused1, 0, sizeof(bImage.unused1));
  bImage.mediaDescriptor = 0xF8;
  memset(bImage.unused2, 0, sizeof(bImage.unused2));
  /* The number of sectors before the first FAT */
  bImage.tableOffset = 2;
  /* Sectors per partition */
  bImage.partitionSize = sectorCount;
  /* Sectors per FAT record */
  bImage.tableSize = (clusterCount + (CELL_COUNT - 1)) / CELL_COUNT;
  memset(bImage.unused3, 0, sizeof(bImage.unused3));
  /* Root directory cluster */
  bImage.rootCluster = 2;
  /* Information sector number */
  bImage.infoSector = 1;
  memset(bImage.unused4, 0, sizeof(bImage.unused4));
  bImage.bootSignature = TO_BIG_ENDIAN_16(0x55AA);

  if ((res = ifSetParam(interface, IF_POSITION_64, &bootPosition)) != E_OK)
    return res;
  if (ifWrite(interface, &bImage, sizeof(bImage)) != sizeof(bImage))
    return ifGetParam(interface, IF_STATUS, 0);

  static const uint64_t infoPosition = 1 << SECTOR_EXP;
  struct InfoSectorImage iImage;

  iImage.firstSignature = TO_BIG_ENDIAN_32(0x52526141UL);
  memset(iImage.unused0, 0, sizeof(iImage.unused0));
  iImage.infoSignature = TO_BIG_ENDIAN_32(0x72724161UL);
  iImage.freeClusters = clusterCount - 3;
  iImage.lastAllocated = bImage.rootCluster;
  memset(iImage.unused1, 0, sizeof(iImage.unused1));
  iImage.bootSignature = TO_BIG_ENDIAN_16(0x55AA);

  if ((res = ifSetParam(interface, IF_POSITION_64, &infoPosition)) != E_OK)
    return res;
  if (ifWrite(interface, &iImage, sizeof(iImage)) != sizeof(iImage))
    return ifGetParam(interface, IF_STATUS, 0);

  /* Format FAT tables */
  for (uint32_t i = 0; i < bImage.tableSize; ++i)
  {
    uint8_t buffer[1 << SECTOR_EXP];

    if (i > 0)
    {
      memset(buffer, 0, sizeof(buffer));
    }
    else
    {
      static const uint32_t pattern[] = {
          TO_LITTLE_ENDIAN_32(CLUSTER_EOC_VAL),
          TO_LITTLE_ENDIAN_32(CLUSTER_RES_VAL),
          TO_LITTLE_ENDIAN_32(CLUSTER_EOC_VAL)
      };
      memcpy(buffer, pattern, sizeof(pattern));
      memset(buffer + sizeof(pattern), 0, sizeof(buffer) - sizeof(pattern));
    }

    for (size_t fat = 0; fat < bImage.tableCount; ++fat)
    {
      const uint32_t sectors = i + bImage.tableOffset + bImage.tableSize * fat;
      const uint64_t position = (uint64_t)sectors << SECTOR_EXP;

      if ((res = ifSetParam(interface, IF_POSITION_64, &position)) != E_OK)
        return res;
      if (ifWrite(interface, buffer, sizeof(buffer)) != sizeof(buffer))
        return ifGetParam(interface, IF_STATUS, 0);
    }
  }

  /* Clear root cluster */
  for (size_t i = 0; i < sectorsPerCluster; ++i)
  {
    uint8_t buffer[1 << SECTOR_EXP];
    memset(buffer, 0, sizeof(buffer));

    /* Add volume label */
    if (config->label)
    {
      struct DirEntryImage * const entry = (struct DirEntryImage *)buffer;
      memcpy(entry->filename, config->label, strlen(config->label));
      entry->flags = FLAG_VOLUME;
    }

    const uint32_t sectors = i + bImage.reservedSectors
        + bImage.tableSize * bImage.tableCount;
    const uint64_t position = (uint64_t)sectors << SECTOR_EXP;

    if ((res = ifSetParam(interface, IF_POSITION_64, &position)) != E_OK)
      return res;
    if (ifWrite(interface, buffer, sizeof(buffer)) != sizeof(buffer))
      return ifGetParam(interface, IF_STATUS, 0);
  }

  return E_OK;
}
