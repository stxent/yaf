/*
 * libyaf/fat32_inlines.h
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBYAF_FAT32_INLINES_H_
#define LIBYAF_FAT32_INLINES_H_
/*----------------------------------------------------------------------------*/
#include <libyaf/fat32_defs.h>
/*----------------------------------------------------------------------------*/
static inline bool clusterFree(uint32_t cluster)
{
  return !(cluster & 0x0FFFFFFFUL);
}
/*----------------------------------------------------------------------------*/
static inline bool clusterEoc(uint32_t cluster)
{
  return (cluster & 0x0FFFFFF8UL) == 0x0FFFFFF8UL;
}
/*----------------------------------------------------------------------------*/
static inline bool clusterUsed(uint32_t cluster)
{
  return (cluster & 0x0FFFFFFFUL) >= 0x00000002UL
      && (cluster & 0x0FFFFFFFUL) <= 0x0FFFFFEFUL;
}
/*----------------------------------------------------------------------------*/
static inline struct DirEntryImage *getEntry(struct CommandContext *context,
    uint16_t index)
{
  return (struct DirEntryImage *)(context->buffer + ENTRY_OFFSET(index));
}
/*----------------------------------------------------------------------------*/
/* Calculate first sector number of the cluster */
static inline uint32_t getSector(const struct FatHandle *handle,
    uint32_t cluster)
{
  return handle->dataSector + (((cluster) - 2) << handle->clusterSize);
}
/*----------------------------------------------------------------------------*/
/* File or directory entries per directory cluster */
static inline uint16_t nodeCount(const struct FatHandle *handle)
{
  return 1 << ENTRY_EXP << handle->clusterSize;
}
/*----------------------------------------------------------------------------*/
/* Calculate current sector in data cluster for read or write operations */
static inline uint8_t sectorInCluster(const struct FatHandle *handle,
    uint32_t offset)
{
  return (offset >> SECTOR_EXP) & ((1 << handle->clusterSize) - 1);
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
static inline bool hasLongName(const struct FatNode *node)
{
  return node->cluster != node->nameCluster || node->index != node->nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_THREADS
static inline void lockHandle(struct FatHandle *handle)
{
  if (queueCapacity(&handle->contextPool.queue) > 1)
    mutexLock(&handle->consistencyMutex);
}
#else
static inline void lockHandle(struct FatHandle *handle __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_THREADS
static inline void unlockHandle(struct FatHandle *handle)
{
  if (queueCapacity(&handle->contextPool.queue) > 1)
    mutexUnlock(&handle->consistencyMutex);
}
#else
static inline void unlockHandle(struct FatHandle *handle
    __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_THREADS
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
#ifdef CONFIG_FAT_THREADS
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
#endif /* LIBYAF_FAT32_INLINES_H_ */
