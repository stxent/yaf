#ifndef FAT_H_
#define FAT_H_
/*---------------------------------------------------------------------------*/
#include <stdint.h>
#include "settings.h"
/*---------------------------------------------------------------------------*/
//#define FS_WRITE_BUFFERED
#define FS_WRITE_ENABLED
#define FS_RTC_ENABLED
#pragma pack(1)
/*---------------------------------------------------------------------------*/
/* Cluster size may be 1, 2, 4, 8, 16, 32, 64, 128 sectors                   */
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512            */
/*---------------------------------------------------------------------------*/
#define SECTOR_SIZE 9 /* Power of sector size */
#define FS_BUFFER (1 << SECTOR_SIZE)
/*---------------------------------------------------------------------------*/
enum fsCondition {FS_OPENED = 0, FS_CLOSED};
enum fsMode      {FS_NONE = 0, FS_READ, FS_WRITE, FS_APPEND};
enum fsResult    {FS_OK = 0, FS_ERROR, FS_WRITE_ERROR, FS_READ_ERROR,
                  FS_NOT_FOUND, FS_DEVICE_ERROR, FS_EOF};
/*---------------------------------------------------------------------------*/
struct fsEntry
{
  uint8_t attribute; /* File or directory attributes */
  uint16_t index; /* Entry position in parent cluster */
  uint32_t cluster; /* First cluster of entry */
  uint32_t parent; /* Directory cluster where entry located */
  uint32_t size; /* File size or zero for directories */
  char name[13]; /* Name + dot + extension + null character */
#ifdef DEBUG
  uint16_t time, date;
#endif
};
/*---------------------------------------------------------------------------*/
struct fsFile
{
  struct fsHandle *descriptor;
  enum fsCondition state; /* Opened or closed */
  enum fsMode mode; /* Access mode: read, write or append */
  uint32_t cluster; /* First cluster of file data */
  uint32_t size; /* File size */
  uint32_t position; /* Position in file */
  uint8_t currentSector; /* Sector in current cluster */
  uint32_t currentCluster;
#ifdef FS_WRITE_ENABLED
  uint16_t parentIndex; /* Entry position in parent cluster */
  uint32_t parentCluster; /* Directory cluster where entry located */
#endif
#ifdef DEBUG
  struct fsEntry data;
#endif
};
/*---------------------------------------------------------------------------*/
struct fsDir
{
  struct fsHandle *descriptor;
  enum fsCondition state; /* Opened or closed */
  uint32_t cluster; /* First cluster of folder data */
  uint16_t currentIndex; /* Entry in current cluster */
  uint32_t currentCluster;
#ifdef DEBUG
  struct fsEntry data;
#endif
};
/*---------------------------------------------------------------------------*/
struct fsHandle
{
  struct sDevice *device;
  uint8_t *buffer;
  uint8_t clusterSize; /* Sectors per cluster */
  uint32_t currentSector, rootCluster, dataSector, fatSector;
#ifdef FS_WRITE_ENABLED
  uint8_t fatCount;
  uint16_t infoSector;
  uint32_t sectorsPerFAT;
  uint32_t clusterCount; /* Number of clusters */
  uint32_t lastAllocated; /* Last allocated cluster */
#endif
};
/*---------------------------------------------------------------------------*/
enum fsResult fsOpen(struct fsHandle *, struct fsFile *, const char *, enum fsMode);
enum fsResult fsClose(struct fsFile *);
enum fsResult fsRead(struct fsFile *, uint8_t *, uint16_t, uint16_t *);
enum fsResult fsSeek(struct fsFile *, uint32_t);
enum fsResult fsOpenDir(struct fsHandle *, struct fsDir *, const char *);
enum fsResult fsReadDir(struct fsDir *, char *);
enum fsResult fsSeekDir(struct fsDir *, uint16_t);
/*---------------------------------------------------------------------------*/
#ifdef FS_WRITE_ENABLED
enum fsResult fsWrite(struct fsFile *, uint8_t *, uint16_t, uint16_t *);
enum fsResult fsTruncate(struct fsFile *);
enum fsResult fsRemove(struct fsHandle *, const char *);
enum fsResult fsMakeDir(struct fsHandle *, const char *);
enum fsResult fsMove(struct fsHandle *, const char *, const char *);
#endif
/*---------------------------------------------------------------------------*/
enum fsResult fsLoad(struct fsHandle *, struct sDevice *, uint8_t *);
enum fsResult fsUnload(struct fsHandle *);
/*---------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(struct fsHandle *);
#endif
/*---------------------------------------------------------------------------*/
#endif
