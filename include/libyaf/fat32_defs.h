/*
 * libyaf/fat32_defs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBYAF_FAT32_DEFS_H_
#define LIBYAF_FAT32_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <bits.h>
#include <fs.h>
#include <containers/list.h>
#include <containers/queue.h>
#include <libyaf/unicode.h>
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_THREADS
#include <os/mutex.h>
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
#include <rtc.h>
#endif
/*----------------------------------------------------------------------------*/
/*
 * Sector size may be 512, 1024, 2048 or 4096 bytes, default is 512.
 * Size is configured as an exponent of the value.
 */
#ifdef CONFIG_FAT_SECTOR
#define SECTOR_EXP              CONFIG_FAT_SECTOR
#else
#define SECTOR_EXP              9
#endif
/* Sector size in bytes */
#define SECTOR_SIZE             (1 << SECTOR_EXP)
/*----------------------------------------------------------------------------*/
/* Default pool sizes */
#define DEFAULT_NODE_COUNT      4
#define DEFAULT_DIR_COUNT       2
#define DEFAULT_FILE_COUNT      2
#define DEFAULT_THREAD_COUNT    1
/*----------------------------------------------------------------------------*/
#define FLAG_RO                 BIT(0) /* Read only */
#define FLAG_HIDDEN             BIT(1)
#define FLAG_SYSTEM             BIT(2) /* System entry */
#define FLAG_VOLUME             BIT(3) /* Volume name */
#define FLAG_DIR                BIT(4) /* Directory */
#define FLAG_ARCHIVED           BIT(5)
/*----------------------------------------------------------------------------*/
#define LFN_ENTRY_LENGTH        13 /* Long file name entry length */
#define LFN_LAST                BIT(6) /* Last LFN entry */
#define LFN_DELETED             BIT(7) /* Deleted LFN entry */
#define MASK_LFN                BIT_FIELD(0x0F, 0) /* Long file name chunk */
/*----------------------------------------------------------------------------*/
#define CLUSTER_EOC_VAL         0x0FFFFFF8UL
#define E_FLAG_EMPTY            (char)0xE5 /* Directory entry free flag */
#define FILE_SIZE_MAX           0xFFFFFFFFUL
#define RESERVED_CLUSTER        0 /* Reserved cluster number */
#define RESERVED_SECTOR         0xFFFFFFFFUL /* Initial sector number */
/*----------------------------------------------------------------------------*/
/* Table entries per allocation table sector power */
#define CELL_COUNT              (SECTOR_EXP - 2)
/* Table entry offset in allocation table sector */
#define CELL_OFFSET(arg)        (((arg) & ((1 << CELL_COUNT) - 1)) << 2)
/* File or directory entry size power */
#define ENTRY_EXP               (SECTOR_EXP - 5)
/* Directory entry offset in sector */
#define ENTRY_OFFSET(index)     (((index) << 5) & (SECTOR_SIZE - 1))
/* Directory entry position in cluster */
#define ENTRY_SECTOR(index)     ((index) >> ENTRY_EXP)
/*----------------------------------------------------------------------------*/
struct CommandContext
{
  uint32_t sector;
  uint8_t buffer[SECTOR_SIZE];
};
/*----------------------------------------------------------------------------*/
struct Pool
{
  void *data;
  struct Queue queue;
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  struct FsHandle parent;

  struct FsHandle *head;
  struct Interface *interface;

  struct Pool metadataPool;

#ifdef CONFIG_FAT_POOLS
  struct Pool nodePool;
  struct Pool dirPool;
  struct Pool filePool;
#endif

#ifdef CONFIG_FAT_THREADS
  struct Pool contextPool;
  struct Mutex consistencyMutex;
  struct Mutex memoryMutex;
#else
  struct CommandContext *context;
#endif

#ifdef CONFIG_FAT_WRITE
  struct List openedFiles;
#endif

  /* Number of the first sector containing cluster data */
  uint32_t dataSector;
  /* First cluster of the root directory */
  uint32_t rootCluster;
  /* Starting point of the file allocation table */
  uint32_t tableSector;
#ifdef CONFIG_FAT_WRITE
  /* Number of clusters in the partition */
  uint32_t clusterCount;
  /* Last allocated cluster */
  uint32_t lastAllocated;
  /* Size of each allocation table in sectors */
  uint32_t tableSize;
  /* Information sector number */
  uint16_t infoSector;
  /* Number of file allocation tables */
  uint8_t tableNumber;
#endif
  /* Sectors per cluster in power of two */
  uint8_t clusterSize;
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

  /* Directory cluster where the entry is located */
  uint32_t cluster;
  /* First cluster of the entry */
  uint32_t payload;
#ifdef CONFIG_FAT_UNICODE
  /* Directory cluster of the first name entry */
  uint32_t nameCluster;
  /* First name entry position in the parent cluster */
  uint16_t nameIndex;
#endif
  /* Entry position in the parent cluster */
  uint16_t index;
  /* Access rights */
  access_t access;
  /* Node type */
  enum fsNodeType type;
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

  /* First cluster of the directory entries */
  uint32_t payload;
  /* Current cluster of the directory entries */
  uint32_t currentCluster;
  /* Position in the current cluster */
  uint16_t currentIndex;
};
/*----------------------------------------------------------------------------*/
struct FatFileConfig
{
  struct FsNode *node;
  access_t access;
};
/*----------------------------------------------------------------------------*/
struct FatFile
{
  struct FsEntry parent;

  struct FsHandle *handle;

  /* File size */
  uint32_t size;
  /* First cluster of the file data */
  uint32_t payload;
  /* Position in the file */
  uint32_t position;
  /* Current cluster inside data chain */
  uint32_t currentCluster;
#ifdef CONFIG_FAT_WRITE
  /* Directory cluster where entry located */
  uint32_t parentCluster;
  /* Entry position in parent cluster */
  uint16_t parentIndex;
#endif
  /* Access rights to the file */
  access_t access;
};
/*----------------------------------------------------------------------------*/
/* Directory entry or long file name entry*/
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

    /* Long file name entry fields */
    struct
    {
      uint8_t ordinal;
      char16_t longName0[5];
    } __attribute__((packed));
  };

  uint8_t flags;
  uint8_t unused0;
  union
  {
    /* Directory entry field */
    uint8_t unused1;
    /* Long file name entry field */
    uint8_t checksum;
  };

  union
  {
    /* Directory entry fields */
    struct
    {
      uint16_t unused2[3];
      uint16_t clusterHigh; /* Starting cluster high word */
      uint16_t time;
      uint16_t date;
      uint16_t clusterLow; /* Starting cluster low word */
      uint32_t size;
    } __attribute__((packed));

    /* Long file name entry fields */
    struct
    {
      char16_t longName1[6];
      char16_t unused3;
      char16_t longName2[2];
    } __attribute__((packed));
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
enum cleanup
{
  FREE_ALL = 0,
  FREE_FILE_POOL,
  FREE_DIR_POOL,
  FREE_NODE_POOL,
  FREE_FILE_LIST,
  FREE_METADATA_POOL,
  FREE_CONTEXT_POOL,
  FREE_LOCKS
};
/*----------------------------------------------------------------------------*/
static enum result allocatePool(struct Pool *, unsigned int, unsigned int,
    const void *);
static void freePool(struct Pool *);
/*----------------------------------------------------------------------------*/
static enum result allocateBuffers(struct FatHandle *,
    const struct Fat32Config * const);
static struct CommandContext *allocateContext(struct FatHandle *);
static void *allocateNode(struct FatHandle *);
static void extractShortName(const struct DirEntryImage *, char *);
static enum result fetchEntry(struct CommandContext *, struct FatNode *);
static enum result fetchNode(struct CommandContext *, struct FatNode *,
    struct FsMetadata *);
static const char *followPath(struct CommandContext *, struct FatNode *,
    const char *, const struct FatNode *);
static void freeBuffers(struct FatHandle *, enum cleanup);
static void freeContext(struct FatHandle *, const struct CommandContext *);
static const char *getChunk(const char *, char *);
static enum result getNextCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum result mount(struct FatHandle *);
static enum result readBuffer(struct FatHandle *, uint32_t, uint8_t *,
    uint32_t);
static enum result readSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
enum result rawTimestampToTime(time_t *, uint16_t, uint16_t);
uint16_t timeToRawDate(const struct RtcTime *);
uint16_t timeToRawTime(const struct RtcTime *);
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
static void extractLongName(const struct DirEntryImage *, char16_t *);
static uint8_t getChecksum(const char *, uint8_t);
static enum result readLongName(struct CommandContext *, const struct FatNode *,
    char *);
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result allocateCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum result clearCluster(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum result createNode(struct CommandContext *, struct FatNode *,
    const struct FatNode *, const struct FsMetadata *);
static void fillDirEntry(struct DirEntryImage *, const struct FatNode *);
static enum result fillShortName(char *, const char *);
static enum result findGap(struct CommandContext *, struct FatNode *,
    const struct FatNode *, uint8_t);
static enum result freeChain(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum result markFree(struct CommandContext *, const struct FatNode *);
static char processCharacter(char);
static enum result setupDirCluster(struct CommandContext *,
    const struct FatNode *);
static enum result syncFile(struct CommandContext *, struct FatFile *);
static enum result updateTable(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum result writeBuffer(struct FatHandle *, uint32_t, const uint8_t *,
    uint32_t);
static enum result writeSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) && defined(CONFIG_FAT_WRITE)
static enum result allocateStaticNode(struct FatHandle *, struct FatNode *);
static void fillLongName(struct DirEntryImage *, char16_t *);
static void fillLongNameEntry(struct DirEntryImage *, uint8_t, uint8_t,
    uint8_t);
static void freeStaticNode(struct FatNode *);
#endif
/*----------------------------------------------------------------------------*/
/* Filesystem handle functions */
static enum result fatHandleInit(void *, const void *);
static void fatHandleDeinit(void *);
static void *fatFollow(void *, const char *, const void *);
static enum result fatSync(void *);

/* Node functions */
static enum result fatNodeInit(void *, const void *);
static void fatNodeDeinit(void *);
static void *fatClone(const void *);
static void fatFree(void *);
static enum result fatGet(const void *, enum fsNodeData, void *);
static enum result fatLink(void *, const struct FsMetadata *, const void *,
    void *);
static enum result fatMake(void *, const struct FsMetadata *, void *);
static void *fatOpen(void *, access_t);
static enum result fatSet(void *, enum fsNodeData, const void *);
static enum result fatTruncate(void *);
static enum result fatUnlink(void *);

/* Directory functions */
static enum result fatDirInit(void *, const void *);
static void fatDirDeinit(void *);
static enum result fatDirClose(void *);
static bool fatDirEnd(const void *);
static enum result fatDirFetch(void *, void *);
static enum result fatDirSeek(void *, uint64_t, enum fsSeekOrigin);
static uint64_t fatDirTell(const void *);

/* File functions */
static enum result fatFileInit(void *, const void *);
static void fatFileDeinit(void *);
static enum result fatFileClose(void *);
static bool fatFileEnd(const void *);
static uint32_t fatFileRead(void *, void *, uint32_t);
static enum result fatFileSeek(void *, uint64_t, enum fsSeekOrigin);
static uint64_t fatFileTell(const void *);
static uint32_t fatFileWrite(void *, const void *, uint32_t);

/* Stubs */
static enum result fatMount(void *, void *);
static void fatUnmount(void *);
static uint32_t fatDirRead(void *, void *, uint32_t);
static uint32_t fatDirWrite(void *, const void *, uint32_t);
static enum result fatFileFetch(void *, void *);
/*----------------------------------------------------------------------------*/
#endif /* LIBYAF_FAT32_DEFS_H_ */
