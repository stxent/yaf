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
#include "fs.h"
/*----------------------------------------------------------------------------*/
#ifdef FAT_TIME
#include "rtc.h"
#endif
#ifdef FAT_LFN
#include "unicode.h"
#endif
/*----------------------------------------------------------------------------*/
#define SECTOR_SIZE             (1 << SECTOR_POW) /* Sector size in bytes */
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512 */
#define SECTOR_POW              9 /* Sector size in power of 2 */
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Buffer size in code points for internal long file name processing */
#define FILE_NAME_BUFFER        128
/* Length in bytes for short names and UTF-8 long file names */
#define FILE_NAME_MAX           256
/* Long file name entry length: 13 UTF-16LE characters */
#define LFN_ENTRY_LENGTH        13
#else
/* Length in bytes for short names */
#define FILE_NAME_MAX           13
#endif /* FAT_LFN */
/*----------------------------------------------------------------------------*/
#define FLAG_RO                 (uint8_t)0x01 /* Read only */
#define FLAG_HIDDEN             (uint8_t)0x02
#define FLAG_SYSTEM             (uint8_t)0x04 /* System entry */
#define FLAG_VOLUME             (uint8_t)0x08 /* Volume name */
#define FLAG_DIR                (uint8_t)0x10 /* Subdirectory */
#define FLAG_ARCHIVE            (uint8_t)0x20
#define MASK_LFN                (uint8_t)0x0F /* Long file name chunk */
/*----------------------------------------------------------------------------*/
#define LFN_DELETED             (uint8_t)0x80 /* Deleted LFN entry */
#define LFN_LAST                (uint8_t)0x40 /* Last LFN entry */
/*----------------------------------------------------------------------------*/
#define E_FLAG_EMPTY            (char)0xE5 /* Directory entry is free */
/*----------------------------------------------------------------------------*/
#define CLUSTER_EOC_VAL         (uint32_t)0x0FFFFFF8
#define FILE_SIZE_MAX           (uint32_t)0xFFFFFFFF
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
/*------------------Filesystem classes----------------------------------------*/
struct FatConfig
{
  struct Interface *interface;
  uint8_t *buffer;
};
/*----------------------------------------------------------------------------*/
struct FatDir
{
  struct FsDir parent;

  uint32_t currentCluster;
  uint32_t cluster; /* First cluster of directory data */
  uint16_t currentIndex; /* Entry in current cluster */
};
/*----------------------------------------------------------------------------*/
struct FatFile
{
  struct FsFile parent;

  uint32_t size; /* File size */
  uint32_t position; /* Position in file */
  uint32_t cluster; /* First cluster of file data */
  uint32_t currentCluster;
#ifdef FAT_WRITE
  uint32_t parentCluster; /* Directory cluster where entry located */
  uint16_t parentIndex; /* Entry position in parent cluster */
#endif
  enum fsMode mode; /* Access mode: read, write or append */
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  struct FsHandle parent;

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
/* Directory entry descriptor */
struct FatObject
{
  uint32_t parent; /* Directory cluster where the entry is located */
  uint32_t cluster; /* First cluster of the entry */
  uint32_t size; /* File size or zero for directories */
#ifdef FAT_LFN
  uint32_t nameParent; /* Directory cluster of the first name entry */
  uint16_t nameIndex; /* First name entry position in the parent cluster */
#endif
  uint16_t index; /* Entry position in the parent cluster */
  uint8_t attribute; /* File or directory attributes */
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
/*----------------------------------------------------------------------------*/
/*------------------Specific FAT32 memory structures--------------------------*/
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
/*------------------Inline functions------------------------------------------*/
static inline bool clusterFree(uint32_t);
static inline bool clusterEOC(uint32_t);
static inline bool clusterUsed(uint32_t);
static inline uint32_t getSector(struct FatHandle *, uint32_t);
static inline uint16_t entryCount(struct FatHandle *);
static inline uint8_t sectorInCluster(struct FatHandle *, uint32_t);
/*----------------------------------------------------------------------------*/
/*------------------Specific FAT32 functions----------------------------------*/
static void extractShortName(const struct DirEntryImage *, char *);
static const char *getChunk(const char *, char *);
static enum result getNextCluster(struct FatHandle *, uint32_t *);
static enum result fetchEntry(struct FatHandle *, struct FatObject *, char *);
static const char *followPath(struct FatHandle *, struct FatObject *,
    const char *);
static enum result readSector(struct FatHandle *, uint32_t, uint8_t *, uint8_t);
#ifdef FAT_LFN
/* Function for long file name support */
static void extractLongName(const struct DirEntryImage *, char16_t *);
static uint8_t getChecksum(const char *, uint8_t);
static enum result readLongName(struct FatHandle *, struct LfnObject *, char *);
#endif
#ifdef FAT_WRITE
/* Functions with write access */
static enum result allocateCluster(struct FatHandle *, uint32_t *);
static enum result allocateEntry(struct FatHandle *, struct FatObject *,
    uint8_t);
static enum result createEntry(struct FatHandle *, struct FatObject *,
    const char *);
static inline void fillDirEntry(struct DirEntryImage *, struct FatObject *);
static bool fillShortName(char *, const char *);
static enum result freeChain(struct FatHandle *, uint32_t);
static enum result markFree(struct FatHandle *, struct FatObject *);
static char processCharacter(char);
static enum result writeSector(struct FatHandle *, uint32_t, const uint8_t *,
    uint8_t);
static enum result truncate(struct FatFile *);
static enum result updateTable(struct FatHandle *, uint32_t);
#endif
#if defined(FAT_WRITE) && defined(FAT_LFN)
static void fillLongName(struct DirEntryImage *, char16_t *);
#endif
/*----------------------------------------------------------------------------*/
/*------------------Filesystem functions--------------------------------------*/
/* Constructor and destructor */
static enum result fatInit(void *, const void *);
static void fatDeinit(void *);
/*------------------Implemented filesystem methods----------------------------*/
/* Common functions */
static void *fatOpen(void *, const char *, enum fsMode);
static void *fatOpenDir(void *, const char *);
static enum result fatStat(void *, struct FsStat *, const char *);
#ifdef FAT_WRITE
static enum result fatMove(void *, const char *, const char *);
static enum result fatRemove(void *, const char *);
#endif
/*----------------------------------------------------------------------------*/
/* File functions */
static void fatClose(void *);
static bool fatEof(void *);
static uint32_t fatRead(void *, uint8_t *, uint32_t);
static enum result fatSeek(void *, asize_t, enum fsSeekOrigin);
static asize_t fatTell(void *);
#ifdef FAT_WRITE
static enum result fatFlush(void *);
static uint32_t fatWrite(void *, const uint8_t *, uint32_t);
#endif
/*----------------------------------------------------------------------------*/
/* Directory functions */
static void fatCloseDir(void *);
static enum result fatReadDir(void *, char *);
/* Functions with write access */
static enum result fatMakeDir(void *, const char *);
static enum result fatRemoveDir(void *, const char *);
/*----------------------------------------------------------------------------*/
#endif /* FAT32_DEFS_H_ */
