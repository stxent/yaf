#include "fat.h"
#include "mem.h"
//---------------------------------------------------------------------------
#include <iostream>
using namespace std;
//---------------------------------------------------------------------------
static uint8_t readSector(struct fsHandle *, uint32_t);
static uint8_t writeSector(struct fsHandle *, uint32_t);
static uint8_t followPath(struct fsHandle *, struct fsEntry *, const char *);
static uint8_t fetchEntry(struct fsHandle *, struct fsEntry *, uint32_t *);
static uint32_t getNextCluster(struct fsHandle *, uint32_t);
static uint32_t allocateCluster(struct fsHandle *, uint32_t);
static uint8_t compareStrings(const char *, const char *);
static char *getChunk(const char *, char *);
static uint8_t strLen(const char *);
//---------------------------------------------------------------------------
uint8_t fsLoad(struct fsHandle *desc)
{
  uint16_t tmp;
  uint8_t value;

  desc->state = FS_CLOSED;
  desc->buffer = NULL;

  desc->sectorList.size = 0;
  desc->sectorList.top = 0;

  if (!openDevice())
    return FS_DEVICE_ERROR;

  desc->currentSector = 0;
  if (!readSector(desc, 0))
  {
    fsUnload(desc);
    return FS_READ_ERROR;
  }

  tmp = *((uint8_t *)(desc->buffer + 0x0D));
  value = 0;
  while (tmp >>= 1)
    value++;
  desc->sectorsPerCluster = value;

  if (*((uint16_t *)(desc->buffer + 0x0B)) != (1 << SECTOR_SIZE_POW))
    return FS_DEVICE_ERROR;

  desc->fatCount = *((uint8_t *)(desc->buffer + 0x10));
  desc->sectorsPerFAT = *((uint32_t *)(desc->buffer + 0x24));
  desc->fatSector = *((uint16_t *)(desc->buffer + 0x0E));
  desc->dataSector = desc->fatSector + *((uint8_t *)(desc->buffer + 0x10)) * *((uint32_t *)(desc->buffer + 0x24));
  desc->rootCluster = *((uint32_t *)(desc->buffer + 0x2C));
  desc->state = FS_OPENED;
  desc->streams = 0;
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsUnload(struct fsHandle *desc)
{
  if (desc->streams || (desc->state == FS_CLOSED))
    return FS_ERROR;
  closeDevice();
  desc->state = FS_CLOSED;
  return FS_OK;
}
//---------------------------------------------------------------------------
static uint32_t getNextCluster(struct fsHandle *desc, uint32_t value)
{
  uint32_t newCluster;
  if (!readSector(desc, desc->fatSector + (value >> (SECTOR_SIZE_POW - 2))))
    return 0;
  newCluster = *((uint32_t *)(desc->buffer + ((value & ((1 << (SECTOR_SIZE_POW - 2)) - 1)) << 2)));
//  cout << "----Fetched cluster: " << newCluster << " valid: " << (int)(newCluster >= 0x00000002 && newCluster <= 0x0FFFFFEF) << endl;
//  if (newCluster >= 0x000000001 && newCluster <= 0xFFFFFFF5)
  if (newCluster >= 0x00000002 && newCluster <= 0x0FFFFFEF)
    return newCluster;
  else
    return 0;
}
//---------------------------------------------------------------------------
//Shift items from [0..count-1] to [1..count]
void shiftBuffer(struct item *data, uint8_t count)
{
  while (count)
  {
    data[count] = data[count - 1];
    count--;
  }
}
//---------------------------------------------------------------------------
/*uint8_t getLast(struct buffer *buf)
{
  if (size < BUFFER_LEVEL)
    return current;
  if (
}*/
//---------------------------------------------------------------------------
static uint8_t readSector(struct fsHandle *desc, uint32_t sector)
{
  if (desc->sectorList.size /*desc->buffer */&& (sector == desc->currentSector))
    return 1;
  uint8_t counter;
  for (counter = 0; counter < desc->sectorList.size; counter++)
    if (desc->sectorList.records[counter].sector == sector)
    {
/*      tmp = desc->sectorList[counter];
      shiftBuffer(desc->sectorList, counter);
      bufferItems[0] = tmp;
      break;*/
      desc->buffer = desc->sectorList.records[counter].data;
      desc->currentSector = sector;
      cout << "Sector " << sector << " loaded from cache, id " << (int)counter << endl;
      return 1;
    }
  if (counter == desc->sectorList.size)
  {
    desc->buffer = desc->sectorList.records[desc->sectorList.top].data;
    if (!readBlock(desc->buffer, sector))
      return 0;
    desc->sectorList.records[desc->sectorList.top].sector = sector;
    desc->currentSector = sector;
    if (desc->sectorList.size < BUFFER_LEVEL)
      desc->sectorList.size++;
    desc->sectorList.top++;
    if (desc->sectorList.top == BUFFER_LEVEL)
      desc->sectorList.top = 0;
//    cout << "Sector " << sector << " loaded from device, new top " << (int)(desc->sectorList.top) << endl;
    return 1;
  }
/*  desc->buffer = desc->sectorList[0].data;
  desc->currentSector = desc->sectorList[0].sector;*/
  //desc->currentSector = sector;
  return 0;
}
//---------------------------------------------------------------------------
static uint32_t allocateCluster(struct fsHandle *desc, uint32_t reference)
{
  uint32_t sector = 0;
  uint8_t record, fat;
//  cout << "Trying to allocate" << endl;
  for (; sector < desc->sectorsPerFAT; sector++)
  {
    if (!readSector(desc, desc->fatSector + sector))
      return 0;
    if (!sector)
      record = 2;
    else
      record = 0;
    for (; record < (1 << (SECTOR_SIZE_POW - 2)); record++)
    {
      if (*((uint32_t *)(desc->buffer + (record << 2))) == 0x00000000) //Is free
      {
//        *((uint32_t *)(desc->buffer + (record << 2))) = 0xFFFFFFFF;
        *((uint32_t *)(desc->buffer + (record << 2))) = 0x0FFFFFFF;
        for (fat = 0; fat < desc->fatCount; fat++)
          if (!writeSector(desc, desc->fatSector + (uint32_t)fat * desc->sectorsPerFAT + sector))
            return 0;
        if (!readSector(desc, desc->fatSector + (reference >> (SECTOR_SIZE_POW - 2))))
          return 0;
        *((uint32_t *)(desc->buffer + ((reference & ((1 << (SECTOR_SIZE_POW - 2)) - 1)) << 2))) = ((sector) << (SECTOR_SIZE_POW - 2)) + record;
        for (fat = 0; fat < desc->fatCount; fat++)
          if (!writeSector(desc, desc->fatSector + (uint32_t)fat * desc->sectorsPerFAT + (reference >> (SECTOR_SIZE_POW - 2))))
            return 0;
//        cout << "Alloc new: " << dec << (long)((uint32_t)(sector) * 0x80 + record) << " old: " << reference << endl;
        return ((sector) << (SECTOR_SIZE_POW - 2)) + record;
      }
    }
  }
  return 0;
}
//---------------------------------------------------------------------------
static uint8_t writeSector(struct fsHandle *desc, uint32_t sector)
{
  if (!writeBlock(desc->buffer, sector))
    return 0;
  else
    return 1;
}
//---------------------------------------------------------------------------
static uint8_t compareStrings(const char *src, const char *dest)
{
  while (*dest && *src)
    if (*dest++ != *src++)
      return 0; //FIXME
  return (*dest == *src);
}
//---------------------------------------------------------------------------
//dest length is 13: 8 name + . + 3 ext + null
static char *getChunk(const char *src, char *dest)
{
  while (*src && *src == '/')
    src++;
  while (*src)
  {
    switch (*src)
    {
      case '/':
        *dest = '\0';
        return (char *)src;
      case ' ':
        src++;
        continue;
    }
    *dest++ = *src++;
  }
  *dest = '\0';
  return (char *)src;
}
//---------------------------------------------------------------------------
static uint8_t strLen(const char *str)
{
  uint8_t tmp = 0;
  while (*str++)
    tmp++;
  return tmp;
}
//---------------------------------------------------------------------------
//FIXME return value == void?
static uint8_t followPath(struct fsHandle *desc, struct fsEntry *item, const char *path)
{
  uint32_t currentCluster = desc->rootCluster;
  char entryName[13];
  path = getChunk(path, entryName);
  if (!strLen(entryName))
  {
    item->cluster = desc->rootCluster;
    return 1;
  }
  item->parent = 0;
  item->index = 0;
  while (1)
  {
//    cout << "Fetching from cluster: " << currentCluster << endl;
    item->parent = currentCluster;
    if (fetchEntry(desc, item, &currentCluster))
    {
//      cout << "Checking: " << entryName << " and " << item->name << endl;
      if (compareStrings(item->name, entryName))
      {
//        cout << "Match: " << entryName << " and " << item->name << endl;
        path = getChunk(path, entryName);
        if (!strLen(entryName))
          return 1;
        currentCluster = item->cluster;
        item->index = 0;
      }
    }
    else
      break;
  }
  return 0;
}
//---------------------------------------------------------------------------
uint8_t fsOpenFile(struct fsHandle *desc, struct fsFile *arg, const char *path, uint8_t mode)
{
  struct fsEntry item;
//  if (!desc || !arg || !path)
//    return 0;
  arg->mode = FS_NONE;
  if ((desc->state != FS_OPENED) || (desc->streams >= FS_MAX_STREAMS))
    return FS_ERROR;
  followPath(desc, &item, path); //FIXME
//Not found or hidden, system, volume name, directory
  if (!item.cluster || (item.attribute & 0x1E))
    return FS_NOT_FOUND;
  if ((item.attribute & 0x01) && ((mode == FS_WRITE) || (mode == FS_APPEND)))
    return FS_ERROR;
  desc->streams++;
  arg->descriptor = desc;
  arg->mode = mode;
  arg->cluster = item.cluster;
  arg->position = 0;
  arg->currentCluster = arg->cluster;
  arg->currentSector = 0;
  arg->size = item.size;
  arg->parentCluster = item.parent;
  arg->parentIndex = item.index - 1;
  if (mode == FS_APPEND)
    if (!fsSeek(arg, arg->size))
      return FS_ERROR;
//  if (mode == FS_WRITE)
//    fsFree()
//  cout << "Opened cluster: " << dec << arg->currentCluster << " parent: " << arg->parentCluster << " size: " << arg->size << endl;
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsCloseFile(struct fsFile *arg) //FIXME void?
{
  if (arg->mode == FS_NONE)
    return FS_ERROR;
  arg->descriptor->streams--;
  arg->mode = FS_NONE;
//  arg->descriptor = 0;
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsSeek(struct fsFile *arg, uint32_t pos)
{
  uint32_t clusterCount;
  uint32_t current;
//  if (!arg || !arg->descriptor)
//    return 0;
//  cout << "SEEK TO: " << pos << " SIZE: " << arg->size << endl;
  if (pos > arg->size)
    return FS_ERROR;
  clusterCount = pos >> (arg->descriptor->sectorsPerCluster + SECTOR_SIZE_POW);
  current = arg->cluster;
  while (clusterCount--)
  {
    current = getNextCluster(arg->descriptor, current);
    if (!current)
      return FS_READ_ERROR;
  }
  arg->currentCluster = current;
  arg->position = pos;
  arg->currentSector = (pos >> SECTOR_SIZE_POW) & ((1 << arg->descriptor->sectorsPerCluster) - 1);
  return FS_OK;
}
//---------------------------------------------------------------------------
uint16_t fsWrite(struct fsFile *arg, uint8_t *buffer, uint16_t count)
{
  uint16_t delta, tmp = 0;
  uint32_t tmpSector;
//  if (!arg || !arg->descriptor)
//    return 0;
  if ((arg->mode != FS_APPEND) && (arg->mode != FS_WRITE))
    return 0;
  while (count)
  {
//    cout << "Cluster: " << dec << arg->currentCluster << " sector: " << (int)arg->currentSector << " pos: " << arg->position << endl;
    tmpSector = arg->descriptor->dataSector + ((arg->currentCluster - 2) << arg->descriptor->sectorsPerCluster) + arg->currentSector;
    if (!readSector(arg->descriptor, tmpSector)) //Error
      break;
    delta = (arg->size + tmp) & ((1 << SECTOR_SIZE_POW) - 1);
    for (; (count > 0) && (delta < (1 << SECTOR_SIZE_POW)); count--, delta++)
      arg->descriptor->buffer[delta] = *(buffer + tmp++);
    if (!writeSector(arg->descriptor, tmpSector)) //Error
      break;
    if (delta >= (1 << SECTOR_SIZE_POW))
    {
      arg->currentSector++;
      if (arg->currentSector >= 1 << arg->descriptor->sectorsPerCluster)
      {
        arg->currentSector = 0;
        arg->currentCluster = allocateCluster(arg->descriptor, arg->currentCluster);
        if (!arg->currentCluster) //Error, update file and return char count
          break;
      }
    }
  }
  arg->size += tmp;
  arg->position = arg->size;
//Update parent sector
  tmpSector = arg->descriptor->dataSector + ((arg->parentCluster - 2) << arg->descriptor->sectorsPerCluster) + (arg->parentIndex >> (SECTOR_SIZE_POW - 5));
  if (!readSector(arg->descriptor, tmpSector))
    return 0;
  *((uint32_t *)(arg->descriptor->buffer + ((arg->parentIndex & ((1 << (SECTOR_SIZE_POW - 5)) - 1)) << 5) + 0x1C)) = arg->size;
  if (!writeSector(arg->descriptor, tmpSector))
    return 0;
  return tmp;
}
//---------------------------------------------------------------------------
uint16_t fsRead(struct fsFile *arg, uint8_t *buffer, uint16_t count)
{
  uint16_t delta, tmp = 0;
  uint32_t tmpSector;
//  if (!arg || !arg->descriptor || (arg->mode != FS_READ))
//    return 0;
  if (arg->mode != FS_READ)
    return 0;
  if (count > arg->size - arg->position)
    count = arg->size - arg->position;
  while (count)
  {
//    cout << "Cluster: " << dec << arg->currentCluster << " sector: " << (int)arg->currentSector << " pos: " << arg->position << endl;
    tmpSector = arg->descriptor->dataSector + ((arg->currentCluster - 2) << arg->descriptor->sectorsPerCluster) + arg->currentSector;
    if (!readSector(arg->descriptor, tmpSector)) //Error
      break;
    delta = (arg->position + tmp) & ((1 << SECTOR_SIZE_POW) - 1);
    for (; (count > 0) && (delta < (1 << SECTOR_SIZE_POW)); count--, delta++)
      *(buffer + tmp++) = arg->descriptor->buffer[delta];
    if (delta >= (1 << SECTOR_SIZE_POW))
    {
      arg->currentSector++;
      if (arg->currentSector >= (1 << arg->descriptor->sectorsPerCluster))
      {
        arg->currentSector = 0;
        arg->currentCluster = getNextCluster(arg->descriptor, arg->currentCluster);
        if (!arg->currentCluster) //Error, return char count
          break;
      }
    }
  }
  arg->position += tmp;
  return tmp;
}
//---------------------------------------------------------------------------
static uint8_t fetchEntry(struct fsHandle *desc, struct fsEntry *entry, uint32_t *cluster)
{
  uint8_t *ptr = 0;
  uint8_t counter;
  uint32_t sector;
  entry->attribute = 0;
  entry->cluster = 0;
  entry->size = 0;
  do {
    if (entry->index >= (uint16_t)1 << (SECTOR_SIZE_POW - 5 + desc->sectorsPerCluster))
    {
      *cluster = getNextCluster(desc, *cluster);
      if (!*cluster)
        return 0;
      entry->index -= 1 << (SECTOR_SIZE_POW - 5 + desc->sectorsPerCluster);
//      entry->index &= 0xFFFF >> (28 - desc->sectorsPerCluster);
    }
//    cout << "cluster: " << *cluster << " sector: " << sector << " pos: " << entry->index << endl;
    sector = desc->dataSector + ((*cluster - 2) << desc->sectorsPerCluster) + (entry->index >> 4);
    if (!readSector(desc, sector))
      return 0;
    ptr = desc->buffer + ((entry->index & 0xF) << 5);
    entry->index++;
    if (!*ptr) //No more entries
      return 0;
  } while ((*ptr == 0xE5) || (*((uint8_t *)(ptr + 0x0B)) & 0x08)); //System or empty
  entry->attribute = *((uint8_t *)(ptr + 0x0B));
  if (!(entry->attribute & 0x10)) //Not directory
    entry->size = *((uint32_t *)(ptr + 0x1C));
  entry->cluster = (((uint32_t)*((uint16_t *)(ptr + 0x14))) << 16) | *((uint16_t *)(ptr + 0x1A));
  //time = *((uint16_t *)(ptr + 0x16));
  //date = *((uint16_t *)(ptr + 0x18));
  for (counter = 0; counter < 8; counter++)
    entry->name[counter] = *ptr++;
  if (!(entry->attribute & 0x10) && (*ptr != ' ')) //Not directory
  {
    entry->name[8] = '.';
    for (counter = 9; counter < 12; counter++)
      entry->name[counter] = *ptr++;
    entry->name[12] = '\0';
  }
  else
    entry->name[8] = '\0';
  getChunk(entry->name, entry->name);
  return 1;
}
