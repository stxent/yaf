#ifndef _FAT_H_
#define _FAT_H_
//---------------------------------------------------------------------------
#include <inttypes.h>
#include "settings.h"
//---------------------------------------------------------------------------
#define FS_MAX_STREAMS 0x10
#define SECTOR_SIZE_POW 0x09 //512 bytes
#define BUFFER_LEVEL 0x08
//---------------------------------------------------------------------------
enum {FS_NONE = 0, FS_READ, FS_WRITE, FS_APPEND};
enum {FS_OPENED = 0, FS_CLOSED};
enum {FS_OK = 0, FS_ERROR, FS_WRITE_ERROR, FS_READ_ERROR, FS_NOT_FOUND, FS_DEVICE_ERROR};
//---------------------------------------------------------------------------
struct item
{
  uint8_t data[1 << SECTOR_SIZE_POW];
  uint32_t sector;
};
//---------------------------------------------------------------------------
struct buffer
{
//  struct item *list[BUFFER_LEVEL];
//  struct item content[BUFFER_LEVEL];
  struct item records[BUFFER_LEVEL];
  uint8_t top, size;
};
//---------------------------------------------------------------------------
struct fsEntry
{
  uint8_t attribute;
  uint16_t index;
  uint32_t size, cluster, parent;
  char name[13];
};
//---------------------------------------------------------------------------
struct fsFile
{
  uint8_t currentSector, mode;
  uint16_t parentIndex;
  uint32_t cluster, parentCluster, currentCluster, size, position;
  struct fsHandle *descriptor;
};
//---------------------------------------------------------------------------
struct fsHandle
{
  uint8_t state, streams, sectorsPerCluster, fatCount;
//  uint8_t buffer[1 << SECTOR_SIZE_POW];
  uint8_t *buffer;
  struct buffer sectorList;
//  uint8_t sectorBuffer[(1 << SECTOR_SIZE_POW) * BUFFER_LEVEL]
//  struct item sectorList[BUFFER_LEVEL];
  uint32_t currentSector, rootCluster, dataSector, fatSector, sectorsPerFAT;
};
//---------------------------------------------------------------------------
uint8_t fsOpenFile(struct fsHandle *, struct fsFile *, const char *, uint8_t);
uint8_t fsCloseFile(struct fsFile *);
uint16_t fsRead(struct fsFile *, uint8_t *, uint16_t);
uint16_t fsWrite(struct fsFile *, uint8_t *, uint16_t);
uint8_t fsSeek(struct fsFile *, uint32_t);
//---------------------------------------------------------------------------
uint8_t fsLoad(struct fsHandle *);
uint8_t fsUnload(struct fsHandle *);
//---------------------------------------------------------------------------
#endif
