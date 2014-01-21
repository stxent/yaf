/*
 * fat32_defs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef FAT32_DEFS_H_
#define FAT32_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <macro.h>
#include "fs.h"
/*----------------------------------------------------------------------------*/
#ifdef FAT_TIME
#include "rtc.h"
#endif
#ifdef FAT_LFN
#include "unicode.h"
#endif
/*----------------------------------------------------------------------------*/
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512 */
#define SECTOR_POW              9 /* Sector size in power of 2 */
#define SECTOR_SIZE             (1 << SECTOR_POW) /* Sector size in bytes */
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Buffer size in code points for internal long file name processing */
#define FILE_NAME_BUFFER        FS_NAME_LENGTH
/* Long file name entry length: 13 UTF-16LE characters */
#define LFN_ENTRY_LENGTH        13
#endif
/*----------------------------------------------------------------------------*/
#define FLAG_RO                 BIT(0) /* Read only */
#define FLAG_HIDDEN             BIT(1)
#define FLAG_SYSTEM             BIT(2) /* System entry */
#define FLAG_VOLUME             BIT(3) /* Volume name */
#define FLAG_DIR                BIT(4) /* Subdirectory */
#define FLAG_ARCHIVED           BIT(5)
#define LFN_LAST                BIT(6) /* Last LFN entry */
#define LFN_DELETED             BIT(7) /* Deleted LFN entry */
#define MASK_LFN                BIT_FIELD(0x0F, 0) /* Long file name chunk */
/*----------------------------------------------------------------------------*/
#define E_FLAG_EMPTY            (char)0xE5 /* Directory entry is free */
/*----------------------------------------------------------------------------*/
#define CLUSTER_EOC_VAL         0x0FFFFFF8UL
#define FILE_SIZE_MAX           0xFFFFFFFFUL
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
struct FatHandle
{
  struct FsHandle parent;

  struct FsHandle *head;
  struct Interface *interface;

  uint8_t *buffer;
#ifdef FAT_LFN
  char16_t *nameBuffer;
#endif
  uint32_t currentSector, dataSector, rootCluster, tableSector;
  uint32_t bufferedSector; /* Number of sector that is stored in buffer */
#ifdef FAT_WRITE
  uint32_t tableSize; /* Size in sectors of each FAT table */
  uint32_t clusterCount; /* Number of clusters */
  uint32_t lastAllocated; /* Last allocated cluster */
  uint16_t infoSector;
  uint8_t tableCount; /* FAT tables count */
#endif
  /* Sectors per cluster, in FAT32 cluster may contain up to 128 sectors */
  uint8_t clusterSize; /* Power of 2 */
};
/*----------------------------------------------------------------------------*/
struct FatNodeConfig
{
  struct FsHandle *handle;
};
/*----------------------------------------------------------------------------*/
struct FatNode
{
  struct FsNode parent;

  struct FsHandle *handle;
  uint32_t cluster; /* Directory cluster where the entry is located */
  uint32_t payload; /* First cluster of the entry */
  uint32_t size; /* File size or zero for directories */
#ifdef FAT_LFN
  uint32_t nameCluster; /* Directory cluster of the first name entry */
  uint16_t nameIndex; /* First name entry position in the parent cluster */
#endif
  uint16_t index; /* Entry position in the parent cluster */
  access_t access; /* Access rights */
  enum fsNodeType type; /* Node type */
};
/*----------------------------------------------------------------------------*/
struct FatDirConfig
{
  struct FsNode *node;
};
/*----------------------------------------------------------------------------*/
struct FatDir
{
  struct FsEntry parent;

  struct FsHandle *handle;
  uint32_t payload; /* First cluster of directory data */
  uint32_t currentCluster; /* Current cluster inside data chain */
  uint16_t currentIndex; /* Index of an entry in current cluster */
};
/*----------------------------------------------------------------------------*/
struct FatFileConfig
{
  struct FsNode *node;
};
/*----------------------------------------------------------------------------*/
struct FatFile
{
  struct FsEntry parent;

  struct FsHandle *handle;
  uint32_t size; /* File size */
  uint32_t payload; /* First cluster of file data */
  uint32_t position; /* Position in file */
  uint32_t currentCluster; /* Current cluster inside data chain */
#ifdef FAT_WRITE
  uint32_t parentCluster; /* Directory cluster where entry located */
  uint16_t parentIndex; /* Entry position in parent cluster */
#endif
  access_t access; /* Access rights to the file */
};
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Long file name entry descriptor */
struct LfnObject
{
  uint32_t parent; /* Directory cluster where entry located */
  uint16_t index; /* Entry position in parent cluster */
  uint8_t checksum, length;
};
#endif
/*------------------FAT32 memory structures-----------------------------------*/
/* Directory entry or long file name entry*/
struct DirEntryImage /* TODO Rename some fields */
{
  union
  {
    char filename[11];
    struct
    {
      char name[8];
      char extension[3];
    } __attribute__((packed));
    struct
    {
      uint8_t ordinal; /* LFN entry ordinal */
#ifdef FAT_LFN
      char16_t longName0[5]; /* First part of unicode name */
#else
      char unused2[10];
#endif
    } __attribute__((packed));
  };
  uint8_t flags;
  char unused0;
#ifdef FAT_LFN
  uint8_t checksum; /* LFN entry checksum, not used in directory entries */
#else
  char unused3;
#endif
  union
  {
    /* Directory entry fields */
    struct
    {
      char unused1[6];
      uint16_t clusterHigh; /* Starting cluster high word */
      uint16_t time;
      uint16_t date;
      uint16_t clusterLow; /* Starting cluster low word */
      uint32_t size;
    } __attribute__((packed));
#ifdef FAT_LFN
    /* Long file name entry fields */
    struct
    {
      char16_t longName1[6];
      char unused2[2];
      char16_t longName2[2];
    } __attribute__((packed));
#endif
  };
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
/* Boot sector */
struct BootSectorImage
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
/* Info sector */
struct InfoSectorImage
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
static void extractShortName(const struct DirEntryImage *, char *);
static const char *getChunk(const char *, char *);
static enum result getNextCluster(struct FatHandle *, uint32_t *);
static enum result fetchEntry(struct FatNode *);
static enum result fetchNode(struct FatNode *, char *);
static const char *followPath(struct FatNode *, const char *,
    const struct FatNode *);
static enum result readSector(struct FatHandle *, uint32_t, uint8_t *,
    uint32_t);
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
static void extractLongName(const struct DirEntryImage *, char16_t *);
static uint8_t getChecksum(const char *, uint8_t);
static enum result readLongName(struct FatNode *, char *);
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result allocateCluster(struct FatHandle *, uint32_t *);
static enum result allocateNode(struct FatNode *, const struct FatNode *,
    uint8_t);
static enum result clearCluster(struct FatHandle *, uint32_t);
static enum result createNode(struct FatNode *, const struct FatNode *,
    const struct FsMetadata *);
static inline void fillDirEntry(struct DirEntryImage *, const struct FatNode *);
static bool fillShortName(char *, const char *);
static enum result freeChain(struct FatHandle *, uint32_t);
static enum result markFree(struct FatNode *);
static char processCharacter(char);
static enum result setupDirCluster(const struct FatNode *);
static enum result updateTable(struct FatHandle *, uint32_t);
static enum result writeSector(struct FatHandle *, uint32_t, const uint8_t *,
    uint32_t);
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(FAT_LFN)
static void fillLongName(struct DirEntryImage *, char16_t *);
#endif
/*----------------------------------------------------------------------------*/
/* Filesystem handle functions */
static enum result fatHandleInit(void *, const void *);
static void fatHandleDeinit(void *);
static void *fatAllocate(void *);
static void *fatFollow(void *, const char *, const void *);

/* Node functions */
static enum result fatNodeInit(void *, const void *);
static void fatNodeDeinit(void *);
static void fatFree(void *);
static enum result fatGet(void *, struct FsMetadata *);
static enum result fatMake(void *, const struct FsMetadata *, void *);
static void *fatOpen(void *, access_t);
static enum result fatRemove(void *);
static enum result fatSet(void *, const struct FsMetadata *);
static enum result fatTruncate(void *);

/* Directory functions */
static enum result fatDirInit(void *, const void *);
static void fatDirDeinit(void *);
static enum result fatDirClose(void *);
static enum result fatDirFetch(void *, void *);
static enum result fatDirRewind(void *);

/* File functions */
static enum result fatFileInit(void *, const void *);
static void fatFileDeinit(void *);
static enum result fatFileClose(void *);
static bool fatFileEnd(void *);
static uint32_t fatFileRead(void *, uint8_t *, uint32_t);
static enum result fatFileSeek(void *, uint64_t, enum fsSeekOrigin);
static enum result fatFileSync(void *);
static uint64_t fatFileTell(void *);
static uint32_t fatFileWrite(void *, const uint8_t *, uint32_t);
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
static inline bool hasLongName(struct FatNode *node)
{
  return node->cluster != node->nameCluster || node->index != node->nameIndex;
}
/*----------------------------------------------------------------------------*/
/* Calculate first sector number of the cluster */
static inline uint32_t getSector(struct FatHandle *handle, uint32_t cluster)
{
  return handle->dataSector + (((cluster) - 2) << handle->clusterSize);
}
/*----------------------------------------------------------------------------*/
/* File or directory entries per directory cluster */
static inline uint16_t nodeCount(struct FatHandle *handle)
{
  return 1 << E_POW << handle->clusterSize;
}
/*----------------------------------------------------------------------------*/
/* Calculate current sector in data cluster for read or write operations */
static inline uint8_t sectorInCluster(struct FatHandle *handle, uint32_t offset)
{
  return (offset >> SECTOR_POW) & ((1 << handle->clusterSize) - 1);
}
/*----------------------------------------------------------------------------*/
#endif /* FAT32_DEFS_H_ */
