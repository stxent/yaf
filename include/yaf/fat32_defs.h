/*
 * yaf/fat32_defs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_FAT32_DEFS_H_
#define YAF_FAT32_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <yaf/pointer_array.h>
#include <yaf/pointer_queue.h>
#include <xcore/bits.h>
#include <xcore/fs/fs.h>
#include <xcore/realtime.h>
#include <xcore/unicode.h>

#ifdef CONFIG_THREADS
#include <osw/mutex.h>
#endif

#include <stdbool.h>
#include <stdint.h>
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
/* Default pool size */
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
/* Index of the first cluster of the data region */
#define CLUSTER_OFFSET          2
/* Reserved cluster value */
#define CLUSTER_RES_VAL         0x0FFFFFFFUL
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
#define CELL_COUNT_EXP          (SECTOR_EXP - 2)
/* Table entry offset in allocation table sector */
#define CELL_OFFSET(arg)        (((arg) & ((1 << CELL_COUNT_EXP) - 1)) << 2)
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
enum
{
  FAT_FLAG_DIR    = 0x01,
  FAT_FLAG_FILE   = 0x02,
  FAT_FLAG_RO     = 0x04,
  FAT_FLAG_DIRTY  = 0x08
};
/*----------------------------------------------------------------------------*/
extern const struct FsHandleClass * const FatHandle;
extern const struct FsNodeClass * const FatNode;
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
  PointerQueue queue;
};
/*----------------------------------------------------------------------------*/
struct FatHandle
{
  struct FsHandle base;

  struct FsHandle *head;
  struct Interface *interface;

  struct
  {
    struct Pool contexts;
    struct Pool nodes;
  } pools;

#ifdef CONFIG_THREADS
  struct Mutex consistencyMutex;
  struct Mutex memoryMutex;
#endif

#ifdef CONFIG_WRITE
  PointerArray openedFiles;
#endif

  /* Number of the first sector containing cluster data */
  uint32_t dataSector;
  /* First cluster of the root directory */
  uint32_t rootCluster;
  /* Starting point of the file allocation table */
  uint32_t tableSector;
#ifdef CONFIG_WRITE
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

struct FatNode
{
  struct FsNode base;

  struct FsHandle *handle;

  /* Parent cluster */
  uint32_t parentCluster;
  /* Position in the parent cluster */
  uint16_t parentIndex;

#ifdef CONFIG_UNICODE
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
  /* Number of LFN entries */
  uint8_t lfn;
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
  /* 0x00 Jump Code + NOP */
  char jumpCode[3];
  /* 0x03 OEM identifier */
  char oemName[8];

  /* 0x0B Bytes per sector */
  uint16_t bytesPerSector;
  /* 0x0D Sectors per cluster */
  uint8_t sectorsPerCluster;
  /* 0x0E Reserved sectors in front of the FAT */
  uint16_t reservedSectors;
  /* 0x10 Number of copies of FAT */
  uint8_t tableCount;

  char unused1[4];

  /* 0x15 Media Descriptor 0xF8 */
  uint8_t mediaDescriptor;

  char unused2[6];

  /* 0x1C Number of hidden sectors preceding the partition */
  uint32_t hiddenSectors;
  /* 0x20 Number of sectors in the partition */
  uint32_t sectorsPerPartition;
  /* 0x24 Number of sectors per FAT */
  uint32_t sectorsPerTable;

  char unused3[4];

  /* 0x2C Cluster number of the Root Directory */
  uint32_t rootCluster;
  /* 0x30 Sector number of the Information Sector */
  uint16_t infoSector;
  /* 0x32 Sector number of the Backup Boot Sector */
  uint16_t backupSector;

  char unused4[12];

  /* 0x40 Logical Drive number of the partition */
  uint8_t logicalNumber;

  char unused5;

  /* 0x42 Extended signature 0x29 */
  uint8_t extendedSignature;
  /* 0x43 Serial number of the partition */
  uint32_t serialNumber;
  /* 0x47 Volume name of the partition */
  char volumeName[11];
  /* 0x52 FAT name */
  char fatName[8];
  /* 0x5A Executable code */
  uint8_t executableCode[420];
  /* 0x1FE Boot Record Signature 0x55 0xAA */
  uint16_t bootSignature;
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
/* Info sector */
struct InfoSectorImage
{
  /* 0x00 First signature */
  uint32_t firstSignature;

  char unused0[480];

  /* 0x1E4 Signature of Information Sector */
  uint32_t infoSignature;
  /* 0x1E8 Number of free clusters */
  uint32_t freeClusters;
  /* 0x1EC Number of cluster that was most recently allocated */
  uint32_t lastAllocated;

  char unused1[14];

  /* 0x1FE Boot Record Signature 0x55 0xAA */
  uint16_t bootSignature;
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
#endif /* YAF_FAT32_DEFS_H_ */
