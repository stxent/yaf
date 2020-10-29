/*
 * yaf/fat32_inlines.h
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_FAT32_INLINES_H_
#define YAF_FAT32_INLINES_H_
/*----------------------------------------------------------------------------*/
#include <yaf/fat32_defs.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
static inline size_t calcLfnCount(size_t length)
{
  return (length - 1) / 2 / LFN_ENTRY_LENGTH;
}
/*----------------------------------------------------------------------------*/
/* File or directory entries per directory cluster */
static inline unsigned int calcNodeCount(const struct FatHandle *handle)
{
  return 1 << ENTRY_EXP << handle->clusterSize;
}
/*----------------------------------------------------------------------------*/
/* Calculate the number of the first sector for a cluster */
static inline uint32_t calcSectorNumber(const struct FatHandle *handle,
    uint32_t cluster)
{
  return handle->dataSector + (((cluster) - 2) << handle->clusterSize);
}
/*----------------------------------------------------------------------------*/
static inline bool isClusterFree(uint32_t cluster)
{
  /* All reserved clusters and bad clusters are skipped */
  return !(cluster & 0x0FFFFFFFUL);
}
/*----------------------------------------------------------------------------*/
static inline bool isClusterUsed(uint32_t cluster)
{
  return (cluster & 0x0FFFFFFFUL) >= 0x00000002UL
      && (cluster & 0x0FFFFFFFUL) <= 0x0FFFFFEFUL;
}
/*----------------------------------------------------------------------------*/
static inline struct DirEntryImage *getDirEntry(struct CommandContext *context,
    uint16_t index)
{
  return (struct DirEntryImage *)(context->buffer + ENTRY_OFFSET(index));
}
/*----------------------------------------------------------------------------*/
static inline uint32_t makeClusterNumber(const struct DirEntryImage *entry)
{
  return ((uint32_t)fromLittleEndian16(entry->clusterHigh) << 16)
      | (uint32_t)fromLittleEndian16(entry->clusterLow);
}
/*----------------------------------------------------------------------------*/
/* Calculate current sector in data cluster for read or write operations */
static inline unsigned int sectorInCluster(const struct FatHandle *handle,
    uint32_t offset)
{
  return (offset >> SECTOR_EXP) & ((1 << handle->clusterSize) - 1);
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
static inline bool hasLongName(const struct FatNode *node)
{
  return node->parentCluster != node->nameCluster
      || node->parentIndex != node->nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_THREADS
static inline void lockHandle(struct FatHandle *handle)
{
  if (pointerQueueCapacity(&handle->contextPool.queue) > 1)
    mutexLock(&handle->consistencyMutex);
}
#else
static inline void lockHandle(struct FatHandle *handle __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_THREADS
static inline void unlockHandle(struct FatHandle *handle)
{
  if (pointerQueueCapacity(&handle->contextPool.queue) > 1)
    mutexUnlock(&handle->consistencyMutex);
}
#else
static inline void unlockHandle(struct FatHandle *handle
    __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_THREADS
static inline void lockPools(struct FatHandle *handle)
{
  mutexLock(&handle->memoryMutex);
}
#else
static inline void lockPools(struct FatHandle *handle __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_THREADS
static inline void unlockPools(struct FatHandle *handle)
{
  mutexUnlock(&handle->memoryMutex);
}
#else
static inline void unlockPools(struct FatHandle *handle __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#endif /* YAF_FAT32_INLINES_H_ */
