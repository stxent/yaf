/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <string.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
#include "fat32.h"
#include "fat32_defs.h"
/*----------------------------------------------------------------------------*/
#ifdef FAT_TIME
#include "rtc.h"
#endif
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif
/*----------------------------------------------------------------------------*/
/*------------------Inline functions------------------------------------------*/
static inline bool clusterFree(uint32_t cluster)
{
  return !(cluster & (uint32_t)0x0FFFFFFF);
}
/*----------------------------------------------------------------------------*/
static inline bool clusterEOC(uint32_t cluster)
{
  return (cluster & (uint32_t)0x0FFFFFF8) == (uint32_t)0x0FFFFFF8;
}
/*----------------------------------------------------------------------------*/
static inline bool clusterUsed(uint32_t cluster)
{
  return (cluster & (uint32_t)0x0FFFFFFF) >= (uint32_t)0x00000002 &&
      (cluster & (uint32_t)0x0FFFFFFF) <= (uint32_t)0x0FFFFFEF;
}
/*----------------------------------------------------------------------------*/
/* Calculate sector position from cluster */
static inline uint32_t getSector(struct FatHandle *handle, uint32_t cluster)
{
  return handle->dataSector + (((cluster) - 2) << handle->clusterSize);
}
/*----------------------------------------------------------------------------*/
/* File or directory entries per directory cluster */
static inline uint16_t entryCount(struct FatHandle *handle)
{
  return 1 << E_POW << handle->clusterSize;
}
/*----------------------------------------------------------------------------*/
static /*inline */enum fsResult readSector(struct FatHandle *handle,
    uint32_t address, uint8_t *buffer, uint8_t count)
{
  /* FIXME */
  if (handle->bufferedSector == address)
    return FS_OK;
  handle->bufferedSector = address;
  return fsBlockRead(handle->parent.dev, address << SECTOR_POW, buffer,
      count << SECTOR_POW) == E_OK ? FS_OK : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
static /*inline */enum fsResult writeSector(struct FatHandle *handle,
    uint32_t address, const uint8_t *buffer, uint8_t count)
{
  /* FIXME */
  return fsBlockWrite(handle->parent.dev, address << SECTOR_POW, buffer,
      count << SECTOR_POW) == E_OK ? FS_OK : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
/*------------------Class descriptors-----------------------------------------*/
static const struct FsFileClass fatFileTable = {
    .size = sizeof(struct FatFile),
    .init = 0,
    .deinit = 0,

    .close = fatClose,
    .eof = fatEof,
    .seek = fatSeek,
    .read = fatRead,

#ifdef FAT_WRITE
    .write = fatWrite
#else
    .write = 0
#endif
};
/*----------------------------------------------------------------------------*/
static const struct FsDirClass fatDirTable = {
    .size = sizeof(struct FatDir),
    .init = 0,
    .deinit = 0,

    .close = fatCloseDir,
    .read = fatReadDir,
};
/*----------------------------------------------------------------------------*/
static const struct FsHandleClass fatHandleTable = {
    .size = sizeof(struct FatHandle),
    .init = fatInit,
    .deinit = fatDeinit,

    .File = (void *)&fatFileTable,
    .Dir = (void *)&fatDirTable,

    .open = fatOpen,
    .openDir = fatOpenDir,
    .stat = fatStat,

#ifdef FAT_WRITE
    .makeDir = fatMakeDir,
    .move = fatMove,
    .remove = fatRemove
#else
    .makeDir = 0,
    .move = 0,
    .remove = 0
#endif
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass *FatHandle = (void *)&fatHandleTable;
/*----------------------------------------------------------------------------*/
/*------------------Specific FAT32 functions----------------------------------*/
static enum fsResult fetchEntry(struct FsHandle *sys, struct FatObject *entry)
{
  /* Members entry->index and entry->parent have to be initialized */
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct DirEntryImage *ptr;
  uint32_t sector;

  entry->attribute = 0;
  entry->cluster = 0;
  entry->size = 0;
  while (1)
  {
    if (entry->index >= entryCount(handle))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      if (getNextCluster(sys, &entry->parent))
        return FS_READ_ERROR;
      entry->index = 0;
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if (readSector(handle, sector, handle->buffer, 1))
      return FS_READ_ERROR;
    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(entry->index));
    if (!ptr->name[0]) /* No more entries */
      return FS_EOF;
    if (ptr->name[0] != E_FLAG_EMPTY) /* Entry exists */
      break;
    entry->index++;
  }
  entry->attribute = ptr->flags;
  /* Copy file size, when entry is not directory */
  if (!(entry->attribute & FLAG_DIR))
    entry->size = ptr->size;
  entry->cluster = ptr->clusterHigh << 16 | ptr->clusterLow;
  /* Copy entry name */
  memcpy(entry->name, ptr->name, sizeof(ptr->name));
  /* Add dot, when entry is not directory or extension exists */
  if (!(entry->attribute & FLAG_DIR) && ptr->extension[0] != ' ')
  {
    //TODO add LFN support
    entry->name[8] = '.';
    /* Copy entry extension */
    memcpy(entry->name + 9, ptr->extension, sizeof(ptr->extension));
    entry->name[12] = '\0';
  }
  else
    entry->name[8] = '\0';
  getChunk(entry->name, entry->name);
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
static const char *followPath(struct FsHandle *sys, struct FatObject *item,
    const char *path)
{
  char name[FILE_NAME_MAX];

  path = getChunk(path, name);
  if (!strlen(name))
    return 0;
  if (name[0] == '/')
  {
    item->size = 0;
    item->cluster = ((struct FatHandle *)sys)->rootCluster;
    item->attribute = FLAG_DIR;
    return path;
  }
  item->parent = item->cluster;
  item->index = 0;
  while (!fetchEntry(sys, item))
  {
    if (!strcmp(item->name, name))
      return path;
    item->index++;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
//TODO rewrite to support LFN
/* dest length is 13: 8 name + dot + 3 extension + null */
static const char *getChunk(const char *src, char *dest)
{
  uint8_t counter = 0;

  if (!*src)
    return src;
  if (*src == '/')
  {
    *dest++ = '/';
    *dest = '\0';
    return src + 1;
  }
  while (*src && (counter++ < 12))
  {
    if (*src == '/')
    {
      src++;
      break;
    }
    if (*src == ' ')
    {
      src++;
      continue;
    }
    *dest++ = *src++;
  }
  *dest = '\0';
  return src;
}
/*----------------------------------------------------------------------------*/
static enum fsResult getNextCluster(struct FsHandle *sys, uint32_t *cluster)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  uint32_t nextCluster;

  if (readSector(handle, handle->tableSector + (*cluster >> TE_COUNT),
      handle->buffer, 1))
  {
    return FS_READ_ERROR;
  }
  nextCluster = *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster));
  if (clusterUsed(nextCluster))
  {
    *cluster = nextCluster;
    return FS_OK;
  }
  else
  {
    return FS_EOF;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult allocateCluster(struct FsHandle *sys,
    uint32_t *cluster)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct InfoSectorImage *info;
  uint16_t offset;
  uint32_t current = handle->lastAllocated + 1;

  for (; current != handle->lastAllocated; current++)
  {
#ifdef DEBUG
    if (current == handle->clusterCount)
      printf("Reached end of partition, continue from third cluster\n");
#endif
    if (current >= handle->clusterCount)
      current = 2;
    if (readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1))
    {
      return FS_READ_ERROR;
    }
    offset = (current & ((1 << TE_COUNT) - 1)) << 2;
    /* Is cluster free */
    if (clusterFree(*(uint32_t *)(handle->buffer + offset)))
    {
      *(uint32_t *)(handle->buffer + offset) = CLUSTER_EOC_VAL;
      if ((!*cluster || (*cluster >> TE_COUNT != current >> TE_COUNT)) &&
          updateTable(sys, current >> TE_COUNT))
      {
        return FS_WRITE_ERROR;
      }
      if (*cluster)
      {
        if (readSector(handle, handle->tableSector + (*cluster >> TE_COUNT),
            handle->buffer, 1))
        {
          return FS_READ_ERROR;
        }
        *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster)) = current;
        if (updateTable(sys, *cluster >> TE_COUNT))
          return FS_WRITE_ERROR;
      }
#ifdef DEBUG
      printf("Allocated new cluster: %u, reference %u\n", current, *cluster);
#endif
      *cluster = current;
      /* Update information sector */
      if (readSector(handle, handle->infoSector, handle->buffer, 1))
        return FS_READ_ERROR;
      info = (struct InfoSectorImage *)handle->buffer;
      /* Set last allocated cluster */
      info->lastAllocated = current;
      handle->lastAllocated = current;
      /* Update free clusters count */
      info->freeClusters--;
      if (writeSector(handle, handle->infoSector, handle->buffer, 1))
        return FS_WRITE_ERROR;
      return FS_OK;
    }
  }
#ifdef DEBUG
  printf("Allocation error, possibly partition is full\n");
#endif
  return FS_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Create new entry inside entry->parent chain */
/* Members entry->parent and entry->attribute have to be initialized */
static enum fsResult createEntry(struct FsHandle *sys,
    struct FatObject *entry, const char *name)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct DirEntryImage *ptr;
  uint32_t sector;
  uint8_t pos;
  /* uint16_t clusterCount = 0; TODO */ /* Followed clusters count */

  entry->cluster = 0;
  entry->index = 0;
  for (pos = 0; *(name + pos); pos++)
  {
    if (name[pos] == '/')
      return FS_ERROR; /* One of directories in path does not exist */
  }
  while (1)
  {
    if (entry->index >= entryCount(handle))
    {
      /* Try to get next cluster or allocate new cluster for directory */
      /* Max directory size is 2^16 entries */
      /* TODO Add file limit
      if (getNextCluster(sys, &(entry->parent)) &&
          (clusterCount < (1 << (16 - ENTRY_COUNT - sys->clusterSize)))) */
      if (getNextCluster(sys, &entry->parent))
      {
        if (allocateCluster(sys, &entry->parent))
          return FS_ERROR;
        else
        {
          sector = getSector(handle, entry->parent);
          memset(handle->buffer, 0, SECTOR_SIZE);
          for (pos = 0; pos < (1 << handle->clusterSize); pos++)
          {
            if (writeSector(handle, sector + pos, handle->buffer, 1))
              return FS_WRITE_ERROR;
          }
        }
      }
      else
        return FS_ERROR; /* Directory full */
      entry->index = 0;
      /* clusterCount++; TODO */
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if (readSector(handle, sector, handle->buffer, 1))
      return FS_ERROR;
    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(entry->index));
    /* Empty or removed entry */
    if (!ptr->name[0] || ptr->name[0] == E_FLAG_EMPTY)
      break;
    entry->index++;
  }

  /* Clear name and extension */
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  for (pos = 0; *name && *name != '.' && pos < sizeof(ptr->name); pos++)
    ptr->name[pos] = *name++;
  if (!(entry->attribute & FLAG_DIR) && *name == '.')
  {
    for (pos = 0, name++; *name && pos < sizeof(ptr->extension); pos++)
      ptr->extension[pos] = *name++;
  }
  /* Fill entry fields with zeros */
  memset(ptr->unused, 0, sizeof(ptr->unused));
  ptr->flags = entry->attribute;
  ptr->clusterHigh = 0;
  ptr->clusterLow = 0;
  ptr->size = 0;
#ifdef FAT_TIME
  /* Last modified time and date */
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult freeChain(struct FsHandle *sys, uint32_t cluster)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct InfoSectorImage *info;
  uint8_t *dbuf = handle->buffer;
  uint16_t freeCount = 0;
  uint32_t current = cluster;
  uint32_t next;

  if (!current)
    return FS_OK; /* Already empty */
  while (clusterUsed(current))
  {
    /* Get FAT sector with next cluster value */
    if (readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1))
    {
      return FS_READ_ERROR;
    }
    /* Free cluster */
    next = *(uint32_t *)(dbuf + TE_OFFSET(current));
    *(uint32_t *)(dbuf + TE_OFFSET(current)) = 0;
#ifdef DEBUG
    if (current >> TE_COUNT != next >> TE_COUNT)
    {
      printf("FAT sectors differ, next: %u (0x%07X), current %u\n",
          (next >> TE_COUNT), (next >> TE_COUNT), (current >> TE_COUNT));
    }
    printf("Cleared cluster: %u\n", current);
#endif
    if (current >> TE_COUNT != next >> TE_COUNT &&
        updateTable(sys, current >> TE_COUNT))
    {
      return FS_WRITE_ERROR;
    }
    freeCount++;
    current = next;
  }
  /* Update information sector */
  if (readSector(handle, handle->infoSector, handle->buffer, 1))
    return FS_READ_ERROR;
  info = (struct InfoSectorImage *)dbuf;
  /* Set free clusters count */
  info->freeClusters += freeCount;
  if (writeSector(handle, handle->infoSector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult truncate(struct FsFile *file)
{
  struct FatHandle *handle = (struct FatHandle *)file->descriptor;
  struct FatFile *fh = (struct FatFile *)file;
  struct DirEntryImage *ptr;
  uint32_t sector;

  if (file->mode != FS_WRITE && file->mode != FS_APPEND)
    return FS_ERROR;
  if (freeChain(&handle->parent, fh->cluster) != FS_OK)
    return FS_ERROR;
  sector = getSector(handle, fh->parentCluster) +
      E_SECTOR(fh->parentIndex);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(fh->parentIndex));
  /* Update size and first cluster */
  ptr->size = 0;
  ptr->clusterHigh = 0;
  ptr->clusterLow = 0;
#ifdef FAT_TIME
  /* Update last modified date */
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  fh->cluster = 0;
  fh->currentCluster = 0;
  fh->currentSector = 0;
  file->size = 0;
  file->position = 0;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
/* Copy current sector into FAT sectors located at offset */
#ifdef FAT_WRITE
static enum fsResult updateTable(struct FsHandle *sys, uint32_t offset)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  uint8_t fat;

  for (fat = 0; fat < handle->tableCount; fat++)
  {
    if (writeSector(handle, handle->tableSector + (uint32_t)fat *
        handle->tableSize + offset, handle->buffer, 1))
    {
      return FS_WRITE_ERROR;
    }
  }
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
/*------------------Implemented filesystem methods----------------------------*/
static enum result fatInit(struct FsHandle *sys, const void *cdata)
{
  const struct Fat32Config *config = (const struct Fat32Config *)cdata;
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct BootSectorImage *boot;
#ifdef FAT_WRITE
  struct InfoSectorImage *info;
#endif
  uint16_t tmp;

  /* Initialize buffer variables */
  handle->buffer = malloc(SECTOR_SIZE);
  if (!handle->buffer)
    return E_MEMORY;
  handle->bufferedSector = (uint32_t)(-1);

  sys->dev = config->interface;
  /* Read first sector */
  if (readSector(handle, 0, handle->buffer, 1))
    return E_INTERFACE;
  boot = (struct BootSectorImage *)handle->buffer;
  /* Check boot sector signature (55AA at 0x01FE) */
  /* TODO move signatures to macro */
  if (boot->bootSignature != 0xAA55)
    return E_ERROR;
  /* Check sector size, fixed size of 2 ^ SECTOR_POW allowed */
  if (boot->bytesPerSector != SECTOR_SIZE)
    return E_ERROR;
  /* Calculate sectors per cluster count */
  tmp = boot->sectorsPerCluster;
  handle->clusterSize = 0;
  while (tmp >>= 1)
    handle->clusterSize++;

  handle->tableSector = boot->reservedSectors;
  handle->dataSector = handle->tableSector + boot->fatCopies * boot->fatSize;
  handle->rootCluster = boot->rootCluster;

#ifdef FAT_WRITE
  handle->tableCount = boot->fatCopies;
  handle->tableSize = boot->fatSize;
  handle->clusterCount = ((boot->partitionSize -
      handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = boot->infoSector;
#ifdef DEBUG
  printf("Sectors count:        %u\n", boot->partitionSize);
#endif /* DEBUG */

  if (readSector(handle, handle->infoSector, handle->buffer, 1))
    return E_INTERFACE;
  info = (struct InfoSectorImage *)handle->buffer;
  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
    return E_ERROR;
  handle->lastAllocated = info->lastAllocated;
#ifdef DEBUG
  printf("Free clusters:        %u\n", info->freeClusters);
#endif /* DEBUG */
#endif /* FAT_WRITE */

#ifdef DEBUG
  printf("Size of FatHandle:    %u\n", (unsigned int)sizeof(struct FatHandle));
  printf("Size of FatFile:      %u\n", (unsigned int)sizeof(struct FatFile));
  printf("Size of FatDir:       %u\n", (unsigned int)sizeof(struct FatDir));
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatDeinit(struct FsHandle *sys)
{
  free(((struct FatHandle *)sys)->buffer);
}
/*----------------------------------------------------------------------------*/
static enum fsResult fatStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct FatObject item;
  const char *followedPath;
  uint32_t sector;
#ifdef FAT_TIME
  struct DirEntryImage *ptr;
  struct Time tm;
#endif

  while (*path && (followedPath = followPath(sys, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
    return FS_NOT_FOUND;

  sector = getSector((struct FatHandle *)sys, item.parent) +
      E_SECTOR(item.index);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
#ifdef FAT_TIME
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(item.index));
#endif

#ifdef DEBUG
  result->cluster = item.cluster;
  result->pcluster = item.parent;
  result->pindex = item.index;
#endif
  result->size = item.size;
  if (item.attribute & FLAG_DIR)
    result->type = FS_TYPE_DIR;
  else
    result->type = FS_TYPE_REG;

#ifdef DEBUG
  result->access = 07; /* rwx */
  if (item.attribute & FLAG_RO)
    result->access &= 05;
#endif

#ifdef FAT_TIME
  tm.sec = ptr->time & 0x1F;
  tm.min = (ptr->time >> 5) & 0x3F;
  tm.hour = (ptr->time >> 11) & 0x1F;
  tm.day = ptr->date & 0x1F;
  tm.mon = (ptr->date >> 5) & 0x0F;
  tm.year = ((ptr->date >> 9) & 0x7F) + 1980;
  result->atime = unixTime(&tm);
#endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
static enum fsResult fatOpen(struct FsHandle *sys, struct FsFile *file,
    const char *path, enum fsMode mode)
{
  struct FatFile *fh = (struct FatFile *)file;
  const char *followedPath;
  struct FatObject item;

  file->descriptor = 0;
  file->mode = mode;
  while (*path && (followedPath = followPath(sys, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
  {
#ifdef FAT_WRITE
    if (mode == FS_WRITE)
    {
      item.attribute = 0;
      if (createEntry(sys, &item, path))
        return FS_ERROR;
    }
    else
      return FS_NOT_FOUND;
#else
    return FS_NOT_FOUND;
#endif
  }
  /* Not found if system, volume name or directory */
  if (item.attribute & (FLAG_SYSTEM | FLAG_DIR))
    return FS_NOT_FOUND;
#ifdef FAT_WRITE
  /* Attempt to write into read-only file */
  if ((item.attribute & FLAG_RO) && (mode == FS_WRITE || mode == FS_APPEND))
    return FS_ERROR;
#endif
  file->position = 0;
  file->size = item.size;

  fh->cluster = item.cluster;
  fh->currentCluster = item.cluster;
  fh->currentSector = 0;

#ifdef FAT_WRITE
  fh->parentCluster = item.parent;
  fh->parentIndex = item.index;

  file->descriptor = sys;
  if (mode == FS_WRITE && !*path && file->size && truncate(file) != FS_OK)
    return FS_ERROR;
  /* In append mode file pointer moves to end of file */
  if (mode == FS_APPEND && fsSeek(file, file->size) != FS_OK)
    return FS_ERROR;
#endif

  return FS_OK;
}
/*----------------------------------------------------------------------------*/
static void fatClose(struct FsFile *file)
{
  file->descriptor = 0;
}
/*----------------------------------------------------------------------------*/
static bool fatEof(struct FsFile *file)
{
  return file->position >= file->size;
}
/*----------------------------------------------------------------------------*/
static enum fsResult fatRead(struct FsFile *file, uint8_t *buffer,
    uint16_t count, uint16_t *result)
{
  struct FatHandle *handle = (struct FatHandle *)file->descriptor;
  struct FatFile *fh = (struct FatFile *)file;
  uint16_t chunk, offset, read = 0;

  if (file->mode != FS_READ)
    return FS_ERROR;
  if (count > file->size - file->position)
    count = file->size - file->position;
  if (!count)
    return FS_EOF;

  while (count)
  {
    if (fh->currentSector >= (1 << handle->clusterSize))
    {
      if (getNextCluster(&handle->parent, &fh->currentCluster))
        return FS_READ_ERROR;
      fh->currentSector = 0;
    }

    /* Position in sector */
    offset = (file->position + read) & (SECTOR_SIZE - 1);
    if (offset || count < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = count < chunk ? count : chunk;
      if (readSector(handle, getSector(handle, fh->currentCluster) +
          fh->currentSector, handle->buffer, 1))
      {
        return FS_READ_ERROR;
      }
      memcpy(buffer + read, handle->buffer + offset,
          chunk);
      if (chunk + offset >= SECTOR_SIZE)
        fh->currentSector++;
    }
    else /* Position aligned with start of sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) -
          (fh->currentSector << SECTOR_POW);
      chunk = (count < chunk) ? count & ~(SECTOR_SIZE - 1) : chunk;
#ifdef DEBUG
      printf("Burst read position %u, chunk size %u, sector count %u\n",
          read, chunk, chunk >> SECTOR_POW);
#endif
      if (readSector(handle, getSector(handle, fh->currentCluster) +
          fh->currentSector, buffer + read, chunk >> SECTOR_POW))
      {
        return FS_READ_ERROR;
      }
      fh->currentSector += chunk >> SECTOR_POW;
    }

    read += chunk;
    count -= chunk;
  }

  file->position += read;
  *result = read;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult fatWrite(struct FsFile *file, const uint8_t *buffer,
    uint16_t count, uint16_t *result)
{
  struct FatHandle *handle = (struct FatHandle *)file->descriptor;
  struct FatFile *fh = (struct FatFile *)file;
  struct DirEntryImage *ptr;
  uint16_t chunk, offset, written = 0;
  uint32_t sector;

  if (file->mode != FS_APPEND && file->mode != FS_WRITE)
    return FS_ERROR;
  if (!file->size)
  {
    if (allocateCluster(&handle->parent, &fh->cluster))
      return FS_ERROR;
    fh->currentCluster = fh->cluster;
  }
  /* Checking file size limit (4 GiB - 1) */
  if (file->size + count > FILE_SIZE_MAX)
    count = FILE_SIZE_MAX - file->size;

  while (count)
  {
    if (fh->currentSector >= (1 << handle->clusterSize))
    {
      if (allocateCluster(&handle->parent, &fh->currentCluster))
        return FS_WRITE_ERROR;
      fh->currentSector = 0;
    }

    /* Position in sector */
    offset = (file->position + written) & (SECTOR_SIZE - 1);
    if (offset || count < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (count < chunk) ? count : chunk;
      sector = getSector(handle, fh->currentCluster) + fh->currentSector;
      if (readSector(handle, sector, handle->buffer, 1))
        return FS_READ_ERROR;
      memcpy(handle->buffer + offset,
          buffer + written, chunk);
      if (writeSector(handle, sector, handle->buffer, 1))
        return FS_WRITE_ERROR;
      if (chunk + offset >= SECTOR_SIZE)
        fh->currentSector++;
    }
    else /* Position aligned with start of sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) -
          (fh->currentSector << SECTOR_POW);
      chunk = (count < chunk) ? count & ~(SECTOR_SIZE - 1) : chunk;
#ifdef DEBUG
      printf("Burst write position %u, chunk size %u, sector count %u\n",
          written, chunk, chunk >> SECTOR_POW);
#endif
      //TODO rename file
      if (writeSector(handle, getSector(handle, fh->currentCluster) +
          fh->currentSector, buffer + written, chunk >> SECTOR_POW))
        return FS_READ_ERROR;
      fh->currentSector += chunk >> SECTOR_POW;
    }

    written += chunk;
    count -= chunk;
  }

  sector = getSector(handle, fh->parentCluster) + E_SECTOR(fh->parentIndex);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(fh->parentIndex));
  /* Update first cluster when writing to empty file */
  if (!file->size)
  {
    ptr->clusterHigh = fh->cluster >> 16;
    ptr->clusterLow = fh->cluster;
  }
  file->size += written;
  file->position = file->size;
  /* Update file size */
  ptr->size = file->size;
#ifdef FAT_TIME
  /* Update last modified date */
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  *result = written;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
//TODO test behavior
static enum fsResult fatSeek(struct FsFile *file, uint32_t pos)
{
  struct FatHandle *handle = (struct FatHandle *)file->descriptor;
  struct FatFile *fh = (struct FatFile *)file;
  uint32_t clusterCount, current;

  if (pos > file->size)
    return FS_ERROR;
  clusterCount = pos;
  if (pos > file->position)
  {
    current = fh->currentCluster;
    clusterCount -= file->position;
  }
  else
    current = fh->cluster;
  clusterCount >>= handle->clusterSize + SECTOR_POW;
  while (clusterCount--)
  {
    if (getNextCluster(file->descriptor, &current))
      return FS_READ_ERROR;
  }
  file->position = pos;
  fh->currentCluster = current;
  /* TODO add macro */
  fh->currentSector = (pos >> SECTOR_POW) & ((1 << handle->clusterSize) - 1);
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult fatMove(struct FsHandle *sys, const char *src,
    const char *dest)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  uint8_t attribute/*, counter*/;
  uint16_t index;
  uint32_t parent, cluster, size, sector;
  struct DirEntryImage *ptr;
  const char *followedPath;
  struct FatObject item;

  while (src && *src)
    src = followPath(sys, &item, src);
  if (!src)
    return FS_NOT_FOUND;

  /* System entries are invisible */
  if (item.attribute & FLAG_SYSTEM)
    return FS_NOT_FOUND;

  /* Save old entry data */
  attribute = item.attribute;
  index = item.index;
  parent = item.parent;
  cluster = item.cluster;
  size = item.size;

  while (*dest && (followedPath = followPath(sys, &item, dest)))
    dest = followedPath;
  if (!*dest) /* Entry with same name exists */
    return FS_ERROR;

/*  if (parent == item.parent) //Same directory
  {
    sector = getSector(handle, parent) + ENTRY_SECTOR(index);
    if (readSector(sys, sector))
      return FS_READ_ERROR;
    ptr = handle->buffer + ENTRY_OFFSET(index);
    memset(ptr, 0x20, 11);
    for (counter = 0; *dest && *dest != '.' && counter < 8; counter++)
      *(char *)(ptr + counter) = *dest++;
    if (!(attribute & FLAG_DIR) && *dest == '.')
    {
      for (counter = 8, name++; *dest && (counter < 11); counter++)
        *(char *)(ptr + counter) = *dest++;
    }
    if (writeSector(handle, sector, handle->buffer, 1))
      return FS_WRITE_ERROR;

    return FS_OK;
  }
  else
  {
    item.attribute = attribute;
    if (createEntry(sys, &item, dest))
      return FS_WRITE_ERROR;
  }*/

  item.attribute = attribute;
  if (createEntry(sys, &item, dest))
    return FS_WRITE_ERROR;

  sector = getSector(handle, parent) + E_SECTOR(index);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
  /* Set old entry as removed */
  *(uint8_t *)(handle->buffer + E_OFFSET(index)) = E_FLAG_EMPTY;
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;

  sector = getSector(handle, item.parent) + E_SECTOR(item.index);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(item.index));
  ptr->clusterHigh = cluster >> 16;
  ptr->clusterLow = cluster;
  ptr->size = size;
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;

  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult fatRemove(struct FsHandle *sys, const char *path)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct DirEntryImage *ptr;
  uint16_t index;
  uint32_t tmp; /* Stores first cluster of entry or entry sector */
  uint32_t parent;
  struct FatObject item;

  while (path && *path)
    path = followPath(sys, &item, path);
  if (!path)
    return FS_NOT_FOUND;
  /* Hidden, system, volume name */
  if (item.attribute & (FLAG_HIDDEN | FLAG_SYSTEM))
    return FS_NOT_FOUND;

  index = item.index;
  tmp = item.cluster; /* First cluster of entry */
  parent = item.parent;
  item.index = 2; /* Exclude . and .. */
  item.parent = item.cluster;
  /* Check if directory not empty */
  if ((item.attribute & FLAG_DIR) && !fetchEntry(sys, &item))
    return FS_ERROR;
  if (freeChain(sys, tmp) != FS_OK)
    return FS_ERROR;

  /* Sector in directory with entry description */
  tmp = getSector((struct FatHandle *)sys, parent) + E_SECTOR(index);
  if (readSector(handle, tmp, handle->buffer, 1))
    return FS_READ_ERROR;
  /* Mark entry as free */
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(index));
  ptr->name[0] = E_FLAG_EMPTY;
  if (writeSector(handle, tmp, handle->buffer, 1))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
static enum fsResult fatOpenDir(struct FsHandle *sys, struct FsDir *dir,
    const char *path)
{
  struct FatDir *dh = (struct FatDir *)dir;
  struct FatObject item;

  dir->descriptor = 0;
  while (path && *path)
    path = followPath(sys, &item, path);
  if (!path)
    return FS_NOT_FOUND;
  /* Hidden, system, volume name or not directory */
  if (!(item.attribute & FLAG_DIR) || item.attribute & FLAG_SYSTEM)
    return FS_NOT_FOUND;

  dh->cluster = item.cluster;
  dh->currentCluster = item.cluster;
  dh->currentIndex = 0;

  dir->descriptor = sys;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
static void fatCloseDir(struct FsDir *dir)
{
  dir->descriptor = 0;
}
/*----------------------------------------------------------------------------*/
static enum fsResult fatReadDir(struct FsDir *dir, char *name)
{
  struct FatDir *dh = (struct FatDir *)dir;
  struct FatObject item;

  item.parent = dh->currentCluster;
  /* Fetch next entry */
  item.index = dh->currentIndex;
  do
  {
    if (fetchEntry(dir->descriptor, &item))
      return FS_NOT_FOUND;
    item.index++;
  }
  while (item.attribute & (FLAG_HIDDEN | FLAG_SYSTEM));
  /* Hidden and system entries not shown */
  dh->currentIndex = item.index; /* Points to next item */
  dh->currentCluster = item.parent;

  strcpy(name, item.name);

  return FS_OK;
}
/*----------------------------------------------------------------------------*/
// enum fsResult fatSeekDir(struct FsDir *dir, uint16_t pos)
// {
//   uint16_t clusterCount;
//   uint32_t current;

  //FIXME completely rewrite
//   if (dir->state != FS_OPENED) //FIXME
//     return FS_ERROR;

  //TODO Search from current position
//   clusterCount = pos;
//   if (pos > dir->position)
//   {
//     current = dir->currentCluster;
//     clusterCount -= dir->position;
//   }
//   else
//     current = dir->cluster;
//   clusterCount >>= SECTOR_POW - 5 + dir->descriptor->clusterSize;

//   current = dir->cluster;
//   clusterCount = pos >> (E_POW + dir->descriptor->clusterSize);
//   while (clusterCount--)
//   {
//     if (getNextCluster(dir->descriptor, &current))
//       return FS_READ_ERROR;
//   }
//   dir->currentCluster = current;
// //   dir->position = pos;
//   return FS_OK;
// }
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum fsResult fatMakeDir(struct FsHandle *sys, const char *path)
{
  //FIXME
  struct FatHandle *handle = (struct FatHandle *)sys;
  struct DirEntryImage *ptr;
  struct FatObject item;
  uint32_t sector, parent = handle->rootCluster;
  const char *followedPath;
  uint8_t pos;

  while (*path && (followedPath = followPath(sys, &item, path)))
  {
    parent = item.cluster;
    path = followedPath;
  }
  if (!*path) /* Entry with same name exists */
    return FS_ERROR;
  item.attribute = FLAG_DIR; /* Create entry with directory attribute */
  if (createEntry(sys, &item, path) || allocateCluster(sys, &item.cluster))
    return FS_WRITE_ERROR;
  sector = getSector(handle, item.parent) + E_SECTOR(item.index);
  if (readSector(handle, sector, handle->buffer, 1))
    return FS_READ_ERROR;
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(item.index));
  ptr->clusterHigh = item.cluster >> 16;
  ptr->clusterLow = item.cluster;
  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  sector = getSector(handle, item.cluster);

  /* Fill cluster with zeros */
  memset(handle->buffer, 0, SECTOR_SIZE);
  for (pos = (1 << handle->clusterSize) - 1; pos > 0; pos--)
  {
    if (writeSector(handle, sector + pos, handle->buffer, 1))
      return FS_WRITE_ERROR;
  }

  /* Current directory entry . */
  ptr = (struct DirEntryImage *)handle->buffer;
  /* Fill name and extension with spaces */
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  ptr->name[0] = '.';
  ptr->flags = FLAG_DIR;
  ptr->clusterHigh = item.cluster >> 16;
  ptr->clusterLow = item.cluster;
#ifdef FS_RTS_ENABLED
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif

  /* Parent directory entry .. */
  ptr++;
  /* Fill name and extension with spaces */
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  ptr->name[0] = ptr->name[1] = '.';
  ptr->flags = FLAG_DIR;
  if (parent != handle->rootCluster)
  {
    ptr->clusterHigh = parent >> 16;
    ptr->clusterLow = parent;
  }
#ifdef FAT_TIME
  ptr->time = (ptr - 1)->time;
  ptr->date = (ptr - 1)->date;
#endif

  if (writeSector(handle, sector, handle->buffer, 1))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(DEBUG)
uint32_t countFree(struct FsHandle *sys)
{
  struct FatHandle *handle = (struct FatHandle *)sys;
  uint32_t res = 0, current;
  uint32_t *count = malloc(sizeof(uint32_t) * handle->tableCount);
  uint8_t fat, i, j;
  uint16_t offset;

  if (!count)
    return res; /* Memory allocation problem */
  for (fat = 0; fat < handle->tableCount; fat++)
  {
    count[fat] = 0;
    for (current = 0; current < handle->clusterCount; current++)
    {
      if (readSector(handle, handle->tableSector + (current >> TE_COUNT),
          handle->buffer, 1))
      {
        return FS_READ_ERROR;
      }
      offset = (current & ((1 << TE_COUNT) - 1)) << 2;
      if (clusterFree(*(uint32_t *)(handle->buffer + offset)))
        count[fat]++;
    }
  }
  for (i = 0; i < handle->tableCount; i++)
    for (j = 0; j < handle->tableCount; j++)
      if (i != j && count[i] != count[j])
      {
        printf("FAT records differ: %u and %u\n", count[i], count[j]);
      }
  res = count[0];
  free(count);
  return res;
}
#endif
