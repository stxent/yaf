/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <stdlib.h>
#include <string.h>
#include "fat32.h"
#include "fat32_defs.h"
/*----------------------------------------------------------------------------*/
//TODO
#define mutexLock(lock)
#define mutexUnlock(lock)
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif
/*----------------------------------------------------------------------------*/
/* Get size of array placed in structure */
#define ARRAY_SIZE(parent, array) (sizeof(((struct parent *)0)->array))
/*------------------Class descriptors-----------------------------------------*/
static const struct FsHandleClass fatHandleTable = {
    .size = sizeof(struct FatHandle),
    .init = fatHandleInit,
    .deinit = fatHandleDeinit,

    .follow = fatFollow
};

static const struct FsNodeClass fatNodeTable = {
    .size = sizeof(struct FatNode),
    .init = fatNodeInit,
    .deinit = fatNodeDeinit,

    .clone = fatClone,
    .free = fatFree,
    .get = fatGet,
    .link = fatLink,
    .make = fatMake,
    .open = fatOpen,
    .set = fatSet,
    .truncate = fatTruncate,
    .unlink = fatUnlink
};

static const struct FsEntryClass fatDirTable = {
    .size = sizeof(struct FatDir),
    .init = fatDirInit,
    .deinit = fatDirDeinit,

    .close = fatDirClose,
    .end = fatDirEnd,
    .fetch = fatDirFetch,
    .seek = fatDirSeek,

    .read = 0,
    .sync = 0,
    .tell = 0,
    .write = 0
};

static const struct FsEntryClass fatFileTable = {
    .size = sizeof(struct FatFile),
    .init = fatFileInit,
    .deinit = fatFileDeinit,

    .close = fatFileClose,
    .end = fatFileEnd,
    .read = fatFileRead,
    .seek = fatFileSeek,
    .sync = fatFileSync,
    .tell = fatFileTell,
    .write = fatFileWrite,

    .fetch = 0
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass *FatHandle = (void *)&fatHandleTable;
const struct FsNodeClass *FatNode = (void *)&fatNodeTable;
const struct FsEntryClass *FatDir = (void *)&fatDirTable;
const struct FsEntryClass *FatFile = (void *)&fatFileTable;
/*----------------------------------------------------------------------------*/
static enum result allocateBuffers(struct FatHandle *handle,
    const struct Fat32Config * const config)
{
  handle->buffer = malloc(SECTOR_SIZE);
  if (!handle->buffer)
    return E_MEMORY;

#ifdef FAT_LFN
  handle->nameBuffer = malloc(FILE_NAME_BUFFER * sizeof(char16_t));
  if (!handle->nameBuffer)
  {
    freeBuffers(handle, FREE_BUFFER);
    return E_MEMORY;
  }
#endif

#ifdef FAT_POOLS
  uint16_t number;
  enum result res;

  /* Allocate and fill node pool */
  number = config->nodes ? config->nodes : NODE_POOL_SIZE;
  res = allocatePool(&handle->nodePool, &handle->nodeData, FatNode, number);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_LFN);
    return res;
  }

  /* Allocate and fill directory entry pool */
  number = config->directories ? config->directories : DIR_POOL_SIZE;
  res = allocatePool(&handle->dirPool, &handle->dirData, FatDir, number);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_NODE_POOL);
    return res;
  }

  /* Allocate and fill file entry pool */
  number = config->files ? config->files : FILE_POOL_SIZE;
  res = allocatePool(&handle->filePool, &handle->fileData, FatFile, number);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_DIR_POOL);
    return res;
  }
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void *allocateNode(struct FatHandle *handle)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };
  struct FatNode *node;

#ifdef FAT_POOLS
  if (queueEmpty(&handle->nodePool))
    return 0;
  queuePop(&handle->nodePool, &node);
  FatNode->init(node, &config);
#else
  node = init(FatNode, &config);
#endif

  return node;
}
/*----------------------------------------------------------------------------*/
static void extractShortName(const struct DirEntryImage *entry, char *str)
{
  const char *src = entry->name;
  char *dest = str;
  uint8_t counter = 0;

  while (counter++ < sizeof(entry->name))
  {
    if (*src != ' ')
      *dest++ = *src;
    src++;
  }
  /* Add dot when entry is not directory or extension exists */
  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    *dest++ = '.';
    /* Copy entry extension */
    counter = 0;
    src = entry->extension;
    while (counter++ < sizeof(entry->extension))
    {
      if (*src != ' ')
        *dest++ = *src;
      ++src;
    }
  }
  *dest = '\0';
}
/*----------------------------------------------------------------------------*/
static void freeBuffers(struct FatHandle *handle, enum cleanup step)
{
  switch (step)
  {
    case FREE_ALL:
#ifdef FAT_POOLS
    case FREE_FILE_POOL:
      queueDeinit(&handle->filePool);
      free(handle->fileData);
    case FREE_DIR_POOL:
      queueDeinit(&handle->dirPool);
      free(handle->dirData);
    case FREE_NODE_POOL:
      queueDeinit(&handle->nodePool);
      free(handle->nodeData);
#endif
#ifdef FAT_LFN
    case FREE_LFN:
      free(handle->nameBuffer);
#endif
    case FREE_BUFFER:
      free(handle->buffer);
      break;
    default:
      break;
  }
}
/*----------------------------------------------------------------------------*/
/* Destination string buffer should be at least FS_NAME_LENGTH characters long */
static const char *getChunk(const char *src, char *dest)
{
  uint8_t counter = 0;

  if (!*src)
    return src;
  if (*src == '/')
  {
    *dest++ = '/';
    *dest = '\0';
    return src + 1;
  }
  while (*src && counter++ < FS_NAME_LENGTH - 1)
  {
    if (*src == '/')
    {
      ++src;
      break;
    }
    *dest++ = *src++;
  }
  *dest = '\0';
  return src;
}
/*----------------------------------------------------------------------------*/
static enum result getNextCluster(struct FatHandle *handle, uint32_t *cluster)
{
  enum result res = readSector(handle, handle->tableSector
      + (*cluster >> TE_COUNT), handle->buffer, 1);
  if (res != E_OK)
    return res;

  uint32_t nextCluster = *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster));
  if (clusterUsed(nextCluster))
  {
    *cluster = nextCluster;
    return E_OK;
  }
  return E_FAULT;
}
/*----------------------------------------------------------------------------*/
/* Fields node->index and node->cluster have to be initialized */
static enum result fetchEntry(struct FatNode *node)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct DirEntryImage *ptr;
  enum result res;

  /* Fields cluster, index and type are updated */
  while (1)
  {
    if (node->index >= nodeCount(handle))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      if ((res = getNextCluster(handle, &node->cluster)) != E_OK)
      {
        /* Set index to the last entry in last existing cluster */
        node->index = nodeCount(handle) - 1;
        return res;
      }
      node->index = 0;
    }

    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      return res;

    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(node->index));

    /* Check for the end of the directory */
    if (!ptr->name[0])
      return E_ENTRY;

    /* Volume entries are ignored */
    if (!(ptr->flags & FLAG_VOLUME && (ptr->flags & MASK_LFN) != MASK_LFN))
      break;
    ++node->index;
  }

  if (ptr->name[0] != E_FLAG_EMPTY && (ptr->flags & MASK_LFN) != MASK_LFN)
    node->type = ptr->flags & FLAG_DIR ? FS_TYPE_DIR : FS_TYPE_FILE;
  else
    node->type = FS_TYPE_NONE;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
//TODO Replace name with metadata structure, add metadata allocation from pool
/* Fields cluster and index should be initialized */
static enum result fetchNode(struct FatNode *node, char *entryName)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct DirEntryImage *ptr;
#ifdef FAT_LFN
  uint8_t checksum;
  uint8_t chunks = 0; /* LFN chunks required */
  uint8_t found = 0; /* LFN chunks found */
#endif
  enum result res;

  while ((res = fetchEntry(node)) == E_OK)
  {
    /* Reload directory sector */
    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      break;
    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(node->index));

#ifdef FAT_LFN
    if ((ptr->flags & MASK_LFN) == MASK_LFN && !(ptr->ordinal & LFN_DELETED))
    {
      if (ptr->ordinal & LFN_LAST)
      {
        found = 0;
        checksum = ptr->checksum;
        chunks = ptr->ordinal & ~LFN_LAST;
        node->nameIndex = node->index;
        node->nameCluster = node->cluster;
      }
      if (chunks)
        ++found;
    }
#endif

    if (node->type != FS_TYPE_NONE)
      break;
    ++node->index;
  }
  if (res != E_OK)
    return res;

  node->access = FS_ACCESS_READ;
  if (!(ptr->flags & FLAG_RO))
    node->access |= FS_ACCESS_WRITE;
  node->payload = ptr->clusterHigh << 16 | ptr->clusterLow;

#ifdef FAT_LFN
  if (!found || found != chunks || checksum != getChecksum(ptr->filename,
      sizeof(ptr->filename)))
  {
    /* Wrong checksum or chunk count does not match */
    node->nameIndex = node->index;
    node->nameCluster = node->cluster;
  }
#endif

  if (entryName)
  {
#ifdef FAT_LFN
    if (hasLongName(node))
    {
      if ((res = readLongName(node, entryName)) != E_OK)
        return res;
    }
    else
      extractShortName(ptr, entryName);
#else
    extractShortName(ptr, entryName);
#endif
  }
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static const char *followPath(struct FatNode *node, const char *path,
    const struct FatNode *root)
{
  char nodeName[FS_NAME_LENGTH], name[FS_NAME_LENGTH];

  path = getChunk(path, name);
  if (!strlen(name))
    return 0;

  node->index = 0;
  if (!root)
  {
    if (name[0] == '/')
    {
      node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
      node->cluster = 0;
      node->payload = ((struct FatHandle *)node->handle)->rootCluster;
      node->type = FS_TYPE_DIR;
      return path;
    }
    node->cluster = node->payload;
  }
  else
    node->cluster = root->payload;

  while (!fetchNode(node, nodeName))
  {
    if (!strcmp(name, nodeName))
      return path;
    ++node->index;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static enum result mount(struct FatHandle *handle)
{
  enum result res;

  /* Read first sector */
  if ((res = readSector(handle, 0, handle->buffer, 1)) != E_OK)
    return res;
  struct BootSectorImage *boot = (struct BootSectorImage *)handle->buffer;

  /* Check boot sector signature (55AA at 0x01FE) */
  if (boot->bootSignature != 0xAA55)
    return E_ERROR;

  /* Check sector size, fixed size of 2 ^ SECTOR_POW allowed */
  if (boot->bytesPerSector != SECTOR_SIZE)
    return E_ERROR;

  /* Calculate sectors per cluster count */
  uint16_t sizePow = boot->sectorsPerCluster;
  handle->clusterSize = 0;
  while (sizePow >>= 1)
    ++handle->clusterSize;

  handle->tableSector = boot->reservedSectors;
  handle->dataSector = handle->tableSector + boot->fatCopies * boot->fatSize;
  handle->rootCluster = boot->rootCluster;

  DEBUG_PRINT("Cluster size:   %u\n", (unsigned int)(1 << handle->clusterSize));
  DEBUG_PRINT("Table sector:   %u\n", (unsigned int)handle->tableSector);
  DEBUG_PRINT("Data sector:    %u\n", (unsigned int)handle->dataSector);

#ifdef FAT_WRITE
  handle->tableCount = boot->fatCopies;
  handle->tableSize = boot->fatSize;
  handle->clusterCount = ((boot->partitionSize
      - handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = boot->infoSector;

  DEBUG_PRINT("Info sector:    %u\n", (unsigned int)handle->infoSector);
  DEBUG_PRINT("Table copies:   %u\n", (unsigned int)handle->tableCount);
  DEBUG_PRINT("Table size:     %u\n", (unsigned int)handle->tableSize);
  DEBUG_PRINT("Cluster count:  %u\n", (unsigned int)handle->clusterCount);
  DEBUG_PRINT("Sectors count:  %u\n", (unsigned int)boot->partitionSize);

  /* Read information sector */
  if ((res = readSector(handle, handle->infoSector, handle->buffer, 1)) != E_OK)
    return res;
  struct InfoSectorImage *info = (struct InfoSectorImage *)handle->buffer;

  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
    return E_ERROR;
  handle->lastAllocated = info->lastAllocated;

  DEBUG_PRINT("Free clusters:  %u\n", (unsigned int)info->freeClusters);
#endif

  DEBUG_PRINT("Size of handle: %u\n", (unsigned int)sizeof(struct FatHandle));
  DEBUG_PRINT("Size of node:   %u\n", (unsigned int)sizeof(struct FatNode));
  DEBUG_PRINT("Size of dir:    %u\n", (unsigned int)sizeof(struct FatDir));
  DEBUG_PRINT("Size of file:   %u\n", (unsigned int)sizeof(struct FatFile));

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result readSector(struct FatHandle *handle, uint32_t sector,
    uint8_t *buffer, uint32_t count)
{
  if (handle->bufferedSector == sector)
    return E_OK;

  const uint64_t address = (uint64_t)sector << SECTOR_POW;
  const uint32_t length = count << SECTOR_POW;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifRead(handle->interface, buffer, length) != length)
    return E_INTERFACE;

  /* Save sector number when internal buffer is used */
  if (buffer == handle->buffer)
    handle->bufferedSector = sector;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Extract 13 unicode characters from long file name entry */
static void extractLongName(const struct DirEntryImage *entry, char16_t *str)
{
  memcpy(str, entry->longName0, sizeof(entry->longName0));
  str += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(str, entry->longName1, sizeof(entry->longName1));
  str += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(str, entry->longName2, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Calculate entry name checksum for long file name entries support */
static uint8_t getChecksum(const char *str, uint8_t length)
{
  uint8_t pos = 0, sum = 0;

  for (; pos < length; ++pos)
    sum = ((sum >> 1) | (sum << 7)) + *str++;
  return sum;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
static enum result readLongName(struct FatNode *node, char *entryName)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  uint32_t cluster = node->cluster; /* Preserve cluster and index values */
  uint16_t index = node->index;
  uint8_t chunks = 0;
  enum result res;

  node->cluster = node->nameCluster;
  node->index = node->nameIndex;

  while ((res = fetchEntry(node)) == E_OK)
  {
    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      break;
    struct DirEntryImage *ptr = (struct DirEntryImage *)(handle->buffer
        + E_OFFSET(node->index));

    if ((ptr->flags & MASK_LFN) == MASK_LFN)
    {
      ++chunks;
      extractLongName(ptr, handle->nameBuffer
          + ((ptr->ordinal & ~LFN_LAST) - 1) * LFN_ENTRY_LENGTH);
      ++node->index;
      continue;
    }
    else
      break; /* No more consecutive long file name entries */

    ++node->index;
  }
  if (res == E_OK)
  {
    if (!chunks)
      res = E_ENTRY;
    else
      uFromUtf16(entryName, handle->nameBuffer, FS_NAME_LENGTH);
  }

  /* Restore values changed during directory processing */
  node->cluster = cluster;
  node->index = index;

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_POOLS
static enum result allocatePool(struct Queue *queue, void *dataPtr,
    const void *descriptorPtr, uint16_t number)
{
  const struct EntityClass *descriptor = descriptorPtr;
  uint8_t *data;
  enum result res;

  if (!(data = malloc(descriptor->size * number)))
    return E_MEMORY;

  if ((res = queueInit(queue, sizeof(struct Entity *), number)) != E_OK)
  {
    free(data);
    return E_MEMORY;
  }

  *(void **)dataPtr = data;
  for (unsigned int index = 0; index < number; ++index)
  {
    /* Manual type initialization */
    ((struct Entity *)data)->type = descriptor;
    queuePush(queue, data);
    data += descriptor->size;
  }

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result allocateCluster(struct FatHandle *handle, uint32_t *cluster)
{
  uint32_t current = handle->lastAllocated + 1;
  enum result res;

  while (current != handle->lastAllocated)
  {
    if (current >= handle->clusterCount)
    {
      DEBUG_PRINT("Reached end of partition, continue from third cluster\n");
      current = 2;
    }

    res = readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1);
    if (res != E_OK)
      return res;

    /* Check whether the cluster is free */
    uint16_t offset = (current & ((1 << TE_COUNT) - 1)) << 2;
    if (clusterFree(*(uint32_t *)(handle->buffer + offset)))
    {
      /* Mark cluster as busy */
      *(uint32_t *)(handle->buffer + offset) = CLUSTER_EOC_VAL;

      /*
       * Save changes to allocation table when reference is not available
       * or cluster reference located in another sector.
       */
      if (!*cluster || (*cluster >> TE_COUNT != current >> TE_COUNT))
      {
        if ((res = updateTable(handle, current >> TE_COUNT)) != E_OK)
          return res;
      }

      /* Update reference cluster when refernce is available */
      if (*cluster)
      {
        res = readSector(handle, handle->tableSector + (*cluster >> TE_COUNT),
            handle->buffer, 1);
        if (res != E_OK)
          return res;

        *(uint32_t *)(handle->buffer + TE_OFFSET(*cluster)) = current;
        if ((res = updateTable(handle, *cluster >> TE_COUNT)) != E_OK)
          return res;
      }

      DEBUG_PRINT("Allocated new cluster: %u, source %u\n", current, *cluster);
      handle->lastAllocated = current;
      *cluster = current;

      /* Update information sector */
      if ((res = readSector(handle, handle->infoSector, handle->buffer, 1)))
        return res;

      struct InfoSectorImage *info = (struct InfoSectorImage *)handle->buffer;
      info->lastAllocated = current;
      --info->freeClusters;

      if ((res = writeSector(handle, handle->infoSector, handle->buffer, 1)))
        return res;

      return E_OK;
    }

    ++current;
  }

  DEBUG_PRINT("Allocation error, possibly partition is full\n");
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result clearCluster(struct FatHandle *handle, uint32_t cluster)
{
  uint32_t sector = getSector(handle, cluster + 1);

  memset(handle->buffer, 0, SECTOR_SIZE);
  do
  {
    enum result res = writeSector(handle, --sector, handle->buffer, 1);
    if (res != E_OK)
      return res;
  }
  while (sector & ((1 << handle->clusterSize) - 1));

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result createNode(struct FatNode *node, const struct FatNode *root,
    const struct FsMetadata *metadata)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct DirEntryImage *ptr;
  uint32_t sector;
  uint8_t chunks = 0;
  char shortName[sizeof(ptr->filename)];
  enum result res;
#ifdef FAT_LFN
  uint8_t checksum;
#endif

  /* TODO Check for duplicates */
  res = fillShortName(shortName, metadata->name);
  if ((node->type & FS_TYPE_DIR))
    memset(shortName + sizeof(ptr->name), ' ', sizeof(ptr->extension));

#ifdef FAT_LFN
  /* Check whether the file name is valid for use as short name */
  if (res != E_OK)
  {
    uint16_t length = uToUtf16(handle->nameBuffer, metadata->name,
        FILE_NAME_BUFFER) + 1;
    chunks = length / LFN_ENTRY_LENGTH;
    if (length > chunks * LFN_ENTRY_LENGTH) /* When fractional part exists */
      ++chunks;
    checksum = getChecksum(shortName, sizeof(ptr->filename));
  }
#endif

  /* Find suitable space within the directory */
  if ((res = findGap(node, root, chunks + 1)) != E_OK)
    return res;

#ifdef FAT_LFN
  /* Save start cluster and index values before filling the chain */
  node->nameCluster = node->cluster;
  node->nameIndex = node->index;
#endif

  uint8_t current = chunks;
  do
  {
    sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      break;
    ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(node->index));

    if (!current)
      break;

#ifdef FAT_LFN
    fillLongName(ptr, handle->nameBuffer + (current - 1) * LFN_ENTRY_LENGTH);
    fillLongNameEntry(ptr, current, chunks, checksum);

    --current;
    ++node->index;

    if (!(node->index & (E_POW - 1)))
    {
      /* Write back updated sector when switching sectors */
      if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
        break;
    }
#endif
  }
  while ((res = fetchEntry(node)) == E_OK);

  if (res != E_OK)
    return res;

  /* Fill uninitialized node fields */
  node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  node->payload = RESERVED_CLUSTER;
  node->type = metadata->type;
  memcpy(ptr->filename, shortName, sizeof(ptr->filename));

  fillDirEntry(ptr, node);

  res = writeSector(handle, sector, handle->buffer, 1);
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static void fillDirEntry(struct DirEntryImage *ptr, const struct FatNode *node)
{
  /* Clear unused fields */
  ptr->unused0 = 0;
  ptr->unused1 = 0;
  memset(ptr->unused2, 0, sizeof(ptr->unused2));

  ptr->flags = 0;
  if (node->type == FS_TYPE_DIR)
    ptr->flags |= FLAG_DIR;
  if (!(node->access & FS_ACCESS_WRITE))
    ptr->flags |= FLAG_RO;

  ptr->clusterHigh = node->payload >> 16;
  ptr->clusterLow = node->payload;
  ptr->size = 0;

#ifdef FAT_TIME
  //FIXME Rewrite
  /* Time and date of last modification */
  ptr->date = rtcGetDate();
  ptr->time = rtcGetTime();
#else
  ptr->date = 0;
  ptr->time = 0;
#endif
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Returns success when node is a valid short name */
static enum result fillShortName(char *shortName, const char *name)
{
  const uint8_t extLength = ARRAY_SIZE(DirEntryImage, extension);
  const uint8_t nameLength = ARRAY_SIZE(DirEntryImage, name);
  const char *dot;
  enum result res = E_OK;

  memset(shortName, ' ', extLength + nameLength);

  /* Find start position of the extension */
  uint16_t length = strlen(name);
  for (dot = name + length - 1; dot >= name && *dot != '.'; --dot);

  if (dot < name)
  {
    /* Dot not found */
    if (length > nameLength)
      res = E_ERROR;
    dot = 0;
  }
  else
  {
    /* Check whether file name and extension have adequate length */
    uint8_t position = dot - name;
    if (position > nameLength || length - position - 1 > extLength)
      res = E_ERROR;
  }

  uint8_t pos = 0;
  for (char symbol = *name; symbol; symbol = *name)
  {
    if (dot && name == dot)
    {
      pos = nameLength;
      ++name;
      continue;
    }
    ++name;

    char converted = processCharacter(symbol);
    if (converted != symbol)
      res = E_ERROR;
    if (!converted)
      continue;
    shortName[pos++] = converted;

    if (pos == nameLength)
    {
      if (dot) /* Check whether extension exists */
      {
        name = dot + 1;
        continue;
      }
      else
        break;
    }
    if (pos == extLength + nameLength)
      break;
  }

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Allocate single node or node chain inside root node chain. */
static enum result findGap(struct FatNode *node, const struct FatNode *root,
    uint8_t chainLength)
{
  struct FatHandle *handle = (struct FatHandle *)root->handle;
  uint32_t parent = root->payload;
  uint16_t index = 0;
  uint8_t chunks = 0;
  enum result res;

  node->cluster = root->payload;
  node->index = 0;

  while (chunks != chainLength)
  {
    bool allocateParent = false;
    switch ((res = fetchEntry(node)))
    {
      case E_ENTRY:
        index = node->index;
        parent = node->cluster;
        allocateParent = false;
        break;
      case E_FAULT:
        index = 0;
        allocateParent = true;
        break;
      default:
        if (res != E_OK)
          return res;
    }
    if (res == E_FAULT || res == E_ENTRY)
    {
      int16_t chunksLeft = (int16_t)(chainLength - chunks)
          - (int16_t)(nodeCount(handle) - node->index);
      while (chunksLeft > 0)
      {
        if ((res = allocateCluster(handle, &node->cluster)) != E_OK)
          return res;
        if (allocateParent)
        {
          parent = node->cluster;
          allocateParent = false;
        }
        chunksLeft -= (int16_t)nodeCount(handle);
      }
      break;
    }

    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      return res;
    struct DirEntryImage *ptr = (struct DirEntryImage *)(handle->buffer
        + E_OFFSET(node->index));

    /* Empty node, deleted node or deleted long file name node */
    if (((ptr->flags & MASK_LFN) == MASK_LFN && ptr->ordinal & LFN_DELETED)
        || !ptr->name[0] || ptr->name[0] == E_FLAG_EMPTY)
    {
      if (!chunks) /* Found first free node */
      {
        index = node->index;
        parent = node->cluster;
      }
      ++chunks;
    }
    else
      chunks = 0;

    ++node->index;
  }

  node->cluster = parent;
  node->index = index;
  node->type = FS_TYPE_NONE;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result freeChain(struct FatHandle *handle, uint32_t cluster)
{
  uint32_t current = cluster, next, released = 0;
  enum result res;

  if (!current)
    return E_OK; /* Already empty */

  while (clusterUsed(current))
  {
    /* Get FAT sector with next cluster value */
    res = readSector(handle, handle->tableSector + (current >> TE_COUNT),
        handle->buffer, 1);
    if (res != E_OK)
      return res;

    next = *(uint32_t *)(handle->buffer + TE_OFFSET(current));
    *(uint32_t *)(handle->buffer + TE_OFFSET(current)) = 0;

    /* Update table when switching table sectors */
    if (current >> TE_COUNT != next >> TE_COUNT)
    {
      if ((res = updateTable(handle, current >> TE_COUNT)) != E_OK)
        return res;
    }

    ++released;
    DEBUG_PRINT("Cleared cluster: %u\n", current);
    current = next;
  }

  /* Update information sector */
  res = readSector(handle, handle->infoSector, handle->buffer, 1);
  if (res != E_OK)
    return res;

  struct InfoSectorImage *info = (struct InfoSectorImage *)handle->buffer;
  /* Set free clusters count */
  info->freeClusters += released;

  res = writeSector(handle, handle->infoSector, handle->buffer, 1);
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result markFree(struct FatNode *node)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  uint32_t cluster = node->cluster;
  uint32_t lastSector;
  uint16_t index = node->index;
  bool lastEntry = false;
  enum result res;

  lastSector = getSector(handle, node->cluster) + E_SECTOR(node->index);
#ifdef FAT_LFN
  node->index = node->nameIndex;
  node->cluster = node->nameCluster;
#endif

  while (!lastEntry && (res = fetchEntry(node)) == E_OK)
  {
    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    lastEntry = sector == lastSector && node->index == index;
    if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
      break;
    struct DirEntryImage *ptr = (struct DirEntryImage *)(handle->buffer
        + E_OFFSET(node->index));

    /* Mark entry as empty by changing first byte of the name */
    ptr->name[0] = E_FLAG_EMPTY;

    if (lastEntry || !(node->index & (E_POW - 1)))
    {
      /* Write back updated sector when switching sectors or last entry freed */
      if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
        break;
    }

    ++node->index;
  }

  /* Restore node values changed during entry fetching */
  node->index = index;
  node->cluster = cluster;

  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Returns zero when character is not allowed or processed character code */
static char processCharacter(char value)
{
  /* All slashes are already removed while file path being calculated */
  /* Drop all multibyte UTF-8 code points */
  if (value & 0x80)
    return 0;
  /* Replace spaces with underscores */
  if (value == ' ')
    return '_';
  /* Convert lower case characters to upper case */
  if (value >= 'a' && value <= 'z')
    return value - 32;
  /* Check specific FAT32 ranges */
  if (value > 0x20 && value != 0x22 && value != 0x7C
      && !(value >= 0x2A && value <= 0x2F)
      && !(value >= 0x3A && value <= 0x3F)
      && !(value >= 0x5B && value <= 0x5D))
  {
    return value;
  }
  return 0;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result setupDirCluster(const struct FatNode *node)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  enum result res;

  if ((res = clearCluster(handle, node->payload)) != E_OK)
    return res;

  struct DirEntryImage *ptr;
  /* Current directory entry . */
  ptr = (struct DirEntryImage *)handle->buffer;
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  ptr->name[0] = '.';
  fillDirEntry(ptr, node);

  /* Parent directory entry .. */
  ++ptr;
  memset(ptr->filename, ' ', sizeof(ptr->filename));
  ptr->name[0] = ptr->name[1] = '.';
  fillDirEntry(ptr, node);
  if (node->cluster != handle->rootCluster)
  {
    ptr->clusterHigh = (uint16_t)(node->cluster >> 16);
    ptr->clusterLow = (uint16_t)node->cluster;
  }
  else
    ptr->clusterLow = ptr->clusterHigh = 0;

  res = writeSector(handle, getSector(handle, node->payload),
      handle->buffer, 1);
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Copy current sector into FAT sectors located at offset */
static enum result updateTable(struct FatHandle *handle, uint32_t offset)
{
  for (uint8_t fat = 0; fat < handle->tableCount; ++fat)
  {
    enum result res = writeSector(handle, offset + handle->tableSector
        + handle->tableSize * fat, handle->buffer, 1);
    if (res != E_OK)
      return res;
  }
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result writeSector(struct FatHandle *handle,
    uint32_t sector, const uint8_t *buffer, uint32_t count)
{
  const uint64_t address = (uint64_t)sector << SECTOR_POW;
  const uint32_t length = count << SECTOR_POW;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifWrite(handle->interface, buffer, length) != length)
    return E_INTERFACE;

  /* Update sector number when internal buffer is used */
  if (buffer == handle->buffer)
    handle->bufferedSector = sector;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(FAT_LFN)
/* Save 13 unicode characters to long file name entry */
static void fillLongName(struct DirEntryImage *entry, char16_t *str)
{
  //FIXME Add checking for the end of the string
  memcpy(entry->longName0, str, sizeof(entry->longName0));
  str += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(entry->longName1, str, sizeof(entry->longName1));
  str += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(entry->longName2, str, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(FAT_LFN)
static void fillLongNameEntry(struct DirEntryImage *ptr, uint8_t current,
    uint8_t total, uint8_t checksum)
{
  /* Clear unused fields */
  ptr->unused0 = 0;
  ptr->unused3 = 0; /* Zero value required */

  ptr->flags = MASK_LFN;
  ptr->checksum = checksum;
  ptr->ordinal = current;
  if (current == total)
    ptr->ordinal |= LFN_LAST;
}
#endif
/*------------------Filesystem handle functions-------------------------------*/
static enum result fatHandleInit(void *object, const void *configPtr)
{
  const struct Fat32Config * const config = configPtr;
  struct FatHandle *handle = object;
  enum result res;

  if ((res = allocateBuffers(handle, config)) != E_OK)
    return res;

  handle->bufferedSector = RESERVED_SECTOR;
  handle->interface = config->interface;

  if ((res = mount(handle)) != E_OK)
    return res;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatHandleDeinit(void *object)
{
  struct FatHandle *handle = object;

  freeBuffers(handle, FREE_ALL);
}
/*----------------------------------------------------------------------------*/
static void *fatFollow(void *object, const char *path, const void *root)
{
  struct FatNode *node = allocateNode(object);

  if (!node)
    return 0;

  while (path && *path)
    path = followPath(node, path, root);

  if (!path)
  {
    deinit(node);
    return 0;
  }
  else
    return node;
}
/*------------------Node functions--------------------------------------------*/
static enum result fatNodeInit(void *object, const void *configPtr)
{
  const struct FatNodeConfig * const config = configPtr;
  struct FatNode *node = object;

  node->handle = config->handle;
  DEBUG_PRINT("Node allocated, address %08lX\n", (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatNodeDeinit(void *object __attribute__((unused)))
{
  DEBUG_PRINT("Node freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
static void *fatClone(void *object)
{
  struct FatNode *node = object;

  return allocateNode((struct FatHandle *)node->handle);
}
/*----------------------------------------------------------------------------*/
static void fatFree(void *object)
{
#ifdef FAT_POOLS
  struct FatNode *node = object;

  FatNode->deinit(object);
  queuePush(&((struct FatHandle *)node->handle)->nodePool, object);
#else
  deinit(object);
#endif
}
/*----------------------------------------------------------------------------*/
static enum result fatGet(void *object, struct FsMetadata *metadata)
{
  const struct DirEntryImage *ptr;
  struct FatNode *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  enum result res;

  const uint32_t sector = getSector(handle, node->cluster)
      + E_SECTOR(node->index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  ptr = (struct DirEntryImage *)(handle->buffer + E_OFFSET(node->index));

  metadata->access = FS_ACCESS_READ;
  if (!(ptr->flags & FLAG_RO))
    metadata->access |= FS_ACCESS_WRITE;
  metadata->size = ptr->size;
  metadata->type = node->type;

#ifdef FAT_LFN
  if (hasLongName(node))
    readLongName(node, metadata->name);
  else
    extractShortName(ptr, metadata->name);
#else
  extractShortName(ptr, metadata->name);
#endif

#ifdef FAT_TIME
  struct Time time;
  time.sec = ptr->time & 0x1F;
  time.min = (ptr->time >> 5) & 0x3F;
  time.hour = (ptr->time >> 11) & 0x1F;
  time.day = ptr->date & 0x1F;
  time.mon = (ptr->date >> 5) & 0x0F;
  time.year = ((ptr->date >> 9) & 0x7F) + 1980;
  metadata->time = unixTime(&time);
#else
  metadata->time = 0;
#endif

#ifdef DEBUG
  struct FatMetadata *debugData = (struct FatMetadata *)metadata;
  debugData->cluster = node->payload;
  debugData->pcluster = node->cluster;
  debugData->pindex = node->index;
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatLink(void *object, const struct FsMetadata *metadata,
    const void *target, void *result)
{
  return E_ERROR; //TODO
}
#else
static enum result fatMake(void *object __attribute__((unused)),
    const struct FsMetadata *metadata __attribute__((unused)),
    const void *target __attribute__((unused)),
    void *result __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatMake(void *object, const struct FsMetadata *metadata,
    void *result)
{
  struct FatNode *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  enum result res;

  struct FatNode *allocatedNode;
  if (!(allocatedNode = fatAllocate(handle)))
    return E_MEMORY;

  if (metadata->type == FS_TYPE_DIR)
  {
    res = allocateCluster(handle, &allocatedNode->payload);
    if (res != E_OK)
      goto allocate_error;
  }

  res = createNode(allocatedNode, node, metadata);
  if (res != E_OK)
    goto create_error;

  if (metadata->type == FS_TYPE_DIR)
  {
    res = setupDirCluster(allocatedNode);
    if (res != E_OK)
      goto setup_error;
  }

  if (result)
    memcpy(result, allocatedNode, sizeof(struct FatNode));

  fatFree(allocatedNode);
  return E_OK;

setup_error:
  fatUnlink(allocatedNode);
create_error:
  freeChain(handle, allocatedNode->payload);
allocate_error:
  fatFree(allocatedNode);
  return res;
}
#else
static enum result fatMake(void *object __attribute__((unused)),
    const struct FsMetadata *metadata __attribute__((unused)),
    void *result __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
static void *fatOpen(void *object, access_t access)
{
  struct FatNode *node = object;

  if ((node->access & access) != access)
    return 0;

  switch (node->type)
  {
    case FS_TYPE_DIR:
    {
      struct FatDirConfig config = {
          .node = object
      };
      struct FatDir *dir;

#ifdef FAT_POOLS
      struct FatHandle *handle = (struct FatHandle *)node->handle;
      if (queueEmpty(&handle->dirPool))
        return 0;
      queuePop(&handle->dirPool, &dir);
      FatDir->init(dir, &config);
#else
      if (!(dir = init(FatDir, &config)))
        return 0;
#endif

      return dir;
    }
    case FS_TYPE_FILE:
    {
      struct FatFileConfig config = {
          .node = object
      };
      struct FatFile *file;

#ifdef FAT_POOLS
      struct FatHandle *handle = (struct FatHandle *)node->handle;
      if (queueEmpty(&handle->filePool))
        return 0;
      queuePop(&handle->filePool, &file);
      FatFile->init(file, &config);
#else
      if (!(file = init(FatFile, &config)))
        return 0;
#endif

      file->access = access; /* Reduce set of access rights */
      return file;
    }
    default:
      return 0;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatSet(void *object, const struct FsMetadata *metadata)
{
  struct FatNode *node = object;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  //TODO Implement
  return E_ERROR;
}
#else
static enum result fatSet(void *object __attribute__((unused)),
    const struct FsMetadata *metadata __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatTruncate(void *object)
{
  struct FatNode *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  enum result res;

  if (node->type != FS_TYPE_FILE)
    return E_VALUE;
  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  /* Mark file table clusters as free */
  if ((res = freeChain(handle, node->payload)) != E_OK)
    return res;

  node->payload = 0;
  return E_OK;
}
#else
static enum result fatTruncate(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatUnlink(void *object)
{
  struct FatNode *node = object;
  enum result res;

  if (node->type != FS_TYPE_DIR && node->type != FS_TYPE_FILE)
    return E_VALUE;
  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (node->type == FS_TYPE_DIR)
  {
    /* Preserve values */
    uint32_t cluster = node->cluster;
    uint16_t index = node->index;

    /* Check if directory not empty */
    node->cluster = node->payload;
    node->index = 2; /* Exclude . and .. */

    res = fetchNode(node, 0);
    if (res == E_OK)
      return E_VALUE;
    if (res != E_ENTRY && res != E_FAULT)
      return res;

    /* Restore values changed by node fetching function */
    node->cluster = cluster;
    node->index = index;
  }

  if ((res = markFree(node)) != E_OK)
    return res;

  return E_OK;
}
#else
static enum result fatUnlink(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*------------------Directory functions---------------------------------------*/
static enum result fatDirInit(void *object, const void *configPtr)
{
  const struct FatDirConfig * const config = configPtr;
  const struct FatNode *node = (struct FatNode *)config->node;
  struct FatDir *dir = object;

  if (node->type != FS_TYPE_DIR)
    return E_VALUE;

  dir->handle = node->handle;
  dir->payload = node->payload;
  dir->currentCluster = node->payload;
  dir->currentIndex = 0;
  DEBUG_PRINT("Dir allocated, address %08lX\n", (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatDirDeinit(void *object __attribute__((unused)))
{
  DEBUG_PRINT("Dir freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
static enum result fatDirClose(void *object)
{
#ifdef FAT_POOLS
  struct FatDir *dir = object;

  FatDir->deinit(object);
  queuePush(&((struct FatHandle *)dir->handle)->dirPool, object);
#else
  deinit(object);
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static bool fatDirEnd(void *object)
{
  struct FatDir *dir = object;

  return dir->currentCluster == RESERVED_CLUSTER;
}
/*----------------------------------------------------------------------------*/
static enum result fatDirFetch(void *object, void *nodePtr)
{
  struct FatNode *node = nodePtr;
  struct FatDir *dir = object;
  enum result res;

  node->cluster = dir->currentCluster;
  node->index = dir->currentIndex;

  if ((res = fetchNode(node, 0)) != E_OK)
  {
    if (res == E_ENTRY)
    {
      /* Reached the end of the directory */
      dir->currentCluster = RESERVED_CLUSTER;
      dir->currentIndex = 0;
    }
    return res;
  }

  /* Make current position pointing to next entry */
  dir->currentCluster = node->cluster;
  dir->currentIndex = node->index + 1;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result fatDirSeek(void *object, uint64_t offset,
    enum fsSeekOrigin origin)
{
  struct FatDir *dir = object;

  /* TODO Implement other seek types */
  if (offset || origin != FS_SEEK_SET)
    return E_VALUE;

  dir->currentCluster = dir->payload;
  dir->currentIndex = 0;

  return E_OK;
}
/*------------------File functions--------------------------------------------*/
static enum result fatFileInit(void *object, const void *configPtr)
{
  const struct FatFileConfig * const config = configPtr;
  const struct FatNode *node = (struct FatNode *)config->node;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct FatFile *file = object;
  enum result res;

  if (node->type != FS_TYPE_FILE)
    return E_VALUE;

  /* TODO Avoid reading */
  const uint32_t sector = getSector(handle, node->cluster)
      + E_SECTOR(node->index);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;
  struct DirEntryImage *ptr = (struct DirEntryImage *)(handle->buffer
      + E_OFFSET(node->index));

  file->access = node->access;
  file->handle = node->handle;
  file->position = 0;
  file->size = ptr->size;
  file->payload = node->payload;
  file->currentCluster = node->payload;
#ifdef FAT_WRITE
  file->parentCluster = node->cluster;
  file->parentIndex = node->index;
#endif
  DEBUG_PRINT("File allocated, address %08lX\n", (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatFileDeinit(void *object __attribute__((unused)))
{
  DEBUG_PRINT("File freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
static enum result fatFileClose(void *object)
{
#ifdef FAT_WRITE
  if (((struct FatFile *)object)->access & FS_ACCESS_WRITE)
    fatFileSync(object);
#endif

#ifdef FAT_POOLS
  struct FatFile *file = object;

  FatFile->deinit(object);
  queuePush(&((struct FatHandle *)file->handle)->filePool, object);
#else
  deinit(object);
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static bool fatFileEnd(void *object)
{
  struct FatFile *file = object;

  return file->position >= file->size;
}
/*----------------------------------------------------------------------------*/
static uint32_t fatFileRead(void *object, void *buffer, uint32_t length)
{
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;
  uint32_t read = 0;
  uint16_t chunk;
  uint8_t current = 0;

  if (!(file->access & FS_ACCESS_READ))
    return 0;

  if (length > file->size - file->position)
    length = file->size - file->position;

  if (file->position)
  {
    current = sectorInCluster(handle, file->position);
    if (!current && !(file->position & (SECTOR_SIZE - 1)))
      current = 1 << handle->clusterSize;
  }

  while (length)
  {
    if (current >= (1 << handle->clusterSize))
    {
      if (getNextCluster(handle, &file->currentCluster) != E_OK)
        return 0; /* Sector read error or end-of-file */
      current = 0;
    }

    /* Position within the sector */
    uint16_t offset = (file->position + read) & (SECTOR_SIZE - 1);
    if (offset || length < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = length < chunk ? length : chunk;

      if (readSector(handle, getSector(handle, file->currentCluster) + current,
          handle->buffer, 1) != E_OK)
      {
        return 0;
      }
      memcpy((uint8_t *)buffer + read, handle->buffer + offset, chunk);

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_POW);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Read data to the buffer directly without additional copying */
      if (readSector(handle, getSector(handle, file->currentCluster)
          + current, (uint8_t *)buffer + read, chunk >> SECTOR_POW) != E_OK)
      {
        return 0;
      }

      current += chunk >> SECTOR_POW;
    }

    read += chunk;
    length -= chunk;
  }

  file->position += read;
  return read;
}
/*----------------------------------------------------------------------------*/
static enum result fatFileSeek(void *object, uint64_t offset,
    enum fsSeekOrigin origin)
{
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;

  switch (origin)
  {
    case FS_SEEK_CUR:
      offset = file->position + offset;
      break;
    case FS_SEEK_END:
      offset = file->size - offset;
      break;
    case FS_SEEK_SET:
      break;
  }
  if (offset > file->size)
    return E_ERROR;

  uint32_t clusterCount = offset;
  uint32_t current;
  if (offset > file->position)
  {
    current = file->currentCluster;
    clusterCount -= file->position;
  }
  else
    current = file->payload;
  clusterCount >>= handle->clusterSize + SECTOR_POW;

  while (--clusterCount)
  {
    enum result res = getNextCluster(handle, &current);
    if (res != E_OK)
      return res;
  }
  file->position = offset;
  file->currentCluster = current;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatFileSync(void *object)
{
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;
  uint32_t sector;
  enum result res;

  if (!(file->access & FS_ACCESS_WRITE))
    return E_ERROR;

  sector = getSector(handle, file->parentCluster) + E_SECTOR(file->parentIndex);
  if ((res = readSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;

  /* Pointer to entry position in sector */
  struct DirEntryImage *ptr = (struct DirEntryImage *)(handle->buffer
      + E_OFFSET(file->parentIndex));
  /* Update first cluster when writing to empty file or truncating file */
  ptr->clusterHigh = (uint16_t)(file->payload >> 16);
  ptr->clusterLow = (uint16_t)file->payload;
  /* Update file size */
  ptr->size = file->size;

#ifdef FAT_TIME
  /* Update last modified date */
  //FIXME rewrite
  ptr->time = rtcGetTime();
  ptr->date = rtcGetDate();
#endif

  if ((res = writeSector(handle, sector, handle->buffer, 1)) != E_OK)
    return res;

  return E_OK;
}
#else
static enum result fatFileSync(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
static uint64_t fatFileTell(void *object)
{
  return (uint64_t)((struct FatFile *)object)->position;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static uint32_t fatFileWrite(void *object, const void *buffer, uint32_t length)
{
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;
  uint32_t written = 0;
  uint16_t chunk;
  uint8_t current = 0; /* Current sector of the data cluster */

  if (!(file->access & FS_ACCESS_WRITE))
    return 0;

  if (file->payload == RESERVED_CLUSTER)
  {
    if (allocateCluster(handle, &file->payload) != E_OK)
      return 0;
    file->currentCluster = file->payload;
  }

  /* Checking file size limit (4 GiB - 1) */
  if (length > FILE_SIZE_MAX - file->size)
    length = FILE_SIZE_MAX - file->size;

  if (file->position)
  {
    current = sectorInCluster(handle, file->position);
    if (!current && !(file->position & (SECTOR_SIZE - 1)))
      current = 1 << handle->clusterSize;
  }

  while (length)
  {
    if (current >= (1 << handle->clusterSize))
    {
      enum result res;

      /* Allocate new cluster when next cluster does not exist */
      res = getNextCluster(handle, &file->currentCluster);
      if ((res != E_FAULT && res != E_OK) || (res == E_FAULT
          && allocateCluster(handle, &file->currentCluster) != E_OK))
      {
        return 0;
      }
      current = 0;
    }

    /* Position within the sector */
    uint16_t offset = (file->position + written) & (SECTOR_SIZE - 1);
    if (offset || length < SECTOR_SIZE) /* Position within sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (length < chunk) ? length : chunk;

      uint32_t sector = getSector(handle, file->currentCluster) + current;
      if (readSector(handle, sector, handle->buffer, 1) != E_OK)
        return 0;

      memcpy(handle->buffer + offset, (uint8_t *)buffer + written, chunk);
      if (writeSector(handle, sector, handle->buffer, 1) != E_OK)
        return 0;

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      /* Length of remaining cluster space */
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_POW);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Write data from the buffer directly without additional copying */
      if (writeSector(handle, getSector(handle, file->currentCluster) + current,
          (uint8_t *)buffer + written, chunk >> SECTOR_POW) != E_OK)
      {
        return 0;
      }

      current += chunk >> SECTOR_POW;
    }

    written += chunk;
    length -= chunk;
  }

  file->size += written;
  file->position = file->size;
  return written;
}
#else
static uint32_t fatFileWrite(void *entry __attribute__((unused)),
    const uint8_t *buffer __attribute__((unused)),
    uint32_t length __attribute__((unused)))
{
  return 0;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(FAT_WRITE) && defined(DEBUG)
uint32_t countFree(void *object)
{
  struct FatHandle *handle = object;
  uint32_t *count = malloc(sizeof(uint32_t) * handle->tableCount);
  uint32_t empty = 0;

  if (!count)
    return 0; /* Memory allocation problem */

  for (uint8_t fat = 0; fat < handle->tableCount; ++fat)
  {
    count[fat] = 0;
    for (uint32_t current = 0; current < handle->clusterCount; ++current)
    {
      if (readSector(handle, handle->tableSector
          + (current >> TE_COUNT), handle->buffer, 1) != E_OK)
      {
        return empty;
      }
      uint16_t offset = (current & ((1 << TE_COUNT) - 1)) << 2;
      if (clusterFree(*(uint32_t *)(handle->buffer + offset)))
        ++count[fat];
    }
  }
  for (uint8_t i = 0; i < handle->tableCount; ++i)
    for (uint8_t j = 0; j < handle->tableCount; ++j)
      if (i != j && count[i] != count[j])
      {
        DEBUG_PRINT("FAT records count differs: %u and %u\n", count[i],
            count[j]);
      }
  empty = count[0];
  free(count);
  return empty;
}
#endif
