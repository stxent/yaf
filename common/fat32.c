/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

/* TODO Propagate errros in underlying functions to callers */
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
/*------------------Class descriptors-----------------------------------------*/
static const struct FsFileClass fatFileTable = {
    .size = sizeof(struct FatFile),
    .init = 0,
    .deinit = 0,

    .close = fatClose,
    .eof = fatEof,
    .tell = fatTell,
    .seek = fatSeek,
    .read = fatRead,

    .write = fatWrite
};
/*----------------------------------------------------------------------------*/
static const struct FsDirClass fatDirTable = {
    .size = sizeof(struct FatDir),
    .init = 0,
    .deinit = 0,

    .close = fatCloseDir,
    .read = fatReadDir
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

    .makeDir = fatMakeDir,
    .move = fatMove,
    .remove = fatRemove,
    .removeDir = fatRemoveDir
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass *FatHandle = (void *)&fatHandleTable;
/*----------------------------------------------------------------------------*/
/*------------------Specific FAT32 functions----------------------------------*/
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
  while (*src && counter++ < FILE_NAME_MAX - 1)
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
static enum result getNextCluster(struct FatHandle *handle, uint32_t *cluster)
{
  uint32_t nextCluster;
  enum result res;

  if ((res = readSector(handle, handle->tableSector + (*cluster >> TE_COUNT),
      handle->buffer, 1)) != E_OK)
  {
    return res;
  }
  nextCluster = *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster));
  if (clusterUsed(nextCluster))
  {
    *cluster = nextCluster;
    return E_OK;
  }
  else
    return E_EOF;
}
/*----------------------------------------------------------------------------*/
/* Members entry->index and entry->parent have to be initialized */
/* Pointer nameBuffer must be at least FILE_NAME_MAX length or zero */
static enum result fetchEntry(struct FatHandle *handle,
    struct FatObject *entry, char *nameBuffer)
{
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
      if (getNextCluster(handle, &entry->parent))
        return E_INTERFACE;
      entry->index = 0;
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if (readSector(handle, sector, handle->buffer, 1))
      return E_INTERFACE;
    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(entry->index));
    if (!ptr->name[0]) /* No more entries */
      return E_EOF;
    if (ptr->name[0] != E_FLAG_EMPTY) /* Entry exists */
      break;
    entry->index++;
  }
  entry->attribute = ptr->flags;
  /* Copy file size, when entry is not directory */
  if (!(entry->attribute & FLAG_DIR))
    entry->size = ptr->size;
  entry->cluster = ptr->clusterHigh << 16 | ptr->clusterLow;
  if (nameBuffer)
  {
    /* Copy entry name */
    memcpy(nameBuffer, ptr->name, sizeof(ptr->name));
    /* Add dot, when entry is not directory or extension exists */
    if (!(entry->attribute & FLAG_DIR) && ptr->extension[0] != ' ')
    {
      //TODO add LFN support
      nameBuffer[8] = '.';
      /* Copy entry extension */
      memcpy(nameBuffer + 9, ptr->extension, sizeof(ptr->extension));
      nameBuffer[12] = '\0';
    }
    else
      nameBuffer[8] = '\0';
    getChunk(nameBuffer, nameBuffer);
  }
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static const char *followPath(struct FatHandle *handle, struct FatObject *item,
    const char *path)
{
  char name[FILE_NAME_MAX], entryName[FILE_NAME_MAX];

  path = getChunk(path, name);
  if (!strlen(name))
    return 0;
  if (name[0] == '/')
  {
    item->size = 0;
    item->cluster = handle->rootCluster;
    item->attribute = FLAG_DIR;
    return path;
  }
  item->parent = item->cluster;
  item->index = 0;
  while (!fetchEntry(handle, item, entryName))
  {
    if (!strcmp(entryName, name))
      return path;
    item->index++;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static enum result readSector(struct FatHandle *handle,
    uint32_t address, uint8_t *buffer, uint8_t count)
{
  if (handle->bufferedSector == address)
    return E_OK;
  handle->bufferedSector = address;
  return fsBlockRead(handle->parent.dev, address << SECTOR_POW, buffer,
      count << SECTOR_POW);
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result allocateCluster(struct FatHandle *handle, uint32_t *cluster)
{
  struct InfoSectorImage *info;
  uint16_t offset;
  uint32_t current = handle->lastAllocated + 1;
  enum result res;

  for (; current != handle->lastAllocated; current++)
  {
#ifdef DEBUG
    if (current == handle->clusterCount)
      printf("Reached end of partition, continue from third cluster\n");
#endif
    if (current >= handle->clusterCount)
      current = 2;
    if ((res = readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1)) != E_OK)
    {
      return res;
    }
    offset = (current & ((1 << TE_COUNT) - 1)) << 2;
    /* Is cluster free */
    if (clusterFree(*(uint32_t *)(handle->buffer + offset)))
    {
      *(uint32_t *)(handle->buffer + offset) = CLUSTER_EOC_VAL;
      if ((!*cluster || (*cluster >> TE_COUNT != current >> TE_COUNT)) &&
          (res = updateTable(handle, current >> TE_COUNT)) != E_OK)
      {
        return res;
      }
      if (*cluster)
      {
        if ((res = readSector(handle, handle->tableSector +
            (*cluster >> TE_COUNT), handle->buffer, 1)) != E_OK)
        {
          return res;
        }
        *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster)) = current;
        if ((res = updateTable(handle, *cluster >> TE_COUNT)) != E_OK)
          return res;
      }
#ifdef DEBUG
      printf("Allocated new cluster: %u, reference %u\n", current, *cluster);
#endif
      *cluster = current;
      /* Update information sector */
      if ((res = readSector(handle, handle->infoSector, handle->buffer, 1)))
        return res;
      info = (struct InfoSectorImage *)handle->buffer;
      /* Set last allocated cluster */
      info->lastAllocated = current;
      handle->lastAllocated = current;
      /* Update free clusters count */
      info->freeClusters--;
      if ((res = writeSector(handle, handle->infoSector, handle->buffer, 1)))
        return res;
      return E_OK;
    }
  }
#ifdef DEBUG
  printf("Allocation error, possibly partition is full\n");
#endif
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Create new entry inside entry->parent chain */
/* Members entry->parent and entry->attribute have to be initialized */
static enum result createEntry(struct FatHandle *handle,
    struct FatObject *entry, const char *name)
{
  /*
   * Officially maximum directory capacity is 2^16 entries
   * but technically there is no such limit
   */
  struct DirEntryImage *ptr;
  enum result res;
  uint32_t sector;
  uint8_t pos;

  entry->cluster = 0;
  entry->index = 0;
  for (pos = 0; *(name + pos); pos++)
  {
    if (name[pos] == '/')
      return E_ERROR; /* One of directories in path does not exist */
  }
  while (1)
  {
    if (entry->index >= entryCount(handle))
    {
      /* Try to get next cluster or allocate new cluster for directory */
      res = getNextCluster(handle, &entry->parent);
      if (res == E_EOF)
      {
        if ((res = allocateCluster(handle, &entry->parent)) != E_OK)
          return res;
        else
        {
          sector = getSector(handle, entry->parent);
          memset(handle->buffer, 0, SECTOR_SIZE);
          for (pos = 0; pos < (1 << handle->clusterSize); pos++)
          {
            if ((res = writeSector(handle, sector + pos,
                handle->buffer, 1)) != E_OK)
            {
              return res;
            }
          }
        }
      }
      else if (res != E_OK)
        return res;
      entry->index = 0;
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      return res;
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
  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result freeChain(struct FatHandle *handle, uint32_t cluster)
{
  struct InfoSectorImage *info;
  uint8_t *dbuf = handle->buffer;
  uint32_t freeCount = 0; /* Freed clusters count */
  uint32_t next, current = cluster;
  enum result res;

  if (!current)
    return E_OK; /* Already empty */
  while (clusterUsed(current))
  {
    /* Get FAT sector with next cluster value */
    if ((res = readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1)) != E_OK)
    {
      return res;
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
        (res = updateTable(handle, current >> TE_COUNT)) != E_OK)
    {
      return res;
    }
    freeCount++;
    current = next;
  }
  /* Update information sector */
  if ((res = readSector(handle, handle->infoSector, handle->buffer, 1)) != E_OK)
    return res;
  info = (struct InfoSectorImage *)dbuf;
  /* Set free clusters count */
  info->freeClusters += freeCount;
  if ((res = writeSector(handle,
      handle->infoSector, handle->buffer, 1)) != E_OK)
  {
    return res;
  }
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result markFree(struct FatHandle *handle, struct FatObject *entry)
{
  struct DirEntryImage *ptr;
  enum result res;
  uint32_t sector; /* Directory sector containing entry description */

  sector = getSector(handle, entry->parent) + E_SECTOR(entry->index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  /* Mark directory entry as free */
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(entry->index));
  ptr->name[0] = E_FLAG_EMPTY;
  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result writeSector(struct FatHandle *handle,
    uint32_t address, const uint8_t *buffer, uint8_t count)
{
  return fsBlockWrite(handle->parent.dev, address << SECTOR_POW, buffer,
      count << SECTOR_POW);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result truncate(struct FatFile *fileHandle)
{
  /* FIXME */
  struct FatHandle *handle = (struct FatHandle *)fileHandle->parent.descriptor;
  enum result res;

  if (fileHandle->parent.mode == FS_READ)
    return E_ERROR; /* TODO Add access denied message */
  if ((res = freeChain(handle, fileHandle->cluster)) != E_OK)
    return res;

  fileHandle->cluster = 0;
  fileHandle->currentCluster = 0;
  fileHandle->currentSector = 0;
  fileHandle->size = 0;
  fileHandle->position = 0;
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
/* Copy current sector into FAT sectors located at offset */
#ifdef FAT_WRITE
static enum result updateTable(struct FatHandle *handle, uint32_t offset)
{
  enum result res;
  uint8_t fat;

  for (fat = 0; fat < handle->tableCount; fat++)
  {
    if ((res = writeSector(handle, handle->tableSector + (uint32_t)fat *
        handle->tableSize + offset, handle->buffer, 1)) != E_OK)
    {
      return res;
    }
  }
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
/*------------------Implemented filesystem methods----------------------------*/
static enum result fatInit(void *object, const void *cdata)
{
  const struct Fat32Config *config = cdata;
  struct FatHandle *handle = object;
  struct BootSectorImage *boot;
  enum result res;
#ifdef FAT_WRITE
  struct InfoSectorImage *info;
#endif
  uint16_t tmp;

  /* Initialize buffer variables */
  handle->buffer = malloc(SECTOR_SIZE);
  if (!handle->buffer)
    return E_MEMORY;
  handle->bufferedSector = (uint32_t)(-1);

  handle->parent.dev = config->interface;
  /* Read first sector */
  if ((res = readSector(handle, 0, handle->buffer, 1)) != E_OK)
    return res;
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
#ifdef DEBUG
  printf("Cluster size:       %u\n", (unsigned int)(1 << handle->clusterSize));
  printf("Table sector:       %u\n", (unsigned int)handle->tableSector);
  printf("Data sector:        %u\n", (unsigned int)handle->dataSector);
#endif

#ifdef FAT_WRITE
  handle->tableCount = boot->fatCopies;
  handle->tableSize = boot->fatSize;
  handle->clusterCount = ((boot->partitionSize -
      handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = boot->infoSector;
#ifdef DEBUG
  printf("Info sector:        %u\n", (unsigned int)handle->infoSector);
  printf("Table copies:       %u\n", (unsigned int)handle->tableCount);
  printf("Table size:         %u\n", (unsigned int)handle->tableSize);
  printf("Cluster count:      %u\n", (unsigned int)handle->clusterCount);
  printf("Sectors count:      %u\n", (unsigned int)boot->partitionSize);
#endif /* DEBUG */

  if ((res = readSector(handle, handle->infoSector, handle->buffer, 1)) != E_OK)
    return res;
  info = (struct InfoSectorImage *)handle->buffer;
  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
    return E_ERROR;
  handle->lastAllocated = info->lastAllocated;
#ifdef DEBUG
  printf("Free clusters:      %u\n", (unsigned int)info->freeClusters);
#endif /* DEBUG */
#endif /* FAT_WRITE */

#ifdef DEBUG
  printf("Size of FatHandle:  %u\n", (unsigned int)sizeof(struct FatHandle));
  printf("Size of FatFile:    %u\n", (unsigned int)sizeof(struct FatFile));
  printf("Size of FatDir:     %u\n", (unsigned int)sizeof(struct FatDir));
#endif
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatDeinit(void *object)
{
  free(((struct FatHandle *)object)->buffer);
}
/*----------------------------------------------------------------------------*/
/*------------------Common functions------------------------------------------*/
static enum result fatStat(void *object, struct FsStat *result,
    const char *path)
{
  struct FatHandle *handle = object;
  struct FatObject item;
  enum result res;
  const char *followedPath;
  uint32_t sector;
#ifdef FAT_TIME
  struct DirEntryImage *ptr;
  struct Time tm;
#endif

  while (*path && (followedPath = followPath(handle, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
    return E_NONEXISTENT;

  sector = getSector(object, item.parent) + E_SECTOR(item.index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
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
    result->type = FS_TYPE_FILE;

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
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result fatOpen(void *handleObject, void *fileObject,
    const char *path, enum fsMode mode)
{
  struct FatHandle *handle = handleObject;
  struct FatFile *fileHandle = fileObject;
  struct FatObject item;
  const char *followedPath;
#ifdef FAT_WRITE
  enum result res;
#endif

  fileHandle->parent.descriptor = 0;
  fileHandle->parent.mode = mode;
  while (*path && (followedPath = followPath(handle, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
  {
#ifdef FAT_WRITE
    if (mode == FS_WRITE)
    {
      item.attribute = 0;
      if ((res = createEntry(handle, &item, path)) != E_OK)
        return res;
    }
    else
      return E_NONEXISTENT;
#else
    return E_NONEXISTENT;
#endif
  }
  /* Not found if volume name, system or directory entry */
  if (item.attribute & (FLAG_VOLUME | FLAG_SYSTEM | FLAG_DIR))
    return E_NONEXISTENT;
#ifdef FAT_WRITE
  /* Attempt to write into read-only file */
  if ((item.attribute & FLAG_RO) && (mode == FS_WRITE || mode == FS_APPEND))
    return E_ERROR;
#endif
  fileHandle->parent.descriptor = handleObject;

  fileHandle->position = 0;
  fileHandle->size = item.size;
  fileHandle->cluster = item.cluster;
  fileHandle->currentCluster = item.cluster;
  fileHandle->currentSector = 0;

#ifdef FAT_WRITE
  fileHandle->parentCluster = item.parent;
  fileHandle->parentIndex = item.index;

  if (mode == FS_WRITE && !*path && fileHandle->size &&
      (res = truncate(fileHandle)) != E_OK)
  {
    return res;
  }
  /* In append mode file pointer moves to end of file */
  if (mode == FS_APPEND && (res = fsSeek(fileObject, 0, FS_SEEK_END)) != E_OK)
    return res;
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result fatOpenDir(void *handleObject, void *dirObject,
    const char *path)
{
  struct FatHandle *handle = handleObject;
  struct FatDir *dirHandle = dirObject;
  struct FatObject item;

  dirHandle->parent.descriptor = 0;
  while (path && *path)
    path = followPath(handle, &item, path);
  if (!path)
    return E_NONEXISTENT;
  /* Not directory or volume name, system */
  if (!(item.attribute & FLAG_DIR) ||
      item.attribute & (FLAG_VOLUME | FLAG_SYSTEM))
  {
    return E_NONEXISTENT;
  }

  dirHandle->cluster = item.cluster;
  dirHandle->currentCluster = item.cluster;
  dirHandle->currentIndex = 0;

  dirHandle->parent.descriptor = handleObject;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatMove(void *object, const char *src, const char *dest)
{
  struct FatHandle *handle = object;
  uint32_t sector;
  struct DirEntryImage *ptr;
  struct FatObject item, oldItem;
  enum result res;
  const char *followedPath;

  while (src && *src)
    src = followPath(handle, &item, src);
  if (!src)
    return E_NONEXISTENT;

  /* System entries are invisible */
  if (item.attribute & FLAG_SYSTEM)
    return E_NONEXISTENT;

  /* Save old entry data */
  oldItem = item;

  while (*dest && (followedPath = followPath(handle, &item, dest)))
    dest = followedPath;
  if (!*dest) /* Entry with same name exists */
    return E_ERROR;

/*  if (parent == item.parent) //Same directory
  {
    sector = getSector(handle, parent) + ENTRY_SECTOR(index);
    if (readSector(handle, sector))
      return E_INTERFACE;
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
      return E_INTERFACE;

    return E_OK;
  }
  else
  {
    item.attribute = attribute;
    if (createEntry(handle, &item, dest))
      return E_INTERFACE;
  }*/

  item.attribute = oldItem.attribute;
  if ((res = createEntry(handle, &item, dest)) != E_OK)
    return res;

  if ((res = markFree(handle, &oldItem)) != E_OK)
    return res;

  sector = getSector(handle, item.parent) + E_SECTOR(item.index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(item.index));
  /* TODO Possibly optimize */
  ptr->clusterHigh = oldItem.cluster >> 16;
  ptr->clusterLow = oldItem.cluster;
  ptr->size = oldItem.size;
  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;

  return E_OK;
}
#else
static enum result fatMove(void *object __attribute__((unused)),
    const char *src __attribute__((unused)),
    const char *dest __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatRemove(void *object, const char *path)
{
  struct FatHandle *handle = object;
  struct FatObject item;
  enum result res;

  while (path && *path)
    path = followPath(handle, &item, path);
  if (!path)
    return E_NONEXISTENT;
  /* Read only, volume name, system or directory */
  if (item.attribute & (FLAG_RO | FLAG_VOLUME | FLAG_SYSTEM | FLAG_DIR))
    return E_NONEXISTENT;

  /* Mark file table clusters as free */
  if ((res = freeChain(handle, item.cluster)) != E_OK)
    return res;

  if ((res = markFree(handle, &item)) != E_OK)
    return res;
  return E_OK;
}
#else
static enum result fatRemove(void *object __attribute__((unused)),
    const char *path __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
/*------------------File functions--------------------------------------------*/
static void fatClose(void *object)
{
  struct FsFile *file = object;

  /* Write changes when file was opened for writing or updating */
  if (file->mode != FS_READ)
    fatFlush(object);
  file->descriptor = 0;
}
/*----------------------------------------------------------------------------*/
static bool fatEof(void *object)
{
  struct FatFile *fileHandle = object;

  return fileHandle->position >= fileHandle->size;
}
/*----------------------------------------------------------------------------*/
static uint32_t fatRead(void *object, uint8_t *buffer, uint32_t count)
{
  struct FatFile *fileHandle = object;
  /* FIXME */
  struct FatHandle *handle = (struct FatHandle *)fileHandle->parent.descriptor;
  uint16_t chunk, offset;
  uint32_t read = 0;

  if (fileHandle->parent.mode != FS_READ &&
      fileHandle->parent.mode != FS_UPDATE)
  {
    return 0;
  }
  if (count > fileHandle->size - fileHandle->position)
    count = fileHandle->size - fileHandle->position;
  if (!count)
    return 0;

  while (count)
  {
    if (fileHandle->currentSector >= (1 << handle->clusterSize))
    {
      if (getNextCluster(handle, &fileHandle->currentCluster) != E_OK)
        return 0; /* Sector read error or end-of-file */
      fileHandle->currentSector = 0;
    }

    /* Position in sector */
    offset = (fileHandle->position + read) & (SECTOR_SIZE - 1);
    if (offset || count < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = count < chunk ? count : chunk;
      if (readSector(handle, getSector(handle, fileHandle->currentCluster) +
          fileHandle->currentSector, handle->buffer, 1) != E_OK)
      {
        return 0;
      }
      memcpy(buffer + read, handle->buffer + offset, chunk);
      if (chunk + offset >= SECTOR_SIZE)
        fileHandle->currentSector++;
    }
    else /* Position aligned with start of sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) -
          (fileHandle->currentSector << SECTOR_POW);
      chunk = (count < chunk) ? count & ~(SECTOR_SIZE - 1) : chunk;
#ifdef DEBUG
      printf("Burst read position %u, chunk size %u, sector count %u\n",
          read, chunk, chunk >> SECTOR_POW);
#endif
      if (readSector(handle, getSector(handle,
          fileHandle->currentCluster) + fileHandle->currentSector,
          buffer + read, chunk >> SECTOR_POW) != E_OK)
      {
        return 0;
      }
      fileHandle->currentSector += chunk >> SECTOR_POW;
    }

    read += chunk;
    count -= chunk;
  }

  fileHandle->position += read;
  return read;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static uint32_t fatWrite(void *object, const uint8_t *buffer, uint32_t count)
{
  struct FatFile *fileHandle = object;
  struct FatHandle *handle = (struct FatHandle *)fileHandle->parent.descriptor;
  enum result res;
  uint16_t chunk, offset;
  uint32_t sector, written = 0;

  if (fileHandle->parent.mode == FS_READ)
    return 0;
  if (!fileHandle->size)
  {
    if (allocateCluster(handle, &fileHandle->cluster) != E_OK)
      return 0;
    fileHandle->currentCluster = fileHandle->cluster;
  }
  /* Checking file size limit (4 GiB - 1) */
  if (fileHandle->size + count > FILE_SIZE_MAX)
    count = FILE_SIZE_MAX - fileHandle->size;

  /* TODO Possibly save intermediate state on error */
  while (count)
  {
    if (fileHandle->currentSector >= (1 << handle->clusterSize))
    {
      /* Allocate new cluster when next cluster does not exist */
      res = getNextCluster(handle, &fileHandle->currentCluster);
      if ((res != E_EOF && res != E_OK) || (res == E_EOF &&
          allocateCluster(handle, &fileHandle->currentCluster) != E_OK))
      {
          return 0;
      }
      fileHandle->currentSector = 0;
    }

    /* Position in sector */
    offset = (fileHandle->position + written) & (SECTOR_SIZE - 1);
    if (offset || count < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (count < chunk) ? count : chunk;
      sector = getSector(handle, fileHandle->currentCluster) +
          fileHandle->currentSector;
      if (readSector(handle, sector, handle->buffer, 1) != E_OK)
        return 0;
      memcpy(handle->buffer + offset, buffer + written, chunk);
      if (writeSector(handle, sector, handle->buffer, 1) != E_OK)
        return 0;
      if (chunk + offset >= SECTOR_SIZE)
        fileHandle->currentSector++;
    }
    else /* Position aligned with start of sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) -
          (fileHandle->currentSector << SECTOR_POW);
      chunk = (count < chunk) ? count & ~(SECTOR_SIZE - 1) : chunk;
      if (writeSector(handle, getSector(handle,
          fileHandle->currentCluster) + fileHandle->currentSector,
          buffer + written, chunk >> SECTOR_POW) != E_OK)
      {
        return 0;
      }
      fileHandle->currentSector += chunk >> SECTOR_POW;
    }

    written += chunk;
    count -= chunk;
  }

  fileHandle->size += written;
  fileHandle->position = fileHandle->size;
  return written;
}
#else
static uint32_t fatWrite(void *object __attribute__((unused)),
    const uint8_t *buffer __attribute__((unused)),
    uint32_t count __attribute__((unused)))
{
  return 0;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatFlush(void *object)
{
  struct FatFile *fileHandle = object;
  struct FatHandle *handle = (struct FatHandle *)fileHandle->parent.descriptor;
  struct DirEntryImage *ptr;
  enum result res;
  uint32_t sector;

  if (fileHandle->parent.mode == FS_READ)
    return E_ERROR;
  sector = getSector(handle, fileHandle->parentCluster) +
      E_SECTOR(fileHandle->parentIndex);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  /* Pointer to entry position in sector */
  ptr = (struct DirEntryImage *)(handle->buffer +
      E_OFFSET(fileHandle->parentIndex));
  /* Update first cluster when writing to empty file or truncating file */
  ptr->clusterHigh = fileHandle->cluster >> 16;
  ptr->clusterLow = fileHandle->cluster;
  /* Update file size */
  ptr->size = fileHandle->size;
#ifdef FAT_TIME
  /* Update last modified date */
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  return E_OK;
}
#else
static enum result fatFlush(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
//TODO Rewrite
static enum result fatSeek(void *object, asize_t offset,
    enum fsSeekOrigin origin)
{
  struct FatFile *fileHandle = object;
  struct FatHandle *handle = (struct FatHandle *)fileHandle->parent.descriptor;
  enum result res;
  uint32_t clusterCount, current;

  switch (origin)
  {
    case FS_SEEK_CUR:
      offset = fileHandle->position + offset;
      break;
    case FS_SEEK_END: /* Offset must be negative */
      offset = fileHandle->size + offset;
      break;
    case FS_SEEK_SET:
    default:
      break;
  }
  if (offset < 0 || offset > fileHandle->size)
    return E_ERROR;
  clusterCount = offset;
  if (offset > fileHandle->position)
  {
    current = fileHandle->currentCluster;
    clusterCount -= fileHandle->position;
  }
  else
    current = fileHandle->cluster;
  clusterCount >>= handle->clusterSize + SECTOR_POW;
  while (clusterCount--)
  {
    if ((res = getNextCluster(handle, &current)) != E_OK)
      return E_INTERFACE;
  }
  fileHandle->position = offset;
  fileHandle->currentCluster = current;
  /* TODO add macro */
  fileHandle->currentSector =
      (offset >> SECTOR_POW) & ((1 << handle->clusterSize) - 1);
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static asize_t fatTell(void *object)
{
  struct FatFile *fileHandle = object;

  return (asize_t)fileHandle->position;
}
/*----------------------------------------------------------------------------*/
/*------------------Directory functions---------------------------------------*/
static void fatCloseDir(void *object)
{
  ((struct FsDir *)object)->descriptor = 0;
}
/*----------------------------------------------------------------------------*/
static enum result fatReadDir(void *object, char *name)
{
  struct FatDir *dirHandle = object;
  struct FatObject item;
  enum result res;
  char entryName[FILE_NAME_MAX];

  item.parent = dirHandle->currentCluster;
  /* Fetch next entry */
  item.index = dirHandle->currentIndex;
  do
  {
    if ((res = fetchEntry((struct FatHandle *)dirHandle->parent.descriptor,
        &item, entryName)) != E_OK)
    {
      return E_NONEXISTENT;
    }
    item.index++;
  }
  while (item.attribute & (FLAG_HIDDEN | FLAG_SYSTEM));
  /* Hidden and system entries not shown */
  dirHandle->currentIndex = item.index; /* Points to next item */
  dirHandle->currentCluster = item.parent;

  strcpy(name, entryName);
  return E_OK;
}
/*----------------------------------------------------------------------------*/
// enum result fatSeekDir(void *object, uint16_t pos)
// {
//   uint16_t clusterCount;
//   uint32_t current;

  //FIXME completely rewrite
//   if (dir->state != FS_OPENED) //FIXME
//     return E_ERROR;

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
//       return E_INTERFACE;
//   }
//   dir->currentCluster = current;
// //   dir->position = pos;
//   return E_OK;
// }
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatMakeDir(void *object, const char *path)
{
  //FIXME
  struct FatHandle *handle = object;
  struct DirEntryImage *ptr;
  struct FatObject item;
  enum result res;
  uint32_t sector, parent = handle->rootCluster;
  const char *followedPath;
  uint8_t pos;

  /* TODO Check top folder attributes */
  while (*path && (followedPath = followPath(handle, &item, path)))
  {
    parent = item.cluster;
    path = followedPath;
  }
  if (!*path) /* Entry with same name exists */
    return E_ERROR;
  item.attribute = FLAG_DIR; /* Create entry with directory attribute */
  if ((res = createEntry(handle, &item, path)) != E_OK ||
      (res = allocateCluster(handle, &item.cluster)) != E_OK)
  {
    return res;
  }
  sector = getSector(handle, item.parent) + E_SECTOR(item.index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(item.index));
  ptr->clusterHigh = item.cluster >> 16;
  ptr->clusterLow = item.cluster;
  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  sector = getSector(handle, item.cluster);

  /* Fill cluster with zeros */
  memset(handle->buffer, 0, SECTOR_SIZE);
  for (pos = (1 << handle->clusterSize) - 1; pos > 0; pos--)
  {
    if ((res = writeSector(handle, sector + pos, handle->buffer, 1)) != E_OK)
      return res;
  }

  /* TODO Optimize with mark entry */
  /* Current directory entry . */
  ptr = (struct DirEntryImage *)handle->buffer;
  /* Fill name and extension with spaces */
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  ptr->name[0] = '.';
  ptr->flags = FLAG_DIR;
  ptr->clusterHigh = item.cluster >> 16;
  ptr->clusterLow = item.cluster;
#ifdef FAT_TIME
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif

  /* Parent directory entry .. */
  ptr++;
  /* Fill name and extension with spaces */
  /* TODO LFN */
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

  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  return E_OK;
}
#else
static enum result fatMakeDir(void *object __attribute__((unused)),
    const char *path __attribute__((unused)))
{
  return E_ERROR;
}
#endif /* FAT_WRITE */
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatRemoveDir(void *object, const char *path)
{
  struct FatHandle *handle = object;
  struct FatObject item, dirItem;
  enum result res;

  while (path && *path)
    path = followPath(handle, &item, path);
  if (!path)
    return E_NONEXISTENT;
  /* Not directory or read only, volume name, system */
  if (!(item.attribute & FLAG_DIR) ||
      item.attribute & (FLAG_RO | FLAG_VOLUME | FLAG_SYSTEM))
  {
    return E_NONEXISTENT;
  }

  /* Check if directory not empty */
  dirItem = item;
  dirItem.index = 2; /* Exclude . and .. */
  dirItem.parent = item.cluster;
  res = fetchEntry(handle, &dirItem, 0);
  if (res == E_OK)
    return E_ERROR;
  else if (res != E_EOF)
    return res;

  /* Mark file table clusters as free */
  if ((res = freeChain(handle, item.cluster)) != E_OK)
    return res;

  if ((res = markFree(handle, &item)) != E_OK)
    return res;
  return E_OK;
}
#else
static enum result fatRemoveDir(void *object __attribute__((unused)),
    const char *path __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(DEBUG)
uint32_t countFree(void *object)
{
  struct FatHandle *handle = object;
  uint32_t freeCount = 0, current;
  uint32_t *count = malloc(sizeof(uint32_t) * handle->tableCount);
  uint8_t fat, i, j;
  uint16_t offset;

  if (!count)
    return 0; /* Memory allocation problem */
  for (fat = 0; fat < handle->tableCount; fat++)
  {
    count[fat] = 0;
    for (current = 0; current < handle->clusterCount; current++)
    {
      if (readSector(handle, handle->tableSector +
          (current >> TE_COUNT), handle->buffer, 1) != E_OK)
      {
        return freeCount;
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
        printf("FAT records count differs: %u and %u\n", count[i], count[j]);
      }
  freeCount = count[0];
  free(count);
  return freeCount;
}
#endif
