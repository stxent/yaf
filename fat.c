#include <string.h>
#include "fat.h"
#include "mem.h"
#if defined (FS_WRITE_ENABLED) && defined (FS_RTC_ENABLED)
#include "rtc.h"
#endif
//---------------------------------------------------------------------------
#define FS_FLAG_RO      0x01 /* Read only */
#define FS_FLAG_HIDDEN  0x02
#define FS_FLAG_SYSTEM  0x0C /* System (0x04) or volume label (0x08) */
#define FS_FLAG_DIR     0x10 /* Subdirectory */
#define FS_FLAG_ARCHIVE 0x20
//---------------------------------------------------------------------------
//Entry count per directory sector
#define ENTRY_COUNT (SECTOR_SIZE - 5)
//Entry count per cluster
#define ENTRIES_IN_CLUSTER(arg) (1 << (ENTRY_COUNT + arg->clusterSize))
//Entries (2^ENTRY_COUNT) per FAT sector
#define FAT_ENTRY_COUNT (SECTOR_SIZE - 2)
//Cluster entry offset in FAT sector
#define FAT_ENTRY_OFFSET(arg) (((arg) & ((1 << FAT_ENTRY_COUNT) - 1)) << 2)
//Calculate sector position from cluster
#define SECTOR(descriptor, cluster) (descriptor->dataSector + (((cluster) - 2) << descriptor->clusterSize))
//Directory entry position in cluster
#define ENTRY_SECTOR(index) ((index) >> ENTRY_COUNT)
//Directory entry offset in sector
#define ENTRY_OFFSET(index) (((index) & ((1 << ENTRY_COUNT) - 1)) << 5)
//---------------------------------------------------------------------------
#ifdef DEBUG
#include <iostream>
using namespace std;
long readExcess = 0;
#endif
//---------------------------------------------------------------------------
static uint8_t readSector(struct fsHandle *, uint32_t);
static uint8_t fetchEntry(struct fsHandle *, struct fsEntry *);
static const char *followPath(struct fsHandle *, struct fsEntry *, const char *);
static uint8_t getNextCluster(struct fsHandle *, uint32_t *);
static const char *getChunk(const char *, char *);
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
static uint8_t freeChain(struct fsHandle *, uint32_t);
static uint8_t writeSector(struct fsHandle *, uint32_t);
static uint8_t allocateCluster(struct fsHandle *, uint32_t *);
static uint8_t createEntry(struct fsHandle *, struct fsEntry *, const char *);
static uint8_t updateTable(struct fsHandle *, uint32_t);
#endif
//---------------------------------------------------------------------------
//struct BootSector
//{
//  char code[3]; /* Jump code */
//  char osName[8]; /* Name of the formatting program */
//  uint16_t bytesPerSector; //+
//  uint8_t sectorsPerCluster; //+
//  uint16_t reservedSectors; //+
//  uint8_t fatCopies; //+
//  char reserved0[4];
//  uint8_t mediaDescriptor; //TODO rm
//  char reserved1[2];
//  uint16_t sectorsPerTrack; //TODO rm
//  uint16_t heads; //TODO rm
//  uint32_t firstSector;
//  uint32_t partitionSize; /* Sectors per partition */ //+
//  uint32_t fatSize; /* Sectors per FAT record */ //+
//  uint16_t fatFlags;
//  uint16_t fatDriveVersion; //TODO rm
//  uint32_t firstCluster; /* Root directory cluster */ //+
//  uint16_t infoSector; /* Sector number for information sector */ //+
//  uint16_t backupSector; //TODO rm
//  char reserved2[12];
//
//  uint8_t driveNumber;
//  uint8_t currentHead;
////  uint8_t signature;
//  char unused;
//  uint32_t serialNumber;
//  char volumeLabel[11];
//  char systemId[8];
//  char bootCode[420];
//  uint16_t signature; //+
//};
//---------------------------------------------------------------------------
struct BootSector
{
  char space0[11];
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectors;
  uint8_t fatCopies;
  char space1[15];
  uint32_t partitionSize; /* Sectors per partition */
  uint32_t fatSize; /* Sectors per FAT record */
  char space2[4];
  uint32_t rootCluster; /* Root directory cluster */
  uint16_t infoSector; /* Sector number for information sector */
  char space3[460];
  uint16_t bootSignature;
};
//---------------------------------------------------------------------------
struct InfoSector
{
  uint32_t firstSignature;
  char space0[480];
  uint32_t infoSignature;
  uint32_t freeClusters;
  uint32_t lastAllocated;
  char space1[14];
  uint16_t bootSignature;
};
//---------------------------------------------------------------------------
uint8_t fsLoad(struct fsHandle *fsDesc, struct sDevice *device, uint8_t *memBuffer)
{
  struct BootSector *boot;
  struct InfoSector *info;
  uint16_t tmp;

  fsDesc->buffer = memBuffer;
  fsDesc->device = device;
  /* Read first sector */
  fsDesc->currentSector = 0;
  if (readSector(fsDesc, 0))
    return FS_READ_ERROR;
  boot = (struct BootSector *)fsDesc->buffer;
  /* Check boot sector signature (55AA at 0x01FE) */
  if (boot->bootSignature != 0xAA55)
    return FS_DEVICE_ERROR;
  /* Check sector size, fixed size of 2^SECTOR_SIZE allowed */
  if (boot->bytesPerSector != (1 << SECTOR_SIZE))
    return FS_DEVICE_ERROR;
  /* Calculate sectors per cluster count */
  tmp = boot->sectorsPerCluster;
  fsDesc->clusterSize = 0;
  while (tmp >>= 1)
    fsDesc->clusterSize++;

  fsDesc->fatSector = boot->reservedSectors;
  fsDesc->dataSector = fsDesc->fatSector + boot->fatCopies * boot->fatSize;
  fsDesc->rootCluster = boot->rootCluster;
#ifdef FS_WRITE_ENABLED
  fsDesc->fatCount = boot->fatCopies;
  fsDesc->sectorsPerFAT = boot->fatSize;
  fsDesc->clusterCount = ((boot->partitionSize - fsDesc->dataSector) >> fsDesc->clusterSize) + 2;
  fsDesc->infoSector = boot->infoSector;
#ifdef DEBUG
  cout << "Partition sector count: " << boot->partitionSize << endl;
#endif

  if (readSector(fsDesc, fsDesc->infoSector))
    return FS_READ_ERROR;
  info = (struct InfoSector *)fsDesc->buffer;
  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
    return FS_DEVICE_ERROR;
  fsDesc->lastAllocated = info->lastAllocated;
#ifdef DEBUG
  cout << "Free clusters: " << info->freeClusters << endl;
#endif
#endif
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsUnload(struct fsHandle *fsDesc)
{
  return FS_OK;
}
//---------------------------------------------------------------------------
static uint8_t getNextCluster(struct fsHandle *fsDesc, uint32_t *cluster)
{
  uint32_t nextCluster;

  if (readSector(fsDesc, fsDesc->fatSector + (*cluster >> FAT_ENTRY_COUNT)))
    return FS_READ_ERROR;
  nextCluster = *((uint32_t *)(fsDesc->buffer + FAT_ENTRY_OFFSET(*cluster)));
  if (nextCluster >= 0x00000002 && nextCluster <= 0x0FFFFFEF)
  {
    *cluster = nextCluster;
    return FS_OK;
  }
  else
    return FS_EOF;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
#ifdef DEBUG
uint32_t countFree(struct fsHandle *fsDesc)
{
  uint32_t res;
  uint32_t *count = new uint32_t[fsDesc->fatCount];

  for (uint8_t fat = 0; fat < fsDesc->fatCount; fat++)
  {
    uint16_t offset;
    count[fat] = 0;
    for (uint32_t current = 0; current < fsDesc->clusterCount; current++)
    {
      if (readSector(fsDesc, fsDesc->fatSector + (current >> FAT_ENTRY_COUNT)))
        return FS_READ_ERROR;
      offset = (current & ((1 << FAT_ENTRY_COUNT) - 1)) << 2;
      if (*((uint32_t *)(fsDesc->buffer + offset)) == 0x00000000)
        count[fat]++;
    }
  }
  for (uint8_t i = 0; i < fsDesc->fatCount; i++)
    for (uint8_t j = 0; j < fsDesc->fatCount; j++)
      if ((i != j) && (count[i] != count[j]))
      {
        cout << "FAT records differ: " << count[i] << " and " << count[j] << endl;
      }
  res = count[0];
  delete[] count;
  return res;
}
#endif
#endif
//---------------------------------------------------------------------------
static uint8_t readSector(struct fsHandle *fsDesc, uint32_t sector)
{
  if (sector && (sector == fsDesc->currentSector))
#ifdef DEBUG
  {
    readExcess++;
    return FS_OK;
  }
#else
    return FS_OK;
#endif
  if (sRead(fsDesc->device, fsDesc->buffer, sector))
    return FS_READ_ERROR;
  fsDesc->currentSector = sector;
  return FS_OK;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
static uint8_t writeSector(struct fsHandle *fsDesc, uint32_t sector)
{
  if (sWrite(fsDesc->device, fsDesc->buffer, sector))
    return FS_WRITE_ERROR;
  fsDesc->currentSector = sector;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
/* Copy current sector into FAT sectors located at offset */
#ifdef FS_WRITE_ENABLED
static uint8_t updateTable(struct fsHandle *fsDesc, uint32_t offset)
{
  uint8_t fat;

  for (fat = 0; fat < fsDesc->fatCount; fat++)
  {
    if (writeSector(fsDesc, fsDesc->fatSector + (uint32_t)fat * fsDesc->sectorsPerFAT + offset))
      return FS_WRITE_ERROR;
  }
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
static uint8_t allocateCluster(struct fsHandle *fsDesc, uint32_t *cluster)
{
  struct InfoSector *info;
  uint16_t offset;
  uint32_t current = fsDesc->lastAllocated + 1;

  for (; current != fsDesc->lastAllocated; current++)
  {
    if (current >= fsDesc->clusterCount)
#ifdef DEBUG
    {
      cout << "Reached end of partition, continue from third cluster" << endl;
      current = 2;
    }
#else
      current = 2;
#endif
    if (readSector(fsDesc, fsDesc->fatSector + (current >> FAT_ENTRY_COUNT)))
      return FS_READ_ERROR;
    offset = (current & ((1 << FAT_ENTRY_COUNT) - 1)) << 2;
    /* Is cluster free */
    if (*((uint32_t *)(fsDesc->buffer + offset)) == 0x00000000)
    {
      *((uint32_t *)(fsDesc->buffer + offset)) = 0x0FFFFFFF;
      if ((!*cluster || ((*cluster >> FAT_ENTRY_COUNT) != (current >> FAT_ENTRY_COUNT))) && updateTable(fsDesc, (current >> FAT_ENTRY_COUNT)))
        return 1;
      if (*cluster)
      {
        if (readSector(fsDesc, fsDesc->fatSector + (*cluster >> FAT_ENTRY_COUNT)))
          return FS_READ_ERROR;
        *((uint32_t *)(fsDesc->buffer + FAT_ENTRY_OFFSET(*cluster))) = current;
        if (updateTable(fsDesc, *cluster >> FAT_ENTRY_COUNT))
          return FS_WRITE_ERROR;
      }
#ifdef DEBUG
      cout << "Allocated new cluster: " << current;
      cout << ", reference: " << *cluster << endl;
#endif
      *cluster = current;
      /* Update information sector */
      if (readSector(fsDesc, fsDesc->infoSector))
        return FS_READ_ERROR;
      info = fsDesc->buffer;
      /* Set last allocated cluster */
      info->lastAllocated = current;
      fsDesc->lastAllocated = current;
      /* Update free clusters count */
      info->freeClusters--;
      if (writeSector(fsDesc, fsDesc->infoSector))
        return FS_WRITE_ERROR;
      return FS_OK;
    }
  }
#ifdef DEBUG
  cout << "Allocation error, possibly partition is full" << endl;
#endif
  return FS_ERROR;
}
#endif
//---------------------------------------------------------------------------
/* dest length is 13: 8 name + dot + 3 extension + null */
static const char *getChunk(const char *src, char *dest)
{
  uint8_t counter = 0;

  if (!*src)
    return src;
  if (*src == '/')
  {
    *dest++ = '/';
    *dest = '\0';
    return (src + 1);
  }
  while (*src && (counter++ < 12))
  {
    if (*src == '/')
    {
      src++;
      break;
    }
    if (*src == ' ')
    {
      src++;
      continue;
    }
    *dest++ = *src++;
  }
  *dest = '\0';
  return src;
}
//---------------------------------------------------------------------------
/* Members entry->index and entry->parent have to be initialized */
static uint8_t fetchEntry(struct fsHandle *fsDesc, struct fsEntry *entry)
{
  uint32_t sector;
  uint8_t *ptr;

  entry->attribute = 0;
  entry->cluster = 0;
  entry->size = 0;
  while (1)
  {
    if (entry->index >= ENTRIES_IN_CLUSTER(fsDesc))
    {
      /* Check clusters until end of directory (EOF entry in FAT) */
      if (getNextCluster(fsDesc, &(entry->parent)))
        return FS_READ_ERROR;
      entry->index = 0;
    }
    sector = fsDesc->dataSector + ((entry->parent - 2) << fsDesc->clusterSize) + ENTRY_SECTOR(entry->index);
//    if (!(entry->index & ((1 << ENTRY_COUNT) - 1)) && readSector(fsDesc, sector)) //FIXME possibly sector not read
    if (readSector(fsDesc, sector))
      return FS_READ_ERROR;
    ptr = fsDesc->buffer + ENTRY_OFFSET(entry->index);
    if (!*ptr) /* No more entries */
      return FS_EOF;
    if (*ptr != 0xE5) /* Entry exists */
      break;
    entry->index++;
  }
  entry->attribute = *((uint8_t *)(ptr + 0x0B));
  /* Copy file size, when entry is not directory */
  if (!(entry->attribute & FS_FLAG_DIR))
    entry->size = *((uint32_t *)(ptr + 0x1C));
  entry->cluster = (((uint32_t)*((uint16_t *)(ptr + 0x14))) << 16) | *((uint16_t *)(ptr + 0x1A));
#ifdef DEBUG
  entry->time = *((uint16_t *)(ptr + 0x16));
  entry->date = *((uint16_t *)(ptr + 0x18));
#endif
  /* Copy entry name */
  memcpy(entry->name, ptr, 8);
  /* Add dot, when entry is not directory or extension exists */
  if (!(entry->attribute & FS_FLAG_DIR) && (*(ptr + 8) != ' '))
  {
    entry->name[8] = '.';
    /* Copy entry extension */
    memcpy(entry->name + 9, ptr + 8, 3);
    entry->name[12] = '\0';
  }
  else
    entry->name[8] = '\0';
  getChunk(entry->name, entry->name);
  return FS_OK;
}
//---------------------------------------------------------------------------
static const char *followPath(struct fsHandle *fsDesc, struct fsEntry *item, const char *path)
{
  char entryName[13];

  path = getChunk(path, entryName);
  if (!strlen(entryName))
    return 0;
  if (entryName[0] == '/')
  {
    item->size = 0;
    item->cluster = fsDesc->rootCluster;
    item->attribute = FS_FLAG_DIR;
    return path;
  }
  item->parent = item->cluster;
  item->index = 0;
  while (!fetchEntry(fsDesc, item))
  {
    if (!strcmp(item->name, entryName))
      return path;
    item->index++;
  }
  return 0;
}
//---------------------------------------------------------------------------
uint8_t fsOpen(struct fsHandle *fsDesc, struct fsFile *fileDesc, const char *path, uint8_t mode)
{
  const char *followedPath;
  struct fsEntry item;

  fileDesc->state = FS_CLOSED;
  fileDesc->mode = mode;
  while (*path && (followedPath = followPath(fsDesc, &item, path)))
    path = followedPath;
  if (*path) /* Null when full path followed, not null when entry not found */
  {
#ifdef FS_WRITE_ENABLED
    if (mode == FS_WRITE)
    {
      item.attribute = 0;
      if (createEntry(fsDesc, &item, path))
        return FS_ERROR;
    }
    else
      return FS_NOT_FOUND;
#else
    return FS_NOT_FOUND;
#endif
  }
  /* Not found if system, volume name or directory */
  if (item.attribute & (FS_FLAG_SYSTEM | FS_FLAG_DIR))
    return FS_NOT_FOUND;
#ifdef FS_WRITE_ENABLED
  /* Attempt to write into read-only file */
  if ((item.attribute & FS_FLAG_RO) && ((mode == FS_WRITE) || (mode == FS_APPEND)))
    return FS_ERROR;
#endif
  fileDesc->descriptor = fsDesc;
  fileDesc->cluster = item.cluster;
  fileDesc->currentCluster = item.cluster;
  fileDesc->currentSector = 0;
  fileDesc->position = 0;
  fileDesc->size = item.size;
#ifdef DEBUG
  fileDesc->data = item;
#endif
#ifdef FS_WRITE_ENABLED
  fileDesc->parentCluster = item.parent;
  fileDesc->parentIndex = item.index;
  if ((mode == FS_WRITE) && !*path && fileDesc->size && (fsTruncate(fileDesc) != FS_OK))
    return FS_ERROR;
  /* In append mode file pointer moves to end of file */
  //FIXME compare file size with zero?
  if ((mode == FS_APPEND) && (fsSeek(fileDesc, fileDesc->size) != FS_OK))
    return FS_ERROR;
#endif
  fileDesc->state = FS_OPENED;
  return FS_OK;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
static uint8_t freeChain(struct fsHandle *fsDesc, uint32_t cluster)
{
  struct InfoSector *info;
  uint16_t freeCount = 0;
  uint32_t current = cluster;
  uint32_t next;

  if (!current)
    return FS_OK; /* Already empty */
  while (current >= 0x00000002 && current <= 0x0FFFFFEF)
  {
    /* Get FAT sector with next cluster value */
    if (readSector(fsDesc, fsDesc->fatSector + (current >> FAT_ENTRY_COUNT)))
      return FS_READ_ERROR;
    /* Free cluster */
    next = *((uint32_t *)(fsDesc->buffer + FAT_ENTRY_OFFSET(current)));
    *((uint32_t *)(fsDesc->buffer + FAT_ENTRY_OFFSET(current))) = 0;
#ifdef DEBUG
    if (current >> FAT_ENTRY_COUNT != next >> FAT_ENTRY_COUNT)
    {
      cout << "FAT sectors differ, next: " << (next >> FAT_ENTRY_COUNT) << hex << " (0x" << (next >> FAT_ENTRY_COUNT) << ")" << dec;
      cout << ", current: " << (current >> FAT_ENTRY_COUNT) << endl;
    }
    cout << "Cleared cluster: " << current << endl;
#endif
    if ((current >> FAT_ENTRY_COUNT != next >> FAT_ENTRY_COUNT) && updateTable(fsDesc, (current >> FAT_ENTRY_COUNT)))
    {
      return FS_WRITE_ERROR;
    }
    freeCount++;
    current = next;
  }
  /* Update information sector */
  if (readSector(fsDesc, fsDesc->infoSector))
    return FS_READ_ERROR;
  info = fsDesc->buffer;
  /* Set free clusters count */
  info->freeClusters += freeCount;
  if (writeSector(fsDesc, fsDesc->infoSector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
uint8_t fsTruncate(struct fsFile *fileDesc)
{
  uint8_t *ptr;
  uint32_t current;

  if ((fileDesc->mode != FS_WRITE) && (fileDesc->mode != FS_APPEND))
    return FS_ERROR;
  if (freeChain(fileDesc->descriptor, fileDesc->cluster) != FS_OK)
    return FS_ERROR;
  current = SECTOR(fileDesc->descriptor, fileDesc->parentCluster) + ENTRY_SECTOR(fileDesc->parentIndex);
  if (readSector(fileDesc->descriptor, current))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  ptr = fileDesc->descriptor->buffer + ENTRY_OFFSET(fileDesc->parentIndex);
  /* Update file size */
  *((uint32_t *)(ptr + 0x1C)) = 0;
  /* Update first cluster */
  *((uint16_t *)(ptr + 0x14)) = 0; /* High 2 bytes of start cluster */
  *((uint16_t *)(ptr + 0x1A)) = 0; /* Low 2 bytes of start cluster */
#ifdef FS_RTC_ENABLED
  /* Update last modified date */
  *((uint16_t *)(ptr + 0x16)) = rtcGetTime(); /* Last modified time */
  *((uint16_t *)(ptr + 0x18)) = rtcGetDate(); /* Last modified date */
#endif
  if (writeSector(fileDesc->descriptor, current))
    return FS_WRITE_ERROR;
  fileDesc->cluster = 0;
  fileDesc->currentCluster = 0;
  fileDesc->currentSector = 0;
  fileDesc->size = 0;
  fileDesc->position = 0;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
/* Create new entry inside entry->parent chain */
/* Members entry->parent and entry->attribute have to be initialized */
static uint8_t createEntry(struct fsHandle *fsDesc, struct fsEntry *entry, const char *name)
{
  uint8_t counter;
//  uint16_t clusterCount = 0; /* Followed clusters count */
  uint32_t sector;
  uint8_t *ptr;

  entry->cluster = 0;
  entry->index = 0;
  for (counter = 0; *(name + counter); counter++)
  {
    if (name[counter] == '/')
      return FS_ERROR; /* One of folders in path does not exist */
  }
  while (1)
  {
    if (entry->index >= ENTRIES_IN_CLUSTER(fsDesc))
    {
      /* Try to get next cluster or allocate new cluster for folder */
      /* Max folder size is 2^16 entries */
      //TODO Add file limit
      //if (getNextCluster(fsDesc, &(entry->parent)) && (clusterCount < (1 << (16 - ENTRY_COUNT - fsDesc->clusterSize))))
      if (getNextCluster(fsDesc, &(entry->parent)))
      {
        if (allocateCluster(fsDesc, &(entry->parent)))
          return FS_ERROR;
        else
        {
          sector = SECTOR(fsDesc, entry->parent);
          memset(fsDesc->buffer, 0, (1 << SECTOR_SIZE));
          for (counter = 0; counter < (1 << fsDesc->clusterSize); counter++)
          {
            if (writeSector(fsDesc, sector + counter))
              return FS_WRITE_ERROR;
          }
        }
      }
      else
        return FS_ERROR; /* Directory full */
      entry->index = 0;
//      clusterCount++; //FIXME remove?
    }
    sector = fsDesc->dataSector + ((entry->parent - 2) << fsDesc->clusterSize) + ENTRY_SECTOR(entry->index);
    if (readSector(fsDesc, sector))
      return FS_ERROR;
    ptr = fsDesc->buffer + ENTRY_OFFSET(entry->index);
    if (!*ptr || (*ptr == 0xE5)) /* Empty or removed entry */
      break;
    entry->index++;
  }
  memset(ptr, 0x20, 11);
  for (counter = 0; *name && (*name != '.') && (counter < 8); counter++)
    *((char *)(ptr + counter)) = *name++;
  if (!(entry->attribute & FS_FLAG_DIR) && (*name == '.'))
  {
    for (counter = 8, name++; *name && (counter < 11); counter++)
      *((char *)(ptr + counter)) = *name++;
  }
  /* Fill entry fields with zeros */
  memset(ptr + 0x0C, 0, 0x20 - 0x0C);
  *((uint8_t *)(ptr + 0x0B)) = entry->attribute;
  //TODO save predefined cluster and size values?
#ifdef FS_RTC_ENABLED
  /* Last modified time and date */
  *((uint16_t *)(ptr + 0x16)) = rtcGetTime();
  *((uint16_t *)(ptr + 0x18)) = rtcGetDate();
#endif
  if (writeSector(fsDesc, sector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
uint8_t fsClose(struct fsFile *fileDesc)
{
  fileDesc->state = FS_CLOSED;
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsSeek(struct fsFile *fileDesc, uint32_t pos)
{
  uint32_t clusterCount, current;

  if (fileDesc->state != FS_OPENED)
    return FS_ERROR;
  if (pos > fileDesc->size)
    return FS_ERROR;
  clusterCount = pos;
  if (pos > fileDesc->position)
  {
    current = fileDesc->currentCluster;
    clusterCount -= fileDesc->position;
  }
  else
    current = fileDesc->cluster;
  clusterCount >>= fileDesc->descriptor->clusterSize + SECTOR_SIZE;
  while (clusterCount--)
  {
    if (getNextCluster(fileDesc->descriptor, &current))
      return FS_READ_ERROR;
  }
  fileDesc->currentCluster = current;
  fileDesc->position = pos;
  fileDesc->currentSector = (pos >> SECTOR_SIZE) & ((1 << fileDesc->descriptor->clusterSize) - 1);
  return FS_OK;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
uint8_t fsWrite(struct fsFile *fileDesc, uint8_t *buffer, uint16_t count, uint16_t *result)
{
  uint8_t *entry;
  uint16_t chunk, offset, written = 0;
  uint32_t tmpSector;

  if ((fileDesc->mode != FS_APPEND) && (fileDesc->mode != FS_WRITE))
    return FS_ERROR;
  if (!fileDesc->size)
  {
    if (allocateCluster(fileDesc->descriptor, &(fileDesc->cluster)))
      return FS_ERROR;
    fileDesc->currentCluster = fileDesc->cluster;
  }
  /* Checking file size limit (2 GiB) */
  if (fileDesc->size + count > 0x7FFFFFFF)
    count = 0x7FFFFFFF - fileDesc->size;
  while (count)
  {
    if (fileDesc->currentSector >= (1 << fileDesc->descriptor->clusterSize))
    {
      if (allocateCluster(fileDesc->descriptor, &(fileDesc->currentCluster)))
        return FS_WRITE_ERROR;
      fileDesc->currentSector = 0;
    }
    tmpSector = SECTOR(fileDesc->descriptor, fileDesc->currentCluster) + fileDesc->currentSector;
    offset = (fileDesc->size + written) & ((1 << SECTOR_SIZE) - 1); /* Position in sector */
    chunk = (1 << SECTOR_SIZE) - offset; /* Length of remaining sector space */
    chunk = (count < chunk) ? count : chunk;
    if (offset && readSector(fileDesc->descriptor, tmpSector))
      return FS_READ_ERROR;
    memcpy(fileDesc->descriptor->buffer + offset, buffer + written, chunk);
    if (writeSector(fileDesc->descriptor, tmpSector))
      return FS_WRITE_ERROR;
    written += chunk;
    count -= chunk;
    if (chunk + offset >= (1 << SECTOR_SIZE))
      fileDesc->currentSector++;
  }
  tmpSector = SECTOR(fileDesc->descriptor, fileDesc->parentCluster) + ENTRY_SECTOR(fileDesc->parentIndex);
  if (readSector(fileDesc->descriptor, tmpSector))
    return FS_READ_ERROR;
  /* Pointer to entry position in sector */
  entry = fileDesc->descriptor->buffer + ENTRY_OFFSET(fileDesc->parentIndex);
  /* Update first cluster when writing to empty file */
  if (!fileDesc->size)
  {
    *((uint16_t *)(entry + 0x14)) = fileDesc->cluster >> 16; /* High 2 bytes of start cluster */
    *((uint16_t *)(entry + 0x1A)) = fileDesc->cluster & 0xFFFF; /* Low 2 bytes of start cluster */
  }
  fileDesc->size += written;
  fileDesc->position = fileDesc->size;
  /* Update file size */
  *((uint32_t *)(entry + 0x1C)) = fileDesc->size;
#ifdef FS_RTC_ENABLED
  /* Update last modified date */
  *((uint16_t *)(entry + 0x16)) = rtcGetTime(); /* Last modified time */
  *((uint16_t *)(entry + 0x18)) = rtcGetDate(); /* Last modified date */
#endif
  if (writeSector(fileDesc->descriptor, tmpSector))
    return FS_WRITE_ERROR;
  *result = written;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
uint8_t fsRead(struct fsFile *fileDesc, uint8_t *buffer, uint16_t count, uint16_t *result)
{
  uint16_t chunk, offset, read = 0;

  if (fileDesc->mode != FS_READ)
    return FS_ERROR;
  if (count > fileDesc->size - fileDesc->position)
    count = fileDesc->size - fileDesc->position;
  if (!count)
    return FS_EOF;
  while (count)
  {
    if (fileDesc->currentSector >= (1 << fileDesc->descriptor->clusterSize))
    {
      if (getNextCluster(fileDesc->descriptor, &(fileDesc->currentCluster)))
        return FS_READ_ERROR;
      fileDesc->currentSector = 0;
    }
    if (readSector(fileDesc->descriptor, SECTOR(fileDesc->descriptor, fileDesc->currentCluster) + fileDesc->currentSector))
      return FS_READ_ERROR;
    offset = (fileDesc->position + read) & ((1 << SECTOR_SIZE) - 1); /* Position in sector */
    chunk = (1 << SECTOR_SIZE) - offset; /* Length of remaining sector space */
    chunk = (count < chunk) ? count : chunk;
    memcpy(buffer + read, fileDesc->descriptor->buffer + offset, chunk);
    read += chunk;
    count -= chunk;
    if (chunk + offset >= (1 << SECTOR_SIZE))
      fileDesc->currentSector++;
  }
  fileDesc->position += read;
  *result = read;
  return FS_OK;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
uint8_t fsRemove(struct fsHandle *fsDesc, const char *path)
{
  uint16_t index;
  uint32_t tmp; /* Stores first cluster of entry or entry sector */
  uint32_t parent;
  struct fsEntry item;

  while (path && *path)
    path = followPath(fsDesc, &item, path);
  if (!path)
    return FS_NOT_FOUND;
  /* Hidden, system, volume name */
  if (item.attribute & (FS_FLAG_HIDDEN | FS_FLAG_SYSTEM))
    return FS_NOT_FOUND;
  index = item.index;
  tmp = item.cluster; /* First cluster of entry */
  parent = item.parent;
  item.index = 2; /* Exclude . and .. */
  item.parent = item.cluster;
  /* Check if directory not empty */
  if ((item.attribute & FS_FLAG_DIR) && !fetchEntry(fsDesc, &item))
    return FS_ERROR;
  if (freeChain(fsDesc, tmp) != FS_OK)
    return FS_ERROR;
  /* Sector in FAT with entry description */
  tmp = SECTOR(fsDesc, parent) + ENTRY_SECTOR(index);
  if (readSector(fsDesc, tmp))
    return FS_READ_ERROR;
  /* Mark entry as free */
  *((uint8_t *)(fsDesc->buffer + ENTRY_OFFSET(index))) = 0xE5;
  if (writeSector(fsDesc, tmp))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
uint8_t fsOpenDir(struct fsHandle *fsDesc, struct fsDir *dirDesc, const char *path)
{
  struct fsEntry item;

  dirDesc->state = FS_CLOSED;
  while (path && *path)
    path = followPath(fsDesc, &item, path);
  if (!path)
    return FS_NOT_FOUND;
  /* Hidden, system, volume name or not directory */
  if (!(item.attribute & FS_FLAG_DIR) || (item.attribute & FS_FLAG_SYSTEM))
    return FS_NOT_FOUND;
#ifdef DEBUG
  dirDesc->data = item;
#endif
  dirDesc->descriptor = fsDesc;
  dirDesc->cluster = item.cluster;
  dirDesc->currentCluster = item.cluster;
  dirDesc->currentIndex = 0;
  dirDesc->state = FS_OPENED;
  return FS_OK;
}
//---------------------------------------------------------------------------
/* Name length is 13 characters and includes file name, dot and extension */
uint8_t fsReadDir(struct fsDir *dirDesc, char *name)
{
  struct fsEntry item;

  if (dirDesc->state != FS_OPENED)
    return FS_ERROR;
  item.parent = dirDesc->currentCluster;
  item.index = dirDesc->currentIndex; /* Fetch next entry */
  do
  {
    if (fetchEntry(dirDesc->descriptor, &item))
      return FS_NOT_FOUND;
    item.index++;
  }
  while (item.attribute & (FS_FLAG_HIDDEN | FS_FLAG_SYSTEM));
  /* Hidden and system entries not shown */
  dirDesc->currentIndex = item.index; /* Points to next item */
  dirDesc->currentCluster = item.parent;
  strcpy(name, item.name);
  return FS_OK;
}
//---------------------------------------------------------------------------
uint8_t fsSeekDir(struct fsDir *dirDesc, uint16_t pos)
{
  uint16_t clusterCount;
  uint32_t current;

  if (dirDesc->state != FS_OPENED)
    return FS_ERROR;

  //TODO add search from current position
  /*clusterCount = pos;
  if (pos > dirDesc->position)
  {
    current = dirDesc->currentCluster;
    clusterCount -= dirDesc->position;
  }
  else
    current = dirDesc->cluster;
  clusterCount >>= dirDesc->descriptor->clusterSize + SECTOR_SIZE - 5;*/

  current = dirDesc->cluster;
  clusterCount = pos >> (SECTOR_SIZE - 5 + dirDesc->descriptor->clusterSize);
  while (clusterCount--)
  {
    if (getNextCluster(dirDesc->descriptor, &current))
      return FS_READ_ERROR;
  }
  dirDesc->currentCluster = current;
  dirDesc->currentIndex = pos & (ENTRIES_IN_CLUSTER(dirDesc->descriptor) - 1);
  return FS_OK;
}
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
uint8_t fsMakeDir(struct fsHandle *fsDesc, const char *path)
{
  uint8_t counter;
  uint32_t tmpSector, parent;
  uint8_t *ptr;
  const char *followedPath;
  struct fsEntry item;

  while (*path && (followedPath = followPath(fsDesc, &item, path)))
  {
    parent = item.cluster;
    path = followedPath;
  }
  if (!*path) /* Entry with same name exists */
    return FS_ERROR;
  item.attribute = FS_FLAG_DIR; /* Create entry with directory attribute */
  if (createEntry(fsDesc, &item, path) || allocateCluster(fsDesc, &item.cluster))
    return FS_WRITE_ERROR;
  tmpSector = SECTOR(fsDesc, item.parent) + ENTRY_SECTOR(item.index);
  if (readSector(fsDesc, tmpSector))
    return FS_READ_ERROR;
  ptr = fsDesc->buffer + ENTRY_OFFSET(item.index);
  *((uint16_t *)(ptr + 0x14)) = item.cluster >> 16; /* High word */
  *((uint16_t *)(ptr + 0x1A)) = item.cluster & 0xFFFF; /* Low word */
  if (writeSector(fsDesc, tmpSector))
    return FS_WRITE_ERROR;
  tmpSector = SECTOR(fsDesc, item.cluster);
  /* Fill sector with zeros */
  memset(fsDesc->buffer, 0, (1 << SECTOR_SIZE));
  for (counter = (1 << fsDesc->clusterSize) - 1; counter > 0; counter--)
  {
    if (writeSector(fsDesc, tmpSector + counter))
      return FS_WRITE_ERROR;
  }
  /* Fill name and extension with spaces */
  memset(fsDesc->buffer + 0x01, 0x20, 11); /* Entry . name */
  memset(fsDesc->buffer + 0x22, 0x20, 11); /* Entry .. name */
  /* Current folder entry . */
  *((uint8_t *)(fsDesc->buffer)) = 0x2E;
  *((uint8_t *)(fsDesc->buffer + 0x0B)) = FS_FLAG_DIR;
  *((uint16_t *)(fsDesc->buffer + 0x14)) = item.cluster >> 16; /* High word */
  *((uint16_t *)(fsDesc->buffer + 0x1A)) = item.cluster & 0xFFFF; /* Low word */
#ifdef FS_RTC_ENABLED
  //Last modified time and date
  *((uint16_t *)(fsDesc->buffer + 0x16)) = rtcGetTime();
  *((uint16_t *)(fsDesc->buffer + 0x18)) = rtcGetDate();
#endif
  //Previous folder entry ..
  *((uint8_t *)(fsDesc->buffer + 0x20)) = 0x2E;
  *((uint8_t *)(fsDesc->buffer + 0x20 + 0x01)) = 0x2E;
  *((uint8_t *)(fsDesc->buffer + 0x20 + 0x0B)) = FS_FLAG_DIR;
  if (parent != fsDesc->rootCluster)
  {
    *((uint16_t *)(fsDesc->buffer + 0x20 + 0x14)) = parent >> 16; /* High word */
    *((uint16_t *)(fsDesc->buffer + 0x20 + 0x1A)) = parent & 0xFFFF; /* Low word */
  }
#ifdef FS_RTC_ENABLED
  /* Parent item date and time are same as current time and date */
  /* Date and time information consume 4 bytes */
  memcpy(fsDesc->buffer + 0x20 + 0x16, fsDesc->buffer + 0x16, 4);
#endif
  if (writeSector(fsDesc, tmpSector))
    return FS_WRITE_ERROR;
  return FS_OK;
}
#endif
//---------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
uint8_t fsMove(struct fsHandle *fsDesc, const char *path, const char *newPath)
{
  uint8_t attribute/*, counter*/;
  uint16_t index;
  uint32_t parent, cluster, size, tmpSector;
  uint8_t *ptr;
  const char *followedPath;
  struct fsEntry item;

  while (path && *path)
    path = followPath(fsDesc, &item, path);
  if (!path)
    return FS_NOT_FOUND;

  /* System entries are invisible */
  if (item.attribute & FS_FLAG_SYSTEM)
    return FS_NOT_FOUND;

  /* Save old entry data */
  attribute = item.attribute;
  index = item.index;
  parent = item.parent;
  cluster = item.cluster;
  size = item.size;

  while (*newPath && (followedPath = followPath(fsDesc, &item, newPath)))
    newPath = followedPath;
  if (!*newPath) //Entry with same name exists
    return FS_ERROR;

/*  if (parent == item.parent) //Same folder
  {
    tmpSector = SECTOR(fsDesc, parent) + ENTRY_SECTOR(index);
    if (readSector(fsDesc, tmpSector))
      return FS_READ_ERROR;
    ptr = fsDesc->buffer + ENTRY_OFFSET(index);
    memset(ptr, 0x20, 11);
    for (counter = 0; *newPath && (*newPath != '.') && (counter < 8); counter++)
      *((char *)(ptr + counter)) = *newPath++;
    if (!(attribute & FS_FLAG_DIR) && (*newPath == '.'))
    {
      for (counter = 8, name++; *newPath && (counter < 11); counter++)
        *((char *)(ptr + counter)) = *newPath++;
    }
    if (writeSector(fsDesc, tmpSector))
      return FS_WRITE_ERROR;

    return FS_OK;
  }
  else
  {
    item.attribute = attribute;
    if (createEntry(fsDesc, &item, newPath))
      return FS_WRITE_ERROR;
  }*/

  item.attribute = attribute;
  if (createEntry(fsDesc, &item, newPath))
    return FS_WRITE_ERROR;

  tmpSector = SECTOR(fsDesc, parent) + ENTRY_SECTOR(index);
  if (readSector(fsDesc, tmpSector))
    return FS_READ_ERROR;
  /* Set old entry as removed */
  *((uint8_t *)(fsDesc->buffer + ENTRY_OFFSET(index))) = 0xE5;
  if (writeSector(fsDesc, tmpSector))
    return FS_WRITE_ERROR;

  tmpSector = SECTOR(fsDesc, item.parent) + ENTRY_SECTOR(item.index);
  if (readSector(fsDesc, tmpSector))
    return FS_READ_ERROR;
  ptr = fsDesc->buffer + ENTRY_OFFSET(item.index);
  //TODO move cluster and size to createEntry?
  /* Write cluster value */
  *((uint16_t *)(ptr + 0x14)) = cluster >> 16; /* High word */
  *((uint16_t *)(ptr + 0x1A)) = cluster & 0xFFFF; /* Low word */
  /* Write file size */
  *((uint32_t *)(ptr + 0x1C)) = size;
  if (writeSector(fsDesc, tmpSector))
    return FS_WRITE_ERROR;

  return FS_OK;
}
#endif
