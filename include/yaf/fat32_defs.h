/*
 * yaf/fat32_defs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_FAT32_DEFS_H_
#define YAF_FAT32_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <xcore/bits.h>
#include <xcore/containers/list.h>
#include <xcore/containers/queue.h>
#include <xcore/fs.h>
#include <xcore/unicode.h>
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FLAG_THREADS
#include <osw/mutex.h>
#endif

#ifdef CONFIG_FLAG_TIME
#include <xcore/realtime.h>
#endif
/*----------------------------------------------------------------------------*/
/* Sector size may be 512, 1024, 2048 or 4096 bytes, default is 512. */
#if CONFIG_SECTOR_SIZE == 512
#define SECTOR_EXP 9
#elif CONFIG_SECTOR_SIZE == 1024
#define SECTOR_EXP 10
#elif CONFIG_SECTOR_SIZE == 2048
#define SECTOR_EXP 11
#elif CONFIG_SECTOR_SIZE == 4096
#define SECTOR_EXP 12
#else
#error "Incorrect sector size"
#endif

/* Sector size in bytes */
#define SECTOR_SIZE             (1 << SECTOR_EXP)
/*----------------------------------------------------------------------------*/
/* Default pool sizes */
#define DEFAULT_NODE_COUNT      4
#define DEFAULT_THREAD_COUNT    1
/*----------------------------------------------------------------------------*/
#define FLAG_RO                 BIT(0) /* Read only */
#define FLAG_HIDDEN             BIT(1)
#define FLAG_SYSTEM             BIT(2) /* System entry */
#define FLAG_VOLUME             BIT(3) /* Volume name */
#define FLAG_DIR                BIT(4) /* Directory */
#define FLAG_ARCHIVED           BIT(5)
/*----------------------------------------------------------------------------*/
/* Long file name entry length */
#define LFN_ENTRY_LENGTH        13
/* Last LFN entry */
#define LFN_LAST                BIT(6)
/* Deleted LFN entry */
#define LFN_DELETED             BIT(7)
/* Long file name chunk */
#define MASK_LFN                BIT_FIELD(0x0F, 0)
/*----------------------------------------------------------------------------*/
/* End of cluster chain value */
#define CLUSTER_EOC_VAL         0x0FFFFFF8UL
/* Directory entry free flag */
#define E_FLAG_EMPTY            (char)0xE5
/* The maximum possible size for a file is 4 GiB minus 1 byte */
#define FILE_SIZE_MAX           0xFFFFFFFFUL
/* Maximum number of duplicate name entries when using the 8.3 convention */
#define MAX_SIMILAR_NAMES       100
/* Reserved cluster number */
#define RESERVED_CLUSTER        0
/* Initial sector number */
#define RESERVED_SECTOR         0xFFFFFFFFUL
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
/* Length of the entry name */
#define BASENAME_LENGTH         8
/* Length of the extension */
#define EXTENSION_LENGTH        3
/* Length of both entry name and extension */
#define NAME_LENGTH             (BASENAME_LENGTH + EXTENSION_LENGTH)
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
enum
{
  FAT_FLAG_DIR    = 0x01,
  FAT_FLAG_FILE   = 0x02,
  FAT_FLAG_RO     = 0x04,
  FAT_FLAG_DIRTY  = 0x08
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  struct FsHandle base;

  struct FsHandle *head;
  struct Interface *interface;

#ifdef CONFIG_FLAG_TIME
  struct RtClock *clock;
#endif

#ifdef CONFIG_FLAG_POOLS
  struct Pool nodePool;
#endif

#ifdef CONFIG_FLAG_THREADS
  struct Pool contextPool;
  struct Mutex consistencyMutex;
  struct Mutex memoryMutex;
#else
  struct CommandContext *context;
#endif

#ifdef CONFIG_FLAG_WRITE
  struct List openedFiles;
#endif

  /* Number of the first sector containing cluster data */
  uint32_t dataSector;
  /* First cluster of the root directory */
  uint32_t rootCluster;
  /* Starting point of the file allocation table */
  uint32_t tableSector;
#ifdef CONFIG_FLAG_WRITE
  /* Number of clusters in the partition */
  uint32_t clusterCount;
  /* Last allocated cluster */
  uint32_t lastAllocated;
  /* Size of each allocation table in sectors */
  uint32_t tableSize;
  /* Information sector number */
  uint16_t infoSector;
  /* Number of file allocation tables */
  uint8_t tableCount;
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
  struct FsNode base;

  struct FsHandle *handle;

  /* Parent cluster */
  uint32_t parentCluster;
  /* Position in the parent cluster */
  uint16_t parentIndex;
#ifdef CONFIG_FLAG_UNICODE
  /* First name entry position in the parent cluster */
  uint16_t nameIndex;
  /* Directory cluster of the first name entry */
  uint32_t nameCluster;
#endif

  /* First cluster of the payload */
  uint32_t payloadCluster;
  /* File size */
  uint32_t payloadSize;

  /* Cached value of the current cluster for fast access */
  uint32_t currentCluster;
  /* Cached value of the position in the payload */
  uint32_t payloadPosition;
  /* Length of the node name converted to UTF-8 */
  uint16_t nameLength;

  /* Status flags */
  uint8_t flags;
};
/*----------------------------------------------------------------------------*/
/* Directory entry or long file name entry */
struct DirEntryImage
{
  union
  {
    char filename[NAME_LENGTH];

    struct
    {
      char name[BASENAME_LENGTH];
      char extension[EXTENSION_LENGTH];
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
  char unused0[3];
  char oemIdentifier[8];
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectors;  /* Reserved sectors in front of the FAT */
  uint8_t tableCount;        /* Number of FAT copies */
  char unused1[4];
  uint8_t mediaDescriptor;   /* 0xF8 */
  char unused2[6];
  uint32_t tableOffset;      /* The number of sectors before the first FAT */
  uint32_t partitionSize;    /* Sectors per partition */
  uint32_t tableSize;        /* Sectors per FAT record */
  char unused3[4];
  uint32_t rootCluster;      /* Root directory cluster */
  uint16_t infoSector;       /* Sector number for information sector */
  char unused4[460];
  uint16_t bootSignature;    /* 0xAA55 */
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
#endif /* YAF_FAT32_DEFS_H_ */
