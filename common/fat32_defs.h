/*
 * fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef FAT32_DEFS_H_
#define FAT32_DEFS_H_
/*----------------------------------------------------------------------------*/
#define FLAG_RO                 (uint8_t)0x01 /* Read only */
#define FLAG_HIDDEN             (uint8_t)0x02
#define FLAG_SYSTEM             (uint8_t)0x0C /* System or volume label */
#define FLAG_DIR                (uint8_t)0x10 /* Subdirectory */
#define FLAG_ARCHIVE            (uint8_t)0x20
/*----------------------------------------------------------------------------*/
#define E_FLAG_EMPTY            (char)0xE5 /* Directory entry is free */
/*----------------------------------------------------------------------------*/
#define CLUSTER_EOC_VAL         (uint32_t)0x0FFFFFF8
#define FILE_SIZE_MAX           (uint32_t)0xFFFFFFFF
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
/*------------------Filesystem classes----------------------------------------*/
struct FatFile
{
  struct FsFile parent;

  uint32_t cluster; /* First cluster of file data */
  uint8_t currentSector; /* Sector in current cluster */
  uint32_t currentCluster;
#ifdef FAT_WRITE_ENABLED
  uint16_t parentIndex; /* Entry position in parent cluster */
  uint32_t parentCluster; /* Directory cluster where entry located */
#endif
};
/*----------------------------------------------------------------------------*/
struct FatDir
{
  struct FsDir parent;

  /* Filesystem-specific fields */
  uint32_t cluster; /* First cluster of directory data */
  uint16_t currentIndex; /* Entry in current cluster */
  uint32_t currentCluster;
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  struct FsHandle parent;

  /* Filesystem-specific fields */
  /* Cluster size may be 1, 2, 4, 8, 16, 32, 64, 128 sectors */
  uint8_t clusterSize; /* Sectors per cluster power */
  uint32_t currentSector, rootCluster, dataSector, tableSector;
#ifdef FAT_WRITE_ENABLED
  uint8_t tableCount; /* FAT tables count */
  uint32_t tableSize; /* Size in sectors of each FAT table */
  uint16_t infoSector;
  uint32_t clusterCount; /* Number of clusters */
  uint32_t lastAllocated; /* Last allocated cluster */
#endif
};
/*----------------------------------------------------------------------------*/
/*------------------Directory entry structure---------------------------------*/
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
/*------------------Specific FAT32 memory structures--------------------------*/
/* Directory entry */
struct DirEntryImage
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
static inline uint32_t getSector(struct FatHandle *sys, uint32_t);
static inline uint16_t entryCount(struct FatHandle *sys);
/*----------------------------------------------------------------------------*/
/*------------------Specific FAT32 functions----------------------------------*/
static enum fsResult fetchEntry(struct FsHandle *, struct FatObject *);
static const char *followPath(struct FsHandle *, struct FatObject *,
    const char *);
static enum fsResult getNextCluster(struct FsHandle *, uint32_t *);
static const char *getChunk(const char *, char *);
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE_ENABLED
static enum fsResult truncate(struct FsFile *);
static enum fsResult freeChain(struct FsHandle *, uint32_t);
static enum fsResult allocateCluster(struct FsHandle *, uint32_t *);
static enum fsResult createEntry(struct FsHandle *, struct FatObject *,
    const char *);
static enum fsResult updateTable(struct FsHandle *, uint32_t);
#endif
/*----------------------------------------------------------------------------*/
/*------------------Implemented virtual methods-------------------------------*/
static enum fsResult fatMount(struct FsHandle *, struct BlockDevice *);
static void fatUmount(struct FsHandle *);
static enum fsResult fatStat(struct FsHandle *, const char *, struct FsStat *);
static enum fsResult fatOpen(struct FsHandle *, struct FsFile *, const char *,
    enum fsMode);
static enum fsResult fatOpenDir(struct FsHandle *, struct FsDir *,
    const char *);
static void fatClose(struct FsFile *);
static bool fatEof(struct FsFile *);
static enum fsResult fatSeek(struct FsFile *, uint32_t);
static enum fsResult fatRead(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
static void fatCloseDir(struct FsDir *);
static enum fsResult fatReadDir(struct FsDir *, char *);
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE_ENABLED
static enum fsResult fatRemove(struct FsHandle *, const char *);
static enum fsResult fatMove(struct FsHandle *, const char *, const char *);
static enum fsResult fatMakeDir(struct FsHandle *, const char *);
static enum fsResult fatWrite(struct FsFile *, const uint8_t *, uint16_t,
    uint16_t *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* FAT32_DEFS_H_ */
