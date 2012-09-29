#ifndef FAT_H_
#define FAT_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
// #ifdef DEBUG
// #pragma pack(1)
// #endif
/*----------------------------------------------------------------------------*/
//#define FS_WRITE_BUFFERED
#define FS_WRITE_ENABLED
#define FS_RTC_ENABLED
/*----------------------------------------------------------------------------*/
/* Cluster size may be 1, 2, 4, 8, 16, 32, 64, 128 sectors                    */
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512             */
/*----------------------------------------------------------------------------*/
#define SECTOR_SIZE     9 /* Power of sector size */
#define FS_BUFFER       (1 << SECTOR_SIZE)
#define FS_NAME_MAX     13 /* Name + dot + extension + null character */
/*----------------------------------------------------------------------------*/
enum fsMode
{
    FS_NONE = 0,
    FS_READ,
    FS_WRITE,
    FS_APPEND
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
enum fsResult
{
    FS_OK = 0,
    FS_EOF,
    FS_ERROR,
    FS_WRITE_ERROR,
    FS_READ_ERROR,
    FS_NOT_FOUND,
    FS_DEVICE_ERROR
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
struct FsDevice
{
  uint8_t *buffer;
  uint8_t type;
  uint32_t offset;
  uint32_t size;
};
/*----------------------------------------------------------------------------*/
struct FsFile
{
  /* Common fields */
  struct FsHandle *descriptor;
  enum fsMode mode; /* Access mode: read, write or append */
  uint32_t size; /* File size */
  uint32_t position; /* Position in file */

  /* Filesystem-specific fields */
  uint32_t cluster; /* First cluster of file data */
  uint8_t currentSector; /* Sector in current cluster */
  uint32_t currentCluster;
#ifdef FS_WRITE_ENABLED
  uint16_t parentIndex; /* Entry position in parent cluster */
  uint32_t parentCluster; /* Directory cluster where entry located */
#endif
};
/*----------------------------------------------------------------------------*/
struct FsDir
{
  /* Common fields */
  struct FsHandle *descriptor;
//   uint16_t position; /* Position in directory */                   /* TODO */

  /* Filesystem-specific fields */
  uint32_t cluster; /* First cluster of directory data */
  uint16_t currentIndex; /* Entry in current cluster */
  uint32_t currentCluster;
};
/*----------------------------------------------------------------------------*/
struct FsHandle
{
  /* Common low-level fields and functions to handle hardware layer */
  uint8_t *buffer;
  struct FsDevice *device;
  enum fsResult (*read)(struct FsDevice *, uint8_t *, uint32_t, uint8_t);
  enum fsResult (*write)(struct FsDevice *, const uint8_t *, uint32_t,
      uint8_t);

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
enum fsEntryType
{
  FS_TYPE_NONE = 0, /* Unknown type */
  FS_TYPE_FIFO, /* Named pipe */
  FS_TYPE_CHAR, /* Character device */
  FS_TYPE_DIR, /* Directory */
  FS_TYPE_REG, /* Regular file */
  FS_TYPE_LNK, /* Symbolic link */
  FS_TYPE_SOCK /* Socket */
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
struct FsStat
{
  uint64_t atime;
  uint32_t size;
#ifdef DEBUG
  uint16_t access;
  uint32_t cluster;
  uint32_t pcluster;
  uint16_t pindex;
#endif
  enum fsEntryType type;
};
/*----------------------------------------------------------------------------*/
enum fsResult fsStat(struct FsHandle *, const char *, struct FsStat *);
enum fsResult fsOpen(struct FsHandle *, struct FsFile *, const char *,
    enum fsMode);
void fsClose(struct FsFile *);
enum fsResult fsRead(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
enum fsResult fsSeek(struct FsFile *, uint32_t);
enum fsResult fsOpenDir(struct FsHandle *, struct FsDir *, const char *);
void fsCloseDir(struct FsDir *);
enum fsResult fsReadDir(struct FsDir *, char *);
// enum fsResult fsRewindDir(struct FsDir *);                         /* TODO */
// enum fsResult fsSeekDir(struct FsDir *, uint16_t);                 /* TODO */
// uint16_t fsTellDir(struct FsDir *);                                /* TODO */
/*----------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
enum fsResult fsWrite(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
enum fsResult fsRemove(struct FsHandle *, const char *);
enum fsResult fsMakeDir(struct FsHandle *, const char *);
enum fsResult fsMove(struct FsHandle *, const char *, const char *);
#endif
/*----------------------------------------------------------------------------*/
void fsSetIO(struct FsHandle *,
    enum fsResult (*)(struct FsDevice *, uint8_t *, uint32_t, uint8_t),
    enum fsResult (*)(struct FsDevice *, const uint8_t *, uint32_t, uint8_t));
enum fsResult fsLoad(struct FsHandle *, struct FsDevice *);
void fsUnload(struct FsHandle *);
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(struct FsHandle *);
#endif
/*----------------------------------------------------------------------------*/
#endif
