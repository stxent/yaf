/*
 * yaf/fat32_helpers.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_FAT32_HELPERS_H_
#define YAF_FAT32_HELPERS_H_
/*----------------------------------------------------------------------------*/
#include <yaf/fat32_defs.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
size_t computeShortNameLength(const struct DirEntryImage *);
void extractShortName(char *, const struct DirEntryImage *);
bool rawDateTimeToTimestamp(time64_t *, uint16_t, uint16_t);

#ifdef CONFIG_UNICODE
uint8_t calcLongNameChecksum(const char *, size_t);
void extractLongName(char16_t *, const struct DirEntryImage *);
#endif /* CONFIG_UNICODE */

#ifdef CONFIG_WRITE
void extractShortBasename(char *, const char *);
void fillDirEntry(struct DirEntryImage *, bool, FsAccess, uint32_t,
    time64_t);
bool fillShortName(char *, const char *, bool);
uint16_t timeToRawDate(const struct RtDateTime *);
uint16_t timeToRawTime(const struct RtDateTime *);
#endif /* CONFIG_WRITE */

#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
void fillLongName(struct DirEntryImage *, const char16_t *, size_t);
void fillLongNameEntry(struct DirEntryImage *, uint8_t, uint8_t,
    uint8_t);
size_t uniqueNameConvert(char *);
#endif /* CONFIG_UNICODE && CONFIG_WRITE */
/*----------------------------------------------------------------------------*/
static inline size_t calcLfnCount(size_t length)
{
  return (length - 1) / 2 / LFN_ENTRY_LENGTH;
}

/* Directory entries per directory cluster */
static inline uint16_t calcNodeCount(const struct FatHandle *handle)
{
  return 1 << ENTRY_EXP << handle->clusterSize;
}

/* Calculate the number of the first sector for a cluster */
static inline uint32_t calcSectorNumber(const struct FatHandle *handle,
    uint32_t cluster)
{
  return handle->dataSector + (((cluster) - 2) << handle->clusterSize);
}

static inline bool isClusterFree(uint32_t cluster)
{
  /* All reserved clusters and bad clusters are skipped */
  return !(cluster & 0x0FFFFFFFUL);
}

static inline bool isClusterUsed(uint32_t cluster)
{
  return (cluster & 0x0FFFFFFFUL) >= 0x00000002UL
      && (cluster & 0x0FFFFFFFUL) <= 0x0FFFFFEFUL;
}

static inline struct DirEntryImage *getDirEntry(struct CommandContext *context,
    uint16_t index)
{
  return (struct DirEntryImage *)(context->buffer + ENTRY_OFFSET(index));
}

static inline uint32_t makeClusterNumber(const struct DirEntryImage *entry)
{
  return ((uint32_t)fromLittleEndian16(entry->clusterHigh) << 16)
      | (uint32_t)fromLittleEndian16(entry->clusterLow);
}

/* Calculate current sector in data cluster for read or write operations */
static inline uint32_t sectorInCluster(const struct FatHandle *handle,
    uint32_t offset)
{
  return (offset >> SECTOR_EXP) & ((1U << handle->clusterSize) - 1);
}

#ifdef CONFIG_UNICODE
static inline bool hasLongName(const struct FatNode *node)
{
  return node->parentCluster != node->nameCluster
      || node->parentIndex != node->nameIndex;
}
#endif

static inline void lockHandle(struct FatHandle *handle)
{
#ifdef CONFIG_THREADS
  mutexLock(&handle->consistencyMutex);
#else
  (void)handle;
#endif
}

static inline void unlockHandle(struct FatHandle *handle)
{
#ifdef CONFIG_THREADS
  mutexUnlock(&handle->consistencyMutex);
#else
  (void)handle;
#endif
}
/*----------------------------------------------------------------------------*/
#endif /* YAF_FAT32_HELPERS_H_ */
