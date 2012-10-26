/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <string.h>
#include "fat32.h"
/*----------------------------------------------------------------------------*/
#ifndef FAT_STATIC_ALLOC
#include <stdlib.h>
#endif
/*----------------------------------------------------------------------------*/
#ifdef FS_RTC_ENABLED
#include "rtc.h"
#endif
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#define FLAG_RO         (uint8_t)0x01 /* Read only */
#define FLAG_HIDDEN     (uint8_t)0x02
#define FLAG_SYSTEM     (uint8_t)0x0C /* System or volume label */
#define FLAG_DIR        (uint8_t)0x10 /* Subdirectory */
#define FLAG_ARCHIVE    (uint8_t)0x20
/*----------------------------------------------------------------------------*/
#define E_FLAG_EMPTY    (char)0xE5 /* Directory entry is free */
/*----------------------------------------------------------------------------*/
#define CLUSTER_EOC_VAL         0x0FFFFFF8
#define FILE_SIZE_MAX           0xFFFFFFFF
#define FILE_NAME_MAX           13 /* Name + dot + extension + null character */
/*----------------------------------------------------------------------------*/
/* File or directory entry size power */
#define E_POW                   (SECTOR_POW - 5)
/* Table entries per FAT sector power */
#define TE_COUNT                (SECTOR_POW - 2)
/* Table entry offset in FAT sector */
#define TE_OFFSET(arg)          (((arg) & ((1 << TE_COUNT) - 1)) << 2)
/* Directory entry position in cluster */
#define E_SECTOR(index)         ((index) >> E_POW)
/* Directory entry offset in sector */
#define E_OFFSET(index)         (((index) << 5) & (SECTOR_SIZE - 1))
/*----------------------------------------------------------------------------*/
struct FatFile
{
  uint32_t cluster; /* First cluster of file data */
  uint8_t currentSector; /* Sector in current cluster */
  uint32_t currentCluster;
#ifdef FS_WRITE_ENABLED
  uint16_t parentIndex; /* Entry position in parent cluster */
  uint32_t parentCluster; /* Directory cluster where entry located */
#endif
};
/*----------------------------------------------------------------------------*/
struct FatDir
{
  /* Filesystem-specific fields */
  uint32_t cluster; /* First cluster of directory data */
  uint16_t currentIndex; /* Entry in current cluster */
  uint32_t currentCluster;
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  /* Filesystem-specific fields */
  uint8_t clusterSize; /* Sectors per cluster power */
  uint32_t currentSector, rootCluster, dataSector, tableSector;
#ifdef FS_WRITE_ENABLED
  uint8_t tableCount; /* FAT tables count */
  uint32_t tableSize; /* Size in sectors of each FAT table */
  uint16_t infoSector;
  uint32_t clusterCount; /* Number of clusters */
  uint32_t lastAllocated; /* Last allocated cluster */
#endif
};
/*----------------------------------------------------------------------------*/
/* Filesystem object describing directory or file */
struct FatObject
{
  uint8_t attribute; /* File or directory attributes */
  uint16_t index; /* Entry position in parent cluster */
  uint32_t cluster; /* First cluster of entry */
  uint32_t parent; /* Directory cluster where entry located */
  uint32_t size; /* File size or zero for directories */
  char name[FILE_NAME_MAX];
};
/*----------------------------------------------------------------------------*/
/* Structure directory entry located in memory buffer */
struct dirEntryImage
{
  union
  {
    char filename[11];
    struct
    {
      char name[8];
      char extension[3];
    } __attribute__((packed));
  };
  uint8_t flags;
  char unused[8];
  uint16_t clusterHigh; /* Starting cluster high word */
  uint16_t time;
  uint16_t date;
  uint16_t clusterLow; /* Starting cluster low word */
  uint32_t size;
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
/* Structure of boot sector located in memory buffer */
struct bootSectorImage
{
  char unused0[11];
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectors;
  uint8_t fatCopies;
  char unused1[15];
  uint32_t partitionSize; /* Sectors per partition */
  uint32_t fatSize; /* Sectors per FAT record */
  char unused2[4];
  uint32_t rootCluster; /* Root directory cluster */
  uint16_t infoSector; /* Sector number for information sector */
  char unused3[460];
  uint16_t bootSignature;
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
/* Structure info sector located in memory buffer */
struct infoSectorImage
{
  uint32_t firstSignature;
  char unused0[480];
  uint32_t infoSignature;
  uint32_t freeClusters;
  uint32_t lastAllocated;
  char unused1[14];
  uint16_t bootSignature;
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
/*------------------Inline functions------------------------------------------*/
static inline bool clusterFree(uint32_t);
static inline bool clusterEOC(uint32_t);
static inline bool clusterUsed(uint32_t);
static inline uint32_t getSector(struct FsHandle *sys, uint32_t);
static inline uint16_t entryCount(struct FsHandle *sys);
/*----------------------------------------------------------------------------*/
static inline bool clusterFree(uint32_t cluster)
{
  return (cluster & 0x0FFFFFFF) == 0x00000000;
}
/*----------------------------------------------------------------------------*/
static inline bool clusterEOC(uint32_t cluster)
{
  return (cluster & 0x0FFFFFF8) == 0x0FFFFFF8;
}
/*----------------------------------------------------------------------------*/
static inline bool clusterUsed(uint32_t cluster)
{
  return (cluster & 0x0FFFFFFF) >= 0x00000002 &&
      (cluster & 0x0FFFFFFF) <= 0x0FFFFFEF;
}
/*----------------------------------------------------------------------------*/
/* Calculate sector position from cluster */
//FIXME rewrite
static inline uint32_t getSector(struct FsHandle *sys, uint32_t cluster)
{
  return ((struct FatHandle *)sys->data)->dataSector + (((cluster) - 2) << ((struct FatHandle *)sys->data)->clusterSize);
}
/*----------------------------------------------------------------------------*/
/* File or directory entries per directory cluster */
//FIXME rewrite
static inline uint16_t entryCount(struct FsHandle *sys)
{
  return 1 << E_POW << ((struct FatHandle *)sys->data)->clusterSize;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static enum fsResult readSector(struct FsHandle *, uint32_t);
static enum fsResult burstReadSector(struct FsHandle *, uint32_t,
    uint8_t *, uint8_t);
static enum fsResult fetchEntry(struct FsHandle *, struct FatObject *);
static const char *followPath(struct FsHandle *, struct FatObject *,
    const char *);
static enum fsResult getNextCluster(struct FsHandle *, uint32_t *);
static const char *getChunk(const char *, char *);
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult writeSector(struct FsHandle *, uint32_t);
static enum fsResult burstWriteSector(struct FsHandle *, uint32_t,
    const uint8_t *, uint8_t);
static enum fsResult truncate(struct FsFile *);
static enum fsResult freeChain(struct FsHandle *, uint32_t);
static enum fsResult allocateCluster(struct FsHandle *, uint32_t *);
static enum fsResult createEntry(struct FsHandle *, struct FatObject *,
    const char *);
static enum fsResult updateTable(struct FsHandle *, uint32_t);
#endif
/*----------------------------------------------------------------------------*/
enum fsResult fatMount(struct FsHandle *, struct FsDevice *);
void fatUmount(struct FsHandle *);
enum fsResult fatStat(struct FsHandle *, const char *, struct FsStat *);
enum fsResult fatOpen(struct FsHandle *, struct FsFile *, const char *,
    enum fsMode);
enum fsResult fatOpenDir(struct FsHandle *, struct FsDir *, const char *);
void fatClose(struct FsFile *);
bool fatEof(struct FsFile *);
enum fsResult fatSeek(struct FsFile *, uint32_t);
enum fsResult fatRead(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
void fatCloseDir(struct FsDir *);
enum fsResult fatReadDir(struct FsDir *, char *);
#ifdef FS_WRITE_ENABLED
enum fsResult fatRemove(struct FsHandle *, const char *);
enum fsResult fatMove(struct FsHandle *, const char *, const char *);
enum fsResult fatMakeDir(struct FsHandle *, const char *);
enum fsResult fatWrite(struct FsFile *, const uint8_t *, uint16_t, uint16_t *);
#endif
/*----------------------------------------------------------------------------*/
void fat32Init(struct FsHandle *sys)
{
  sys->data = 0;
  sys->mount = fatMount;
  sys->umount = fatUmount;
  sys->stat = fatStat;
  sys->open = fatOpen;
  sys->openDir = fatOpenDir;

#ifdef FS_WRITE_ENABLED
  sys->remove = fatRemove;
  sys->move = fatMove;
  sys->makeDir = fatMakeDir;
#else
  sys->remove = 0;
  sys->move = 0;
  sys->makeDir = 0;
#endif
#ifdef DEBUG
  printf("Size of FatHandle: %d\n", (int)sizeof(struct FatHandle));
  printf("Size of FatFile:   %d\n", (int)sizeof(struct FatFile));
  printf("Size of FatDir:    %d\n", (int)sizeof(struct FatDir));
#endif
}
/*----------------------------------------------------------------------------*/
enum fsResult fatMount(struct FsHandle *sys, struct FsDevice *dev)
{
  struct FatHandle *handle;
  uint16_t tmp;
  struct bootSectorImage *boot;
#ifdef FS_WRITE_ENABLED
  struct infoSectorImage *info;
#endif

#ifndef FAT_STATIC_ALLOC
  handle = malloc(sizeof(struct FatHandle)); //FIXME move
#endif
  sys->device = dev;
  sys->data = (void *)handle; //FIXME rewrite readSector to fetch FatHandle
  handle->currentSector = 0;
  /* Read first sector */
  if (readSector(sys, 0))
  {
    free(handle);
    return FS_READ_ERROR;
  }
  boot = (struct bootSectorImage *)sys->device->buffer;
  /* Check boot sector signature (55AA at 0x01FE) */
  /* TODO move signatures to macro */
  if (boot->bootSignature != 0xAA55)
  {
    free(handle);
    return FS_DEVICE_ERROR;
  }
  /* Check sector size, fixed size of 2^SECTOR_POW allowed */
  if (boot->bytesPerSector != SECTOR_SIZE)
  {
    free(handle);
    return FS_DEVICE_ERROR;
  }
  /* Calculate sectors per cluster count */
  tmp = boot->sectorsPerCluster;
  handle->clusterSize = 0;
  while (tmp >>= 1)
    handle->clusterSize++;

  handle->tableSector = boot->reservedSectors;
  handle->dataSector = handle->tableSector + boot->fatCopies * boot->fatSize;
  handle->rootCluster = boot->rootCluster;
#ifdef FS_WRITE_ENABLED
  handle->tableCount = boot->fatCopies;
  handle->tableSize = boot->fatSize;
  handle->clusterCount = ((boot->partitionSize -
      handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = boot->infoSector;
#ifdef DEBUG
  printf("Partition sectors count: %d\n", boot->partitionSize);
#endif

  if (readSector(sys, handle->infoSector))
  {
    free(handle);
    return FS_READ_ERROR;
  }
  info = (struct infoSectorImage *)sys->device->buffer;
  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
  {
    free(handle);
    return FS_DEVICE_ERROR;
  }
  handle->lastAllocated = info->lastAllocated;
#ifdef DEBUG
  printf("Free clusters: %d\n", info->freeClusters);
#endif
#endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
void fatUmount(struct FsHandle *sys)
{
#ifndef FAT_STATIC_ALLOC
  free(sys->data); //FIXME move
#endif
  sys->data = 0;
  sys->device = 0;
}
/*----------------------------------------------------------------------------*/
static enum fsResult getNextCluster(struct FsHandle *sys, uint32_t *cluster)
{
  uint32_t nextCluster;

  if (readSector(sys, ((struct FatHandle *)sys->data)->tableSector +
      (*cluster >> TE_COUNT)))
  {
    return FS_READ_ERROR;
  }
  nextCluster = *(uint32_t *)(sys->device->buffer + TE_OFFSET(*cluster));
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
#ifdef FS_WRITE_ENABLED
#ifdef DEBUG
uint32_t countFree(struct FsHandle *sys)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  uint32_t res, current;
  uint32_t *count = (uint32_t *)malloc(sizeof(uint32_t) * handle->tableCount);
  uint8_t fat, i, j;
  uint16_t offset;

  for (fat = 0; fat < handle->tableCount; fat++)
  {
    count[fat] = 0;
    for (current = 0; current < handle->clusterCount; current++)
    {
      if (readSector(sys, handle->tableSector + (current >> TE_COUNT)))
        return FS_READ_ERROR;
      offset = (current & ((1 << TE_COUNT) - 1)) << 2;
      if (clusterFree(*(uint32_t *)(sys->device->buffer + offset)))
        count[fat]++;
    }
  }
  for (i = 0; i < handle->tableCount; i++)
    for (j = 0; j < handle->tableCount; j++)
      if (i != j && count[i] != count[j])
      {
        printf("FAT records differ: %d and %d\n", count[i], count[j]);
      }
  res = count[0];
  free(count);
  return res;
}
#endif
#endif
/*----------------------------------------------------------------------------*/
static enum fsResult readSector(struct FsHandle *sys, uint32_t sector)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  if (sector && sector == handle->currentSector)
    return FS_OK;
  if (sys->device->read(sys->device, sector, sys->device->buffer, 1))
    return FS_READ_ERROR;
  handle->currentSector = sector;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
static enum fsResult burstReadSector(struct FsHandle *sys,
    uint32_t sector, uint8_t *buffer, uint8_t count)
{
  if (sys->device->read(sys->device, sector, buffer, count))
    return FS_READ_ERROR;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult writeSector(struct FsHandle *sys, uint32_t sector)
{
  if (sys->device->write(sys->device, sector, sys->device->buffer, 1))
    return FS_WRITE_ERROR;
  ((struct FatHandle *)sys->data)->currentSector = sector;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult burstWriteSector(struct FsHandle *sys, uint32_t sector,
    const uint8_t *buffer, uint8_t count)
{
  if (sys->device->write(sys->device, sector, buffer, count))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
/* Copy current sector into FAT sectors located at offset */
#ifdef FS_WRITE_ENABLED
static enum fsResult updateTable(struct FsHandle *sys, uint32_t offset)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  uint8_t fat;

  for (fat = 0; fat < handle->tableCount; fat++)
  {
    if (writeSector(sys, handle->tableSector +
        (uint32_t)fat * handle->tableSize + offset))
    {
      return FS_WRITE_ERROR;
    }
  }
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult allocateCluster(struct FsHandle *sys,
    uint32_t *cluster)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct infoSectorImage *info;
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
    if (readSector(sys, handle->tableSector + (current >> TE_COUNT)))
      return FS_READ_ERROR;
    offset = (current & ((1 << TE_COUNT) - 1)) << 2;
    /* Is cluster free */
    if (clusterFree(*(uint32_t *)(sys->device->buffer + offset)))
    {
      *(uint32_t *)(sys->device->buffer + offset) = CLUSTER_EOC_VAL;
      if ((!*cluster || (*cluster >> TE_COUNT != current >> TE_COUNT)) &&
          updateTable(sys, current >> TE_COUNT))
      {
        return FS_WRITE_ERROR;
      }
      if (*cluster)
      {
        if (readSector(sys, handle->tableSector + (*cluster >> TE_COUNT)))
          return FS_READ_ERROR;
        *(uint32_t *)(sys->device->buffer + TE_OFFSET(*cluster)) = current;
        if (updateTable(sys, *cluster >> TE_COUNT))
          return FS_WRITE_ERROR;
      }
#ifdef DEBUG
      printf("Allocated new cluster: %d, reference %d\n", current, *cluster);
#endif
      *cluster = current;
      /* Update information sector */
      if (readSector(sys, handle->infoSector))
        return FS_READ_ERROR;
      info = (struct infoSectorImage *)sys->device->buffer;
      /* Set last allocated cluster */
      info->lastAllocated = current;
      handle->lastAllocated = current;
      /* Update free clusters count */
      info->freeClusters--;
      if (writeSector(sys, handle->infoSector))
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
/* Members entry->index and entry->parent have to be initialized */
static enum fsResult fetchEntry(struct FsHandle *sys, struct FatObject *entry)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct dirEntryImage *ptr;
  uint32_t sector;

  entry->attribute = 0;
  entry->cluster = 0;
  entry->size = 0;
  while (1)
  {
    if (entry->index >= entryCount(sys))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      if (getNextCluster(sys, &entry->parent))
        return FS_READ_ERROR;
      entry->index = 0;
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if (readSector(sys, sector))
      return FS_READ_ERROR;
    ptr = (struct dirEntryImage *)(sys->device->buffer +
        E_OFFSET(entry->index));
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
    item->cluster = ((struct FatHandle *)sys->data)->rootCluster;
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
enum fsResult fatStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  const char *followedPath;
  struct FatObject item;
  uint32_t sector;
#ifdef FS_RTC_ENABLED
  struct dirEntryImage *ptr;
  struct Time tm;
#endif

  while (*path && (followedPath = followPath(sys, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
    return FS_NOT_FOUND;

  sector = getSector(sys, item.parent) + E_SECTOR(item.index);
  if (readSector(sys, sector))
    return FS_READ_ERROR;
#ifdef FS_RTC_ENABLED
  ptr = (struct dirEntryImage *)(sys->device->buffer +
      E_OFFSET(item.index));
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

#ifdef FS_RTC_ENABLED
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
enum fsResult fatOpen(struct FsHandle *sys, struct FsFile *file,
    const char *path, enum fsMode mode)
{
  struct FatFile *fh;
  const char *followedPath;
  struct FatObject item;

  file->data = 0;
  file->descriptor = 0;
  file->mode = mode;
  while (*path && (followedPath = followPath(sys, &item, path)))
    path = followedPath;
  /* Non-zero when entry not found */
  if (*path)
  {
#ifdef FS_WRITE_ENABLED
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
#ifdef FS_WRITE_ENABLED
  /* Attempt to write into read-only file */
  if ((item.attribute & FLAG_RO) && (mode == FS_WRITE || mode == FS_APPEND))
    return FS_ERROR;
#endif
  file->position = 0;
  file->size = item.size;

#ifndef FAT_STATIC_ALLOC
  fh = malloc(sizeof(struct FsFile));
#endif
  fh->cluster = item.cluster;
  fh->currentCluster = item.cluster;
  fh->currentSector = 0;

#ifdef FS_WRITE_ENABLED
  fh->parentCluster = item.parent;
  fh->parentIndex = item.index;

  file->descriptor = sys;
  if (mode == FS_WRITE && !*path && file->size &&
      truncate(file) != FS_OK)
  {
    file->descriptor = 0;
    free(fh);
    return FS_ERROR;
  }
  /* In append mode file pointer moves to end of file */
  if (mode == FS_APPEND && fsSeek(file, file->size) != FS_OK)
  {
    file->descriptor = 0;
    free(fh);
    return FS_ERROR;
  }
#endif

  file->data = (void *)fh;
  file->close = fatClose;
  file->eof = fatEof;
  file->seek = fatSeek;
  file->read = fatRead;
#ifdef FS_WRITE_ENABLED
  file->write = fatWrite;
#else
  file->write = 0;
#endif
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
bool fatEof(struct FsFile *file)
{
  return (file->position >= file->size);
}
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult freeChain(struct FsHandle *sys, uint32_t cluster)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct infoSectorImage *info;
  uint8_t *dbuf = sys->device->buffer;
  uint16_t freeCount = 0;
  uint32_t current = cluster;
  uint32_t next;

  if (!current)
    return FS_OK; /* Already empty */
  while (clusterUsed(current))
  {
    /* Get FAT sector with next cluster value */
    if (readSector(sys, handle->tableSector + (current >> TE_COUNT)))
      return FS_READ_ERROR;
    /* Free cluster */
    next = *(uint32_t *)(dbuf + TE_OFFSET(current));
    *(uint32_t *)(dbuf + TE_OFFSET(current)) = 0;
#ifdef DEBUG
    if (current >> TE_COUNT != next >> TE_COUNT)
    {
      printf("FAT sectors differ, next: %d (0x%X), current %d\n",
             (next >> TE_COUNT), (next >> TE_COUNT),
             (current >> TE_COUNT));
    }
    printf("Cleared cluster: %d\n", current);
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
  if (readSector(sys, handle->infoSector))
    return FS_READ_ERROR;
  info = (struct infoSectorImage *)dbuf;
  /* Set free clusters count */
  info->freeClusters += freeCount;
  if (writeSector(sys, handle->infoSector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
static enum fsResult truncate(struct FsFile *file)
{
  struct FsHandle *sys = file->descriptor;
  struct FatFile *fh = (struct FatFile *)file->data;
  struct dirEntryImage *ptr;
  uint32_t current;

  if (file->mode != FS_WRITE && file->mode != FS_APPEND)
    return FS_ERROR;
  if (freeChain(sys, fh->cluster) != FS_OK)
    return FS_ERROR;
  current = getSector(sys, fh->parentCluster) + E_SECTOR(fh->parentIndex);
  if (readSector(sys, current))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  ptr = (struct dirEntryImage *)(sys->device->buffer +
      E_OFFSET(fh->parentIndex));
  /* Update size and first cluster */
  ptr->size = 0;
  ptr->clusterHigh = 0;
  ptr->clusterLow = 0;
#ifdef FS_RTC_ENABLED
  /* Update last modified date */
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(sys, current))
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
#ifdef FS_WRITE_ENABLED
/* Create new entry inside entry->parent chain */
/* Members entry->parent and entry->attribute have to be initialized */
static enum fsResult createEntry(struct FsHandle *sys,
    struct FatObject *entry, const char *name)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct dirEntryImage *ptr;
  uint8_t pos;
//  uint16_t clusterCount = 0; /* Followed clusters count */
  uint32_t sector;

  entry->cluster = 0;
  entry->index = 0;
  for (pos = 0; *(name + pos); pos++)
  {
    if (name[pos] == '/')
      return FS_ERROR; /* One of directories in path does not exist */
  }
  while (1)
  {
    if (entry->index >= entryCount(sys))
    {
      /* Try to get next cluster or allocate new cluster for directory */
      /* Max directory size is 2^16 entries */
      //TODO Add file limit
      //if (getNextCluster(sys, &(entry->parent)) && (clusterCount < (1 << (16 - ENTRY_COUNT - sys->clusterSize))))
      if (getNextCluster(sys, &entry->parent))
      {
        if (allocateCluster(sys, &entry->parent))
          return FS_ERROR;
        else
        {
          sector = getSector(sys, entry->parent);
          memset(sys->device->buffer, 0, SECTOR_SIZE);
          for (pos = 0; pos < (1 << handle->clusterSize); pos++)
          {
            if (writeSector(sys, sector + pos))
              return FS_WRITE_ERROR;
          }
        }
      }
      else
        return FS_ERROR; /* Directory full */
      entry->index = 0;
//      clusterCount++; //FIXME remove?
    }
    sector = handle->dataSector + E_SECTOR(entry->index) +
        ((entry->parent - 2) << handle->clusterSize);
    if (readSector(sys, sector))
      return FS_ERROR;
    ptr = (struct dirEntryImage *)(sys->device->buffer +
        E_OFFSET(entry->index));
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
#ifdef FS_RTC_ENABLED
  /* Last modified time and date */
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(sys, sector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
void fatClose(struct FsFile *file)
{
#ifndef FAT_STATIC_ALLOC
  free(file->data);
#endif
  file->data = 0;
  file->descriptor = 0;
  file->close = 0;
  file->eof = 0;
  file->seek = 0;
  file->read = 0;
  file->write = 0;
}
/*----------------------------------------------------------------------------*/
//TODO test behavior
enum fsResult fatSeek(struct FsFile *file, uint32_t pos)
{
  struct FatHandle *handle = (struct FatHandle *)file->descriptor->data;
  struct FatFile *fh = (struct FatFile *)file->data;
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
  fh->currentSector = (pos >> SECTOR_POW) & //TODO add macro?
      ((1 << handle->clusterSize) - 1);
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
enum fsResult fatWrite(struct FsFile *file, const uint8_t *buffer,
    uint16_t count, uint16_t *result)
{
  struct FsHandle *sys = file->descriptor;
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct FatFile *fh = (struct FatFile *)file->data;
  struct dirEntryImage *ptr;
  uint16_t chunk, offset, written = 0;
  uint32_t tmpSector;

  if (file->mode != FS_APPEND && file->mode != FS_WRITE)
    return FS_ERROR;
  if (!file->size)
  {
    if (allocateCluster(sys, &fh->cluster))
      return FS_ERROR;
    fh->currentCluster = fh->cluster;
  }
  /* Checking file size limit (2 GiB) */
  if (file->size + count > FILE_SIZE_MAX)
    count = FILE_SIZE_MAX - file->size;

  while (count)
  {
    if (fh->currentSector >= (1 << handle->clusterSize))
    {
      if (allocateCluster(sys, &fh->currentCluster))
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
      tmpSector = getSector(sys, fh->currentCluster) + fh->currentSector;
      if (readSector(sys, tmpSector))
        return FS_READ_ERROR;
      memcpy(sys->device->buffer + offset,
          buffer + written, chunk);
      if (writeSector(sys, tmpSector))
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
      printf("Burst write position %d, chunk size %d, sector count %d\n",
          written, chunk, chunk >> SECTOR_POW);
#endif
      //TODO rename file
      if (burstWriteSector(sys, getSector(sys, fh->currentCluster) +
          fh->currentSector, buffer + written, chunk >> SECTOR_POW))
      {
        return FS_READ_ERROR;
      }
      fh->currentSector += chunk >> SECTOR_POW;
    }

    written += chunk;
    count -= chunk;
  }

  tmpSector = getSector(sys, fh->parentCluster) + E_SECTOR(fh->parentIndex);
  if (readSector(sys, tmpSector))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  ptr = (struct dirEntryImage *)(sys->device->buffer +
      E_OFFSET(fh->parentIndex));
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
#ifdef FS_RTC_ENABLED
  /* Update last modified date */
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif
  if (writeSector(sys, tmpSector))
    return FS_WRITE_ERROR;
  *result = written;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
enum fsResult fatRead(struct FsFile *file, uint8_t *buffer,
    uint16_t count, uint16_t *result)
{
  struct FsHandle *sys = file->descriptor;
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct FatFile *fh = (struct FatFile *)file->data;
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
      if (getNextCluster(sys, &fh->currentCluster))
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
      if (readSector(sys, getSector(sys,
          fh->currentCluster) + fh->currentSector))
        return FS_READ_ERROR;
      memcpy(buffer + read, sys->device->buffer + offset,
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
      printf("Burst read position %d, chunk size %d, sector count %d\n",
          read, chunk, chunk >> SECTOR_POW);
#endif
      if (burstReadSector(sys, getSector(sys, fh->currentCluster) +
          fh->currentSector, buffer + read, chunk >> SECTOR_POW))
        return FS_READ_ERROR;
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
#ifdef FS_WRITE_ENABLED
enum fsResult fatRemove(struct FsHandle *sys, const char *path)
{
  struct dirEntryImage *ptr;
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

  /* Sector in FAT with entry description */
  tmp = getSector(sys, parent) + E_SECTOR(index);
  if (readSector(sys, tmp))
    return FS_READ_ERROR;
  /* Mark entry as free */
  ptr = (struct dirEntryImage *)(sys->device->buffer + E_OFFSET(index));
  ptr->name[0] = E_FLAG_EMPTY;
  if (writeSector(sys, tmp))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
enum fsResult fatOpenDir(struct FsHandle *sys, struct FsDir *dir,
    const char *path)
{
  struct FatDir *dh;
  struct FatObject item;

  dir->descriptor = 0;
  while (path && *path)
    path = followPath(sys, &item, path);
  if (!path)
    return FS_NOT_FOUND;
  /* Hidden, system, volume name or not directory */
  if (!(item.attribute & FLAG_DIR) || item.attribute & FLAG_SYSTEM)
    return FS_NOT_FOUND;

#ifndef FAT_STATIC_ALLOC
  dh = malloc(sizeof(struct FsDir));
#endif
  dh->cluster = item.cluster;
  dh->currentCluster = item.cluster;
  dh->currentIndex = 0;

  dir->descriptor = sys;
  dir->data = (void *)dh;
  dir->close = fatCloseDir;
  dir->read = fatReadDir;
  return FS_OK;
}
/*----------------------------------------------------------------------------*/
void fatCloseDir(struct FsDir *dir)
{
#ifndef FAT_STATIC_ALLOC
  free(dir->data);
#endif
  dir->data = 0;
  dir->descriptor = 0;
  dir->close = 0;
  dir->read = 0;
}
/*----------------------------------------------------------------------------*/
enum fsResult fatReadDir(struct FsDir *dir, char *name)
{
  struct FatDir *dh = (struct FatDir *)dir->data;
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
#ifdef FS_WRITE_ENABLED
enum fsResult fatMakeDir(struct FsHandle *sys, const char *path)
{
  struct FatHandle *handle = (struct FatHandle *)sys->data;
  struct dirEntryImage *ptr;
  struct FatObject item;
  uint32_t tmpSector, parent = handle->rootCluster;
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
  if (createEntry(sys, &item, path) ||
      allocateCluster(sys, &item.cluster))
    return FS_WRITE_ERROR;
  tmpSector = getSector(sys, item.parent) + E_SECTOR(item.index);
  if (readSector(sys, tmpSector))
    return FS_READ_ERROR;
  ptr = (struct dirEntryImage *)(sys->device->buffer +
      E_OFFSET(item.index));
  ptr->clusterHigh = item.cluster >> 16;
  ptr->clusterLow = item.cluster;
  if (writeSector(sys, tmpSector))
    return FS_WRITE_ERROR;
  tmpSector = getSector(sys, item.cluster);

  /* Fill cluster with zeros */
  memset(sys->device->buffer, 0, SECTOR_SIZE);
  for (pos = (1 << handle->clusterSize) - 1; pos > 0; pos--)
  {
    if (writeSector(sys, tmpSector + pos))
      return FS_WRITE_ERROR;
  }

  /* Current directory entry . */
  ptr = (struct dirEntryImage *)sys->device->buffer;
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
#ifdef FS_RTC_ENABLED
  ptr->time = (ptr - 1)->time;
  ptr->date = (ptr - 1)->date;
#endif

  if (writeSector(sys, tmpSector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
enum fsResult fatMove(struct FsHandle *sys, const char *src,
    const char *dest)
{
  uint8_t attribute/*, counter*/;
  uint16_t index;
  uint32_t parent, cluster, size, tmpSector;
  struct dirEntryImage *ptr;
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
    tmpSector = getSector(sys, parent) + ENTRY_SECTOR(index);
    if (readSector(sys, tmpSector))
      return FS_READ_ERROR;
    ptr = sys->buffer + ENTRY_OFFSET(index);
    memset(ptr, 0x20, 11);
    for (counter = 0; *dest && *dest != '.' && counter < 8; counter++)
      *(char *)(ptr + counter) = *dest++;
    if (!(attribute & FLAG_DIR) && *dest == '.')
    {
      for (counter = 8, name++; *dest && (counter < 11); counter++)
        *(char *)(ptr + counter) = *dest++;
    }
    if (writeSector(sys, tmpSector))
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

  tmpSector = getSector(sys, parent) + E_SECTOR(index);
  if (readSector(sys, tmpSector))
    return FS_READ_ERROR;
  /* Set old entry as removed */
  *(uint8_t *)(sys->device->buffer + E_OFFSET(index)) = E_FLAG_EMPTY;
  if (writeSector(sys, tmpSector))
    return FS_WRITE_ERROR;

  tmpSector = getSector(sys, item.parent) + E_SECTOR(item.index);
  if (readSector(sys, tmpSector))
    return FS_READ_ERROR;
  ptr = (struct dirEntryImage *)(sys->device->buffer +
      E_OFFSET(item.index));
  ptr->clusterHigh = cluster >> 16;
  ptr->clusterLow = cluster;
  ptr->size = size;
  if (writeSector(sys, tmpSector))
    return FS_WRITE_ERROR;

  return FS_OK;
}
#endif
