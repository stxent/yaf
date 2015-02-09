/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <stdlib.h>
#include <string.h>
#include <bits.h>
#include <memory.h>
#include <libyaf/fat32.h>
#include <libyaf/fat32_defs.h>
#include <libyaf/fat32_inlines.h>
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_DEBUG
#include <stdio.h>
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif
/*------------------Class descriptors-----------------------------------------*/
static const struct FsHandleClass fatHandleTable = {
    .size = sizeof(struct FatHandle),
    .init = fatHandleInit,
    .deinit = fatHandleDeinit,

    .follow = fatFollow,
    .sync = fatSync
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
    .unlink = fatUnlink,

    /* Stubs */
    .mount = fatMount,
    .unmount = fatUnmount
};

static const struct FsEntryClass fatDirTable = {
    .size = sizeof(struct FatDir),
    .init = fatDirInit,
    .deinit = fatDirDeinit,

    .close = fatDirClose,
    .end = fatDirEnd,
    .fetch = fatDirFetch,
    .seek = fatDirSeek,
    .tell = fatDirTell,

    /* Stubs */
    .read = fatDirRead,
    .write = fatDirWrite
};

static const struct FsEntryClass fatFileTable = {
    .size = sizeof(struct FatFile),
    .init = fatFileInit,
    .deinit = fatFileDeinit,

    .close = fatFileClose,
    .end = fatFileEnd,
    .read = fatFileRead,
    .seek = fatFileSeek,
    .tell = fatFileTell,
    .write = fatFileWrite,

    /* Stubs */
    .fetch = fatFileFetch
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass * const FatHandle = &fatHandleTable;
const struct FsNodeClass * const FatNode = &fatNodeTable;
const struct FsEntryClass * const FatDir = &fatDirTable;
const struct FsEntryClass * const FatFile = &fatFileTable;
/*----------------------------------------------------------------------------*/
static enum result allocatePool(struct Pool *pool, unsigned int capacity,
    unsigned int width, const void *initializer)
{
  uint8_t *data = malloc(width * capacity);

  if (!data)
    return E_MEMORY;

  const enum result res = queueInit(&pool->queue, sizeof(struct Entity *),
      capacity);

  if (res != E_OK)
  {
    free(data);
    return E_MEMORY;
  }

  pool->data = data;
  for (unsigned int index = 0; index < capacity; ++index)
  {
    if (initializer)
      ((struct Entity *)data)->descriptor = initializer;

    queuePush(&pool->queue, data);
    data += width;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void freePool(struct Pool *pool)
{
  queueDeinit(&pool->queue);
  free(pool->data);
}
/*----------------------------------------------------------------------------*/
static enum result allocateBuffers(struct FatHandle *handle,
    const struct Fat32Config * const config)
{
  uint16_t count;
  enum result res;

#if !defined(CONFIG_FAT_THREADS) && !defined(CONFIG_FAT_POOLS)
  /* Suppress warning */
  (void)config;
#endif

#ifdef CONFIG_FAT_THREADS
  count = config->threads ? config->threads : DEFAULT_THREAD_COUNT;

  /* Create consistency and memory locks */
  if ((res = mutexInit(&handle->consistencyMutex)) != E_OK)
    return res;

  if ((res = mutexInit(&handle->memoryMutex)) != E_OK)
  {
    mutexDeinit(&handle->consistencyMutex);
    return res;
  }

  /* Allocate context pool */
  res = allocatePool(&handle->contextPool, count,
      sizeof(struct CommandContext), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_LOCKS);
    return res;
  }

  /* Custom initialization of default sector values */
  struct CommandContext *contextBase = handle->contextPool.data;

  for (unsigned int index = 0; index < count; ++index, ++contextBase)
    contextBase->sector = RESERVED_SECTOR;

  DEBUG_PRINT("Context pool:   %u\n", (unsigned int)(count
      * (sizeof(struct CommandContext *) + sizeof(struct CommandContext))));
#else
  count = DEFAULT_THREAD_COUNT;

  /* Allocate single context buffer */
  handle->context = malloc(sizeof(struct CommandContext));
  if (!handle->context)
    return E_MEMORY;
  handle->context->sector = RESERVED_SECTOR;

  DEBUG_PRINT("Context pool:   %u\n",
      (unsigned int)sizeof(struct CommandContext));
#endif

  /* Allocate metadata pool */
  res = allocatePool(&handle->metadataPool, count * 2,
      sizeof(struct FsMetadata), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_CONTEXT_POOL);
    return res;
  }

  DEBUG_PRINT("Metadata pool:  %u\n", (unsigned int)(count * 2
      * (sizeof(struct FsMetadata *) + sizeof(struct FsMetadata))));

#ifdef CONFIG_FAT_WRITE
  res = listInit(&handle->openedFiles, sizeof(struct FatFile *));
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_METADATA_POOL);
    return res;
  }
#endif

#ifdef CONFIG_FAT_POOLS
  /* Allocate and fill node pool */
  count = config->nodes ? config->nodes : DEFAULT_NODE_COUNT;
  res = allocatePool(&handle->nodePool, count, sizeof(struct FatNode),
      FatNode);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_FILE_LIST);
    return res;
  }

  DEBUG_PRINT("Node pool:      %u\n", (unsigned int)(count
      * (sizeof(struct FatNode *) + sizeof(struct FatNode))));

  /* Allocate and fill directory entry pool */
  count = config->directories ? config->directories : DEFAULT_DIR_COUNT;
  res = allocatePool(&handle->dirPool, count, sizeof(struct FatDir), FatDir);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_NODE_POOL);
    return res;
  }

  DEBUG_PRINT("Directory pool: %u\n", (unsigned int)(count
      * (sizeof(struct FatDir *) + sizeof(struct FatDir))));

  /* Allocate and fill file entry pool */
  count = config->files ? config->files : DEFAULT_FILE_COUNT;
  res = allocatePool(&handle->filePool, count, sizeof(struct FatFile),
      FatFile);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_DIR_POOL);
    return res;
  }

  DEBUG_PRINT("File pool:      %u\n", (unsigned int)(count
      * (sizeof(struct FatFile *) + sizeof(struct FatFile))));
#endif /* CONFIG_FAT_POOLS */

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static struct CommandContext *allocateContext(struct FatHandle *handle)
{
#ifdef CONFIG_FAT_THREADS
  struct CommandContext *context = 0;

  mutexLock(&handle->memoryMutex);
  if (!queueEmpty(&handle->contextPool.queue))
    queuePop(&handle->contextPool.queue, &context);
  mutexUnlock(&handle->memoryMutex);

  return context;
#else
  return handle->context;
#endif
}
/*----------------------------------------------------------------------------*/
static struct FsMetadata *allocateMetadata(struct FatHandle *handle)
{
  struct FsMetadata *metadata = 0;

  lockPools(handle);
  if (!queueEmpty(&handle->metadataPool.queue))
    queuePop(&handle->metadataPool.queue, &metadata);
  unlockPools(handle);

  return metadata;
}
/*----------------------------------------------------------------------------*/
static void *allocateNode(struct FatHandle *handle)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };
  struct FatNode *node = 0;

  lockPools(handle);
#ifdef CONFIG_FAT_POOLS
  if (!queueEmpty(&handle->nodePool.queue))
  {
    queuePop(&handle->nodePool.queue, &node);
    FatNode->init(node, &config);
  }
#else
  node = init(FatNode, &config);
#endif
  unlockPools(handle);

  return node;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static void extractShortBasename(char *baseName, const char *shortName)
{
  uint8_t index;

  for (index = 0; index < BASENAME_LENGTH; ++index)
  {
    if (!shortName[index] || shortName[index] == ' ')
      break;
    baseName[index] = shortName[index];
  }
  baseName[index] = '\0';
}
#endif
/*----------------------------------------------------------------------------*/
static void extractShortName(char *name, const struct DirEntryImage *entry)
{
  const char *source = entry->name;
  char *destination = name;

  /* Copy entry name */
  for (uint8_t position = 0; position < BASENAME_LENGTH; ++position)
  {
    if (*source != ' ')
      *destination++ = *source++;
  }
  /* Add dot when entry is not directory or extension exists */
  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    *destination++ = '.';
    source = entry->extension;

    /* Copy entry extension */
    for (uint8_t position = 0; position < BASENAME_LENGTH; ++position)
    {
      if (*source != ' ')
        *destination++ = *source++;
    }
  }
  *destination = '\0';
}
/*----------------------------------------------------------------------------*/
/* Fields node->index and node->cluster have to be initialized */
static enum result fetchEntry(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const struct DirEntryImage *entry;
  enum result res;

  /* Fields cluster, index and type are updated */
  while (1)
  {
    if (node->index >= nodeCount(handle))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      if ((res = getNextCluster(context, handle, &node->cluster)) != E_OK)
      {
        /* Set index to the last entry in last existing cluster */
        node->index = nodeCount(handle) - 1;
        return res;
      }
      node->index = 0;
    }

    const uint32_t sector = getSector(handle, node->cluster)
        + ENTRY_SECTOR(node->index);

    if ((res = readSector(context, handle, sector)) != E_OK)
      return res;
    entry = getEntry(context, node->index);

    /* Check for the end of the directory */
    if (!entry->name[0])
      return E_ENTRY;

    /* Volume entries are ignored */
    if (!(entry->flags & FLAG_VOLUME) || (entry->flags & MASK_LFN) == MASK_LFN)
      break;
    ++node->index;
  }

  if (entry->name[0] != E_FLAG_EMPTY && (entry->flags & MASK_LFN) != MASK_LFN)
    node->type = entry->flags & FLAG_DIR ? FS_TYPE_DIR : FS_TYPE_FILE;
  else
    node->type = FS_TYPE_NONE;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
/* Fields cluster and index should be initialized */
static enum result fetchNode(struct CommandContext *context,
    struct FatNode *node, struct FsMetadata *metadata)
{
  const struct DirEntryImage *entry;
  enum result res;
#ifdef CONFIG_FAT_UNICODE
  uint8_t checksum = 0;
  uint8_t chunks = 0; /* LFN chunks required */
  uint8_t found = 0; /* LFN chunks found */
#endif

  while ((res = fetchEntry(context, node)) == E_OK)
  {
    /* There is no need to reload sector in current context */
    entry = getEntry(context, node->index);

#ifdef CONFIG_FAT_UNICODE
    if ((entry->flags & MASK_LFN) == MASK_LFN
        && !(entry->ordinal & LFN_DELETED))
    {
      if (entry->ordinal & LFN_LAST)
      {
        found = 0;
        checksum = entry->checksum;
        chunks = entry->ordinal & ~LFN_LAST;
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
  if (!(entry->flags & FLAG_RO))
    node->access |= FS_ACCESS_WRITE;
  node->payload = ((uint32_t)fromLittleEndian16(entry->clusterHigh) << 16)
      | (uint32_t)fromLittleEndian16(entry->clusterLow);

#ifdef CONFIG_FAT_UNICODE
  if (!found || found != chunks || checksum != getChecksum(entry->filename,
      NAME_LENGTH))
  {
    /* Wrong checksum or chunk count does not match */
    node->nameIndex = node->index;
    node->nameCluster = node->cluster;
  }
#endif

  if (metadata)
  {
    metadata->type = node->type;
#ifdef CONFIG_FAT_UNICODE
    if (hasLongName(node))
    {
      if ((res = readLongName(context, metadata->name, node)) != E_OK)
        return res;
    }
    else
      extractShortName(metadata->name, entry);
#else
    extractShortName(metadata->name, entry);
#endif
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static const char *followPath(struct CommandContext *context,
    struct FatNode *node, const char *path, const struct FatNode *root)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FsMetadata *currentName = 0, *pathPart = 0;
  enum result res;

  /* Allocate temporary metadata buffers */
  if (!(currentName = allocateMetadata(handle)))
    return 0;

  if (!(pathPart = allocateMetadata(handle)))
  {
    freeMetadata(handle, currentName);
    return 0;
  }

  path = getChunk(path, pathPart->name);

  if (strlen(pathPart->name))
  {
    node->index = 0;

    if (!root && pathPart->name[0] == '/')
    {
      /* Resulting node is the root node */
      node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
      node->cluster = RESERVED_CLUSTER;
      node->payload = handle->rootCluster;
      node->type = FS_TYPE_DIR;
    }
    else
    {
      node->cluster = root ? root->payload : node->payload;

      while ((res = fetchNode(context, node, currentName)) == E_OK)
      {
        if (!strcmp(pathPart->name, currentName->name))
          break;
        ++node->index;
      }

      /* Check whether the node is found */
      if (res != E_OK)
        path = 0;
    }
  }
  else
    path = 0;

  /* Return buffers to metadata pool */
  freeMetadata(handle, pathPart);
  freeMetadata(handle, currentName);

  return path;
}
/*----------------------------------------------------------------------------*/
static void freeBuffers(struct FatHandle *handle, enum cleanup step)
{
  switch (step)
  {
    case FREE_ALL:
    case FREE_FILE_POOL:
#ifdef CONFIG_FAT_POOLS
      freePool(&handle->filePool);
#endif
    case FREE_DIR_POOL:
#ifdef CONFIG_FAT_POOLS
      freePool(&handle->dirPool);
#endif
    case FREE_NODE_POOL:
#ifdef CONFIG_FAT_POOLS
      freePool(&handle->nodePool);
#endif
    case FREE_FILE_LIST:
#ifdef CONFIG_FAT_WRITE
      listDeinit(&handle->openedFiles);
#endif
    case FREE_METADATA_POOL:
      freePool(&handle->metadataPool);
    case FREE_CONTEXT_POOL:
#ifdef CONFIG_FAT_THREADS
      freePool(&handle->contextPool);
#else
      free(handle->context);
#endif
    case FREE_LOCKS:
#ifdef CONFIG_FAT_THREADS
      mutexDeinit(&handle->memoryMutex);
      mutexDeinit(&handle->consistencyMutex);
#endif
      break;

    default:
      break;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_THREADS
static void freeContext(struct FatHandle *handle,
    const struct CommandContext *context)
{
  mutexLock(&handle->memoryMutex);
  queuePush(&handle->contextPool.queue, context);
  mutexUnlock(&handle->memoryMutex);
}
#else
static void freeContext(struct FatHandle *handle __attribute__((unused)),
    const struct CommandContext *context __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
static void freeMetadata(struct FatHandle *handle,
    const struct FsMetadata *metadata)
{
  lockPools(handle);
  queuePush(&handle->metadataPool.queue, metadata);
  unlockPools(handle);
}
/*----------------------------------------------------------------------------*/
/* Output buffer length should be greater or equal to maximum name length */
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

  while (*src && counter++ < CONFIG_FILENAME_LENGTH - 1)
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
static enum result getNextCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
{
  uint32_t nextCluster;
  enum result res;

  res = readSector(context, handle, handle->tableSector
      + (*cluster >> CELL_COUNT));
  if (res != E_OK)
    return res;

  memcpy(&nextCluster, context->buffer + CELL_OFFSET(*cluster),
      sizeof(nextCluster));
  nextCluster = fromLittleEndian32(nextCluster);

  if (clusterUsed(nextCluster))
  {
    *cluster = nextCluster;
    return E_OK;
  }

  return E_EMPTY;
}
/*----------------------------------------------------------------------------*/
static enum result mount(struct FatHandle *handle)
{
  struct CommandContext *context;
  enum result res;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  /* Read first sector */
  if ((res = readSector(context, handle, 0)) != E_OK)
    goto exit;

  const struct BootSectorImage * const boot =
      (const struct BootSectorImage *)context->buffer;

  /* Check boot sector signature (55AA at 0x01FE) */
  if (fromBigEndian16(boot->bootSignature) != 0x55AAU)
  {
    res = E_DEVICE;
    goto exit;
  }

  /* Check sector size, fixed size of 2 ^ SECTOR_EXP allowed */
  if (fromLittleEndian16(boot->bytesPerSector) != SECTOR_SIZE)
  {
    res = E_DEVICE;
    goto exit;
  }

  /* Calculate sectors per cluster count */
  uint16_t sizePow = boot->sectorsPerCluster;

  handle->clusterSize = 0;
  while (sizePow >>= 1)
    ++handle->clusterSize;

  handle->tableSector = fromLittleEndian16(boot->reservedSectors);
  handle->dataSector = handle->tableSector
      + boot->fatCopies * fromLittleEndian32(boot->fatSize);
  handle->rootCluster = fromLittleEndian32(boot->rootCluster);

  DEBUG_PRINT("Cluster size:   %u\n", (unsigned int)(1 << handle->clusterSize));
  DEBUG_PRINT("Table sector:   %u\n", (unsigned int)handle->tableSector);
  DEBUG_PRINT("Data sector:    %u\n", (unsigned int)handle->dataSector);

#ifdef CONFIG_FAT_WRITE
  handle->tableCount = boot->fatCopies;
  handle->tableSize = fromLittleEndian32(boot->fatSize);
  handle->clusterCount = ((fromLittleEndian32(boot->partitionSize)
      - handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = fromLittleEndian16(boot->infoSector);

  DEBUG_PRINT("Info sector:    %u\n", (unsigned int)handle->infoSector);
  DEBUG_PRINT("Table copies:   %u\n", (unsigned int)handle->tableCount);
  DEBUG_PRINT("Table size:     %u\n", (unsigned int)handle->tableSize);
  DEBUG_PRINT("Cluster count:  %u\n", (unsigned int)handle->clusterCount);
  DEBUG_PRINT("Sectors count:  %u\n",
      (unsigned int)fromLittleEndian32(boot->partitionSize));

  /* Read information sector */
  if ((res = readSector(context, handle, handle->infoSector)) != E_OK)
    goto exit;

  const struct InfoSectorImage * const info =
      (const struct InfoSectorImage *)context->buffer;

  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (fromBigEndian32(info->firstSignature) != 0x52526141UL
      || fromBigEndian32(info->infoSignature) != 0x72724161UL)
  {
    res = E_DEVICE;
    goto exit;
  }
  handle->lastAllocated = fromLittleEndian32(info->lastAllocated);

  DEBUG_PRINT("Free clusters:  %u\n",
      (unsigned int)fromLittleEndian32(info->freeClusters));
#endif

exit:
  freeContext(handle, context);
  return res;
}
/*----------------------------------------------------------------------------*/
static enum result readBuffer(struct FatHandle *handle, uint32_t sector,
    uint8_t *buffer, uint32_t count)
{
  const uint64_t address = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = count << SECTOR_EXP;
  enum result res;

  ifSet(handle->interface, IF_ACQUIRE, 0);
  res = ifSet(handle->interface, IF_ADDRESS, &address);
  if (res == E_OK)
  {
    if (ifRead(handle->interface, buffer, length) != length)
      res = E_INTERFACE;
  }
  ifSet(handle->interface, IF_RELEASE, 0);

  return res;
}
/*----------------------------------------------------------------------------*/
static enum result readSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  if (context->sector == sector)
    return E_OK;

  const uint64_t address = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = SECTOR_SIZE;
  enum result res;

  ifSet(handle->interface, IF_ACQUIRE, 0);
  res = ifSet(handle->interface, IF_ADDRESS, &address);
  if (res == E_OK)
  {
    if (ifRead(handle->interface, context->buffer, length) == length)
      context->sector = sector;
    else
      res = E_INTERFACE;
  }
  ifSet(handle->interface, IF_RELEASE, 0);

  return res;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
enum result rawTimestampToTime(time64_t *converted, uint16_t date,
    uint16_t time)
{
  struct RtcTime timestamp = {
      .second = time & 0x1F,
      .minute = (time >> 5) & 0x3F,
      .hour = (time >> 11) & 0x1F,
      .day = date & 0x1F,
      .month = (date >> 5) & 0x0F,
      .year = ((date >> 9) & 0x7F) + 1980
  };

  return rtcMakeEpochTime(converted, &timestamp);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
uint16_t timeToRawDate(const struct RtcTime *value)
{
  return value->day | (value->month << 5) | ((value->year - 1980) << 9);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
uint16_t timeToRawTime(const struct RtcTime *value)
{
  return (value->second >> 1) | (value->minute << 5) | (value->hour << 11);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
/* Extract 13 Unicode characters from long file name entry */
static void extractLongName(char16_t *name, const struct DirEntryImage *entry)
{
  memcpy(name, entry->longName0, sizeof(entry->longName0));
  name += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(name, entry->longName1, sizeof(entry->longName1));
  name += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(name, entry->longName2, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
/* Calculate entry name checksum for long file name entries support */
static uint8_t getChecksum(const char *name, uint8_t length)
{
  uint8_t sum = 0;

  for (uint8_t index = 0; index < length; ++index)
    sum = ((sum >> 1) | (sum << 7)) + *name++;

  return sum;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
static enum result readLongName(struct CommandContext *context, char *name,
    const struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FsMetadata *nameBuffer;
  struct FatNode allocatedNode;
  enum result res;

  /* Initialize temporary data */
  if (!(nameBuffer = allocateMetadata(handle)))
    return E_MEMORY;
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
  {
    freeMetadata(handle, nameBuffer);
    return res;
  }

  allocatedNode.cluster = node->nameCluster;
  allocatedNode.index = node->nameIndex;

  uint8_t chunks = 0;

  while ((res = fetchEntry(context, &allocatedNode)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    const struct DirEntryImage * const entry = getEntry(context,
        allocatedNode.index);

    if ((entry->flags & MASK_LFN) != MASK_LFN)
      break;

    const uint16_t offset = ((entry->ordinal & ~LFN_LAST) - 1);

    /* Compare resulting file name length and name buffer capacity */
    if (offset > ((CONFIG_FILENAME_LENGTH - 1) / 2 / LFN_ENTRY_LENGTH) - 1)
    {
      res = E_MEMORY;
      break;
    }

    if (entry->ordinal & LFN_LAST)
      *((char16_t *)nameBuffer->name + (offset + 1) * LFN_ENTRY_LENGTH) = 0;

    extractLongName((char16_t *)nameBuffer->name + offset * LFN_ENTRY_LENGTH,
        entry);
    ++chunks;
    ++allocatedNode.index;
  }

  /*
   * Long file name entries always precede data entry thus
   * processing of return values others than successful result is not needed.
   */
  if (res == E_OK)
  {
    if (chunks)
    {
      uFromUtf16(name, (const char16_t *)nameBuffer->name,
          CONFIG_FILENAME_LENGTH);
    }
    else
      res = E_ENTRY;
  }

  freeStaticNode(&allocatedNode);
  freeMetadata(handle, nameBuffer);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result allocateCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
{
  uint32_t current = handle->lastAllocated + 1;
  enum result res;

  while (current != handle->lastAllocated)
  {
    if (current >= handle->clusterCount)
      current = 2;

    uint32_t * const address = (uint32_t *)(context->buffer
        + CELL_OFFSET(current));
    const uint16_t currentOffset = current >> CELL_COUNT;

    res = readSector(context, handle, handle->tableSector + currentOffset);
    if (res != E_OK)
      return res;

    /* Check whether the cluster is free */
    if (clusterFree(fromLittleEndian32(*address)))
    {
      const uint32_t allocatedCluster = toLittleEndian32(current);
      const uint16_t parentOffset = *cluster >> CELL_COUNT;

      /* Mark cluster as busy */
      *address = toLittleEndian32(CLUSTER_EOC_VAL);

      /*
       * Save changes to the allocation table when previous cluster
       * is not available or parent cluster entry is located in other sector.
       */
      if (!*cluster || parentOffset != currentOffset)
      {
        if ((res = updateTable(context, handle, currentOffset)) != E_OK)
          return res;
      }

      /* Update reference cluster when previous cluster is available */
      if (*cluster)
      {
        res = readSector(context, handle, handle->tableSector + parentOffset);
        if (res != E_OK)
          return res;

        memcpy(context->buffer + CELL_OFFSET(*cluster), &allocatedCluster,
            sizeof(allocatedCluster));

        if ((res = updateTable(context, handle, parentOffset)) != E_OK)
          return res;
      }

      DEBUG_PRINT("Allocated cluster: %u, parent %u\n", current, *cluster);
      handle->lastAllocated = current;
      *cluster = current;

      /* Update information sector */
      if ((res = readSector(context, handle, handle->infoSector)))
        return res;

      struct InfoSectorImage * const info =
          (struct InfoSectorImage *)context->buffer;

      info->lastAllocated = toLittleEndian32(current);
      info->freeClusters =
          toLittleEndian32(fromLittleEndian32(info->freeClusters) - 1);

      if ((res = writeSector(context, handle, handle->infoSector)))
        return res;

      return E_OK;
    }

    ++current;
  }

  DEBUG_PRINT("Allocation error, partition may be full\n");
  return E_FULL;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result clearCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t sector = getSector(handle, cluster + 1);
  enum result res;

  memset(context->buffer, 0, SECTOR_SIZE);
  do
  {
    if ((res = writeSector(context, handle, --sector)) != E_OK)
      return res;
  }
  while (sector & ((1 << handle->clusterSize) - 1));

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static void copyNode(struct FatNode *destination, const struct FatNode *source)
{
  memcpy(destination, source, sizeof(struct FatNode));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result createNode(struct CommandContext *context,
    struct FatNode *node, const struct FatNode *root,
    const struct FsMetadata *metadata)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  char shortName[NAME_LENGTH];
  uint8_t chunks = 0;
  enum result res;

  res = fillShortName(shortName, metadata->name);
  if (metadata->type & FS_TYPE_DIR)
    memset(shortName + BASENAME_LENGTH, ' ', EXTENSION_LENGTH);

#ifdef CONFIG_FAT_UNICODE
  /* Allocate temporary metadata buffer */
  struct FsMetadata * const nameBuffer = allocateMetadata(handle);
  const bool longNameRequired = res != E_OK;

  if (!nameBuffer)
    return E_MEMORY;
#endif

  /* Propose new short name when selected name already exists */
  if ((res = uniqueNamePropose(context, root, shortName)) != E_OK)
    return res;

#ifdef CONFIG_FAT_UNICODE
  if (longNameRequired)
  {
    const uint16_t entryCount = CONFIG_FILENAME_LENGTH / 2 / LFN_ENTRY_LENGTH;
    const uint16_t length = uToUtf16((char16_t *)nameBuffer->name,
        metadata->name, LFN_ENTRY_LENGTH * entryCount);

    chunks = length / LFN_ENTRY_LENGTH;
    if (length > chunks * LFN_ENTRY_LENGTH)
    {
      /* Append additional entry when last chunk is incomplete */
      ++chunks;
      memset((char16_t *)nameBuffer->name + length, 0,
          (chunks * LFN_ENTRY_LENGTH) - length);
    }
  }
#endif

  /* Find suitable space within the directory */
  res = findGap(context, node, root, chunks + 1);

#ifdef CONFIG_FAT_UNICODE
  if (res == E_OK && chunks)
  {
    /* Save start cluster and index values before filling the chain */
    node->nameCluster = node->cluster;
    node->nameIndex = node->index;

    const uint8_t checksum = getChecksum(shortName, NAME_LENGTH);

    for (uint8_t current = chunks; current; --current)
    {
      const uint32_t sector = getSector(handle, node->cluster)
          + ENTRY_SECTOR(node->index);

      if ((res = readSector(context, handle, sector)) != E_OK)
        break;

      struct DirEntryImage * const entry = getEntry(context, node->index);

      fillLongName(entry, (const char16_t *)nameBuffer->name + (current - 1)
          * LFN_ENTRY_LENGTH);
      fillLongNameEntry(entry, current, chunks, checksum);

      ++node->index;

      if (!(node->index & (ENTRY_EXP - 1)))
      {
        /* Write back updated sector when switching sectors */
        if ((res = writeSector(context, handle, sector)) != E_OK)
          break;
      }

      if ((res = fetchEntry(context, node)) == E_ENTRY)
        res = E_OK;

      if (res != E_OK)
        break;
    }
  }

  /* Return buffer to metadata pool */
  freeMetadata(handle, nameBuffer);
#endif

  if (res != E_OK)
    return res;

  /* Buffer is already filled with actual sector data */
  const uint32_t sector = getSector(handle, node->cluster)
      + ENTRY_SECTOR(node->index);
  struct DirEntryImage * const entry = getEntry(context, node->index);

  /* Fill uninitialized node fields */
  node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  node->type = metadata->type;
  memcpy(entry->filename, shortName, NAME_LENGTH);
  /* Node data pointer is already initialized */
  fillDirEntry(entry, node);

  res = writeSector(context, handle, sector);

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static void fillDirEntry(struct DirEntryImage *entry,
    const struct FatNode *node)
{
  /* Clear unused fields */
  entry->unused0 = 0;
  entry->unused1 = 0;
  memset(entry->unused2, 0, sizeof(entry->unused2));

  entry->flags = 0;
  if (node->type == FS_TYPE_DIR)
    entry->flags |= FLAG_DIR;
  if (!(node->access & FS_ACCESS_WRITE))
    entry->flags |= FLAG_RO;

  entry->clusterHigh = toLittleEndian16((uint16_t)(node->payload >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)node->payload);
  entry->size = 0;

#ifdef CONFIG_FAT_TIME
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  if (handle->timer)
  {
    struct RtcTime currentTime;

    rtcMakeTime(&currentTime, rtcTime(handle->timer));
    entry->date = toLittleEndian16(timeToRawDate(&currentTime));
    entry->time = toLittleEndian16(timeToRawTime(&currentTime));
  }
  else
  {
    entry->date = 0;
    entry->time = 0;
  }
#else
  entry->date = 0;
  entry->time = 0;
#endif
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
/* Returns success when node is a valid short name */
static enum result fillShortName(char *shortName, const char *name)
{
  const char *dot;
  enum result res = E_OK;

  memset(shortName, ' ', NAME_LENGTH);

  /* Find start position of the extension */
  const uint16_t length = strlen(name);

  for (dot = name + length - 1; dot >= name && *dot != '.'; --dot);

  if (dot < name)
  {
    /* Dot not found */
    if (length > BASENAME_LENGTH)
      res = E_VALUE;
    dot = 0;
  }
  else
  {
    /* Check whether file name and extension have adequate length */
    if ((uint16_t)(length - (dot - name)) > EXTENSION_LENGTH + 1
        || (uint16_t)(dot - name) > BASENAME_LENGTH)
    {
      res = E_VALUE;
    }
  }

  uint8_t position = 0;

  for (char symbol = *name; symbol; symbol = *name)
  {
    if (dot && name == dot)
    {
      position = BASENAME_LENGTH;
      ++name;
      continue;
    }
    ++name;

    const char converted = processCharacter(symbol);

    if (converted != symbol)
      res = E_VALUE;
    if (!converted)
      continue;
    shortName[position++] = converted;

    if (position == BASENAME_LENGTH)
    {
      if (dot) /* Check whether extension exists */
      {
        name = dot + 1;
        continue;
      }
      else
        break;
    }
    if (position == NAME_LENGTH)
      break;
  }

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
/* Allocate single node or node chain inside root node chain. */
static enum result findGap(struct CommandContext *context, struct FatNode *node,
    const struct FatNode *root, uint8_t chainLength)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  uint32_t parent = root->payload;
  uint16_t index = 0;
  uint8_t chunks = 0;
  enum result res;

  node->cluster = root->payload;
  node->index = 0;

  while (chunks != chainLength)
  {
    bool allocateParent = false;

    switch ((res = fetchEntry(context, node)))
    {
      case E_EMPTY:
        /* End of the cluster chain, there are no empty entries */
        index = 0;
        allocateParent = true;
        break;

      case E_ENTRY:
        /* There are empty entries in the current cluster */
        index = node->index;
        parent = node->cluster;
        allocateParent = false;
        break;

      default:
        if (res != E_OK)
          return res;
        break;
    }

    if (res == E_EMPTY || res == E_ENTRY)
    {
      int16_t chunksLeft = (int16_t)(chainLength - chunks)
          - (int16_t)(nodeCount(handle) - node->index);

      while (chunksLeft > 0)
      {
        if ((res = allocateCluster(context, handle, &node->cluster)) != E_OK)
          return res;
        if ((res = clearCluster(context, handle, node->cluster)) != E_OK)
        {
          //FIXME Directory may contain erroneous entries
          return res;
        }

        if (allocateParent)
        {
          /* Place new entry in the beginning of the allocated cluster */
          parent = node->cluster;
          allocateParent = false;
        }
        chunksLeft -= (int16_t)nodeCount(handle);
      }
      break;
    }

    /*
     * Entry processing will be executed only after entry fetching so
     * in this case sector reloading is redundant.
     */
    const struct DirEntryImage * const entry = getEntry(context, node->index);

    /* Empty node, deleted long file name node or deleted node */
    if (!entry->name[0] || ((entry->flags & MASK_LFN) == MASK_LFN
        && (entry->ordinal & LFN_DELETED)) || entry->name[0] == E_FLAG_EMPTY)
    {
      if (!chunks) /* When first free node found */
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
#ifdef CONFIG_FAT_WRITE
static enum result freeChain(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t current = cluster;
  uint32_t released = 0;
  enum result res;

  if (current == RESERVED_CLUSTER)
    return E_OK; /* Already empty */

  while (clusterUsed(current))
  {
    /* Read allocation table sector with next cluster value */
    res = readSector(context, handle, handle->tableSector
        + (current >> CELL_COUNT));
    if (res != E_OK)
      break;

    uint32_t * const address = (uint32_t *)(context->buffer
        + CELL_OFFSET(current));
    const uint32_t next = fromLittleEndian32(*address);

    *address = 0;

    /* Update table when switching table sectors */
    if (current >> CELL_COUNT != next >> CELL_COUNT)
    {
      if ((res = updateTable(context, handle, current >> CELL_COUNT)) != E_OK)
        break;
    }

    ++released;
    DEBUG_PRINT("Cleared cluster: %u\n", current);
    current = next;
  }

  if (res != E_OK)
    return res;

  /* Update information sector */
  if ((res = readSector(context, handle, handle->infoSector)) != E_OK)
    return res;

  struct InfoSectorImage * const info =
      (struct InfoSectorImage *)context->buffer;

  /* Set free clusters count */
  info->freeClusters = toLittleEndian32(fromLittleEndian32(info->freeClusters)
      + released);

  /* TODO Cache information sector updates */
  res = writeSector(context, handle, handle->infoSector);

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result markFree(struct CommandContext *context,
    const struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FatNode allocatedNode;
  enum result res;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
    return res;

#ifdef CONFIG_FAT_UNICODE
  allocatedNode.cluster = node->nameCluster;
  allocatedNode.index = node->nameIndex;
#endif

  const uint32_t lastSector = getSector(handle, allocatedNode.cluster)
      + ENTRY_SECTOR(allocatedNode.index);

  while ((res = fetchEntry(context, &allocatedNode)) == E_OK)
  {
    const uint32_t sector = getSector(handle, allocatedNode.cluster)
        + ENTRY_SECTOR(allocatedNode.index);
    const bool lastEntry = sector == lastSector
        && allocatedNode.index == node->index;

    /* Sector is already loaded */
    struct DirEntryImage * const entry = getEntry(context, allocatedNode.index);

    /* Mark entry as empty by changing first byte of the name */
    entry->name[0] = E_FLAG_EMPTY;

    if (lastEntry || !(allocatedNode.index & (ENTRY_EXP - 1)))
    {
      /* Write back updated sector when switching sectors or last entry freed */
      if ((res = writeSector(context, handle, sector)) != E_OK)
        break;
    }

    ++allocatedNode.index;

    if (lastEntry)
      break;
  }

  freeStaticNode(&allocatedNode);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static char processCharacter(char value)
{
  /* Returns zero when character is not allowed or character code otherwise */

  if (value == ' ')
  {
    /* Remove spaces */
    return 0;
  }
  else if (value >= 'a' && value <= 'z')
  {
    /* Convert lower case characters to upper case */
    return value - ('a' - 'A');
  }
  else if (value > 0x20 && !(value & 0x80)
      && !(value >= 0x3A && value <= 0x3F)
      && !strchr("\x22\x2A\x2B\x2C\x2E\x2F\x5B\x5C\x5D\x7C\x7F", value))
  {
    return value;
  }
  else
  {
    /* Replace all other characters with underscore */
    return '_';
  }
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static unsigned int uniqueNameConvert(char *shortName)
{
  unsigned int nameIndex = 0;
  uint8_t delimiterPosition = 0;

  //TODO Search for delimiter from the end
  for (uint8_t position = 0; position < BASENAME_LENGTH; ++position)
  {
    if (!shortName[position] || shortName[position] == ' ')
    {
      break;
    }
    else if (shortName[position] == '~')
    {
      delimiterPosition = position++;

      while (position < BASENAME_LENGTH)
      {
        if (shortName[position] < '0' || shortName[position] > '9')
          break;

        const uint8_t currentNumber = shortName[position] - '0';

        nameIndex = (nameIndex * 10) + (unsigned int)currentNumber;
        ++position;
      }

      if (shortName[position] != '\0')
        nameIndex = 0;
      break;
    }
  }

  if (nameIndex)
    shortName[delimiterPosition] = '\0';

  return nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result uniqueNamePropose(struct CommandContext *context,
    const struct FatNode *root, char *shortName)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  struct FatNode allocatedNode;
  char currentName[BASENAME_LENGTH + 1];
  enum result res;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
    return res;

  extractShortBasename(currentName, shortName);
  allocatedNode.cluster = root->payload;
  allocatedNode.index = 0;

  unsigned int proposed = 0;

  while ((res = fetchEntry(context, &allocatedNode)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    const struct DirEntryImage * const entry = getEntry(context,
        allocatedNode.index);

    if (entry->name[0] == E_FLAG_EMPTY || (entry->flags & MASK_LFN) == MASK_LFN)
    {
      ++allocatedNode.index;
      continue;
    }

    unsigned int instance;
    char baseName[BASENAME_LENGTH + 1];

    /* Extract short name without extension */
    extractShortBasename(baseName, entry->name);

    if ((instance = uniqueNameConvert(baseName)))
    {
      if (!strncmp(baseName, currentName, strlen(baseName))
          && proposed <= instance)
      {
        proposed = instance + 1;
      }
    }
    else
    {
      if (!strcmp(currentName, baseName) && !proposed)
        ++proposed;
    }

    ++allocatedNode.index;
  }

  if (!proposed)
  {
    res = E_OK;
  }
  else if (proposed < MAX_SIMILAR_NAMES)
  {
    char suffix[BASENAME_LENGTH - 1];
    char *position = suffix;

    while (proposed)
    {
      const uint8_t symbol = proposed % 10;

      *position++ = symbol + '0';
      proposed /= 10;
    }

    const uint8_t proposedLength = position - suffix;
    const uint8_t remainingSpace = BASENAME_LENGTH - proposedLength - 1;
    uint8_t baseLength = strlen(currentName);

    if (baseLength > remainingSpace)
      baseLength = remainingSpace;

    memset(shortName + baseLength, ' ', BASENAME_LENGTH - baseLength);
    shortName[baseLength] = '~';

    for (uint8_t index = 1; index <= proposedLength; ++index)
      shortName[baseLength + index] = suffix[proposedLength - index];

    DEBUG_PRINT("Proposed short name: \"%.8s\"\n", shortName);

    res = E_OK;
  }
  else
  {
    res = E_EXIST;
  }

  freeStaticNode(&allocatedNode);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result setupDirCluster(struct CommandContext *context,
    const struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  enum result res;

  if ((res = clearCluster(context, handle, node->payload)) != E_OK)
    return res;

  struct DirEntryImage *entry = getEntry(context, 0);

  /* Current directory entry . */
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = '.';
  fillDirEntry(entry, node);

  /* Parent directory entry .. */
  ++entry;
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = entry->name[1] = '.';
  fillDirEntry(entry, node);
  if (node->cluster != handle->rootCluster)
  {
    entry->clusterHigh = toLittleEndian16((uint16_t)(node->payload >> 16));
    entry->clusterLow = toLittleEndian16((uint16_t)node->payload);
  }
  else
    entry->clusterLow = entry->clusterHigh = 0;

  res = writeSector(context, handle, getSector(handle, node->payload));
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result syncFile(struct CommandContext *context,
    struct FatFile *file)
{
  struct FatHandle * const handle = (struct FatHandle *)file->handle;
  enum result res;

  if (!(file->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  const uint32_t sector = getSector(handle, file->parentCluster)
      + ENTRY_SECTOR(file->parentIndex);

  /* Lock handle to prevent directory modifications from other threads */
  lockHandle(handle);
  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    unlockHandle(handle);
    return res;
  }

  /* Pointer to entry position in sector */
  struct DirEntryImage * const entry = getEntry(context, file->parentIndex);

  /* Update first cluster when writing to empty file or truncating file */
  entry->clusterHigh = toLittleEndian16((uint16_t)(file->payload >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)file->payload);
  /* Update file size */
  entry->size = toLittleEndian32(file->size);

#ifdef CONFIG_FAT_TIME
  if (handle->timer)
  {
    struct RtcTime currentTime;

    rtcMakeTime(&currentTime, rtcTime(handle->timer));
    entry->date = toLittleEndian16(timeToRawDate(&currentTime));
    entry->time = toLittleEndian16(timeToRawTime(&currentTime));
  }
#endif

  res = writeSector(context, handle, sector);
  unlockHandle(handle);

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
/* Copy current sector into FAT sectors located at offset */
static enum result updateTable(struct CommandContext *context,
    struct FatHandle *handle, uint32_t offset)
{
  for (uint8_t fat = 0; fat < handle->tableCount; ++fat)
  {
    const enum result res = writeSector(context, handle, offset
        + handle->tableSector + handle->tableSize * fat);

    if (res != E_OK)
      return res;
  }

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result writeBuffer(struct FatHandle *handle,
    uint32_t sector, const uint8_t *buffer, uint32_t count)
{
  const uint64_t address = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = count << SECTOR_EXP;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifWrite(handle->interface, buffer, length) != length)
    return E_INTERFACE;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result writeSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  const uint64_t address = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = SECTOR_SIZE;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifWrite(handle->interface, context->buffer, length) != length)
    return E_INTERFACE;
  context->sector = sector;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) || defined(CONFIG_FAT_WRITE)
static enum result allocateStaticNode(struct FatHandle *handle,
    struct FatNode *node)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };
  enum result res;

  /* Initialize class descriptor manually */
  ((struct Entity *)node)->descriptor = FatNode;

  /* Call constructor for the statically allocated object */
  if ((res = FatNode->init(node, &config)))
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) || defined(CONFIG_FAT_WRITE)
static void freeStaticNode(struct FatNode *node)
{
  FatNode->deinit(node);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) && defined(CONFIG_FAT_WRITE)
/* Save Unicode characters to long file name entry */
static void fillLongName(struct DirEntryImage *entry, const char16_t *name)
{
  memcpy(entry->longName0, name, sizeof(entry->longName0));
  name += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(entry->longName1, name, sizeof(entry->longName1));
  name += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(entry->longName2, name, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) && defined(CONFIG_FAT_WRITE)
static void fillLongNameEntry(struct DirEntryImage *entry, uint8_t current,
    uint8_t total, uint8_t checksum)
{
  /* Clear reserved fields */
  entry->unused0 = 0;
  entry->unused3 = 0;

  entry->flags = MASK_LFN;
  entry->checksum = checksum;
  entry->ordinal = current;
  if (current == total)
    entry->ordinal |= LFN_LAST;
}
#endif
/*------------------Filesystem handle functions-------------------------------*/
static enum result fatHandleInit(void *object, const void *configBase)
{
  const struct Fat32Config * const config = configBase;
  struct FatHandle * const handle = object;
  enum result res;

  if ((res = allocateBuffers(handle, config)) != E_OK)
    return res;

  handle->interface = config->interface;

#ifdef CONFIG_FAT_TIME
  handle->timer = config->timer;
  if (!handle->timer)
    DEBUG_PRINT("Real-time clock is not initialized");
#endif

  if ((res = mount(handle)) != E_OK)
    return res;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatHandleDeinit(void *object)
{
  struct FatHandle * const handle = object;

  freeBuffers(handle, FREE_ALL);
}
/*----------------------------------------------------------------------------*/
static void *fatFollow(void *object, const char *path, const void *root)
{
  struct FatHandle * const handle = object;
  struct CommandContext *context;
  struct FatNode *node;

  if (!(context = allocateContext(handle)))
    return 0;

  if (!(node = allocateNode(object)))
  {
    freeContext(handle, context);
    return 0;
  }

  while (path && *path)
    path = followPath(context, node, path, root);
  freeContext(handle, context);

  if (!path)
  {
    fatFree(node);
    return 0;
  }
  else
    return node;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatSync(void *object)
{
  struct FatHandle * const handle = object;
  struct CommandContext *context;
  struct FatFile *descriptor;
  enum result res;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  const struct ListNode *current = listFirst(&handle->openedFiles);

  while (current)
  {
    listData(&handle->openedFiles, current, &descriptor);
    if ((res = syncFile(context, descriptor)) != E_OK)
    {
      freeContext(handle, context);
      return res;
    }
    current = listNext(current);
  }

  freeContext(handle, context);
  return E_OK;
}
#else
static enum result fatSync(void *object __attribute__((unused)))
{
  return E_OK;
}
#endif
/*------------------Node functions--------------------------------------------*/
static enum result fatNodeInit(void *object, const void *configBase)
{
  const struct FatNodeConfig * const config = configBase;
  struct FatNode * const node = object;

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
static void *fatClone(const void *object)
{
  const struct FatNode * const node = object;

  return allocateNode((struct FatHandle *)node->handle);
}
/*----------------------------------------------------------------------------*/
static void fatFree(void *object)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  lockPools(handle);
#ifdef CONFIG_FAT_POOLS
  FatNode->deinit(node);
  queuePush(&handle->nodePool.queue, node);
#else
  deinit(object);
#endif
  unlockPools(handle);
}
/*----------------------------------------------------------------------------*/
static enum result fatGet(const void *object, enum fsNodeData type, void *data)
{
  const struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  switch (type)
  {
#ifdef CONFIG_FAT_UNICODE
    case FS_NODE_METADATA:
    {
      if (!hasLongName(node))
        break; /* Short name cannot be read without sector reload */

      if (!(context = allocateContext(handle)))
        return E_MEMORY;

      struct FsMetadata *metadata = data;

      metadata->type = node->type;
      res = readLongName(context, metadata->name, node);
      freeContext(handle, context);

      return res;
    }

    case FS_NODE_NAME:
    {
      if (!hasLongName(node))
        break;
      if (!(context = allocateContext(handle)))
        return E_MEMORY;

      res = readLongName(context, (char *)data, node);
      freeContext(handle, context);
      return res;
    }
#endif

    case FS_NODE_ACCESS:
    {
      *(access_t *)data = node->access;
      return E_OK;
    }

    case FS_NODE_TYPE:
    {
      *(enum fsNodeType *)data = node->type;
      return E_OK;
    }

    default:
      break;
  }

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  const uint32_t sector = getSector(handle, node->cluster)
      + ENTRY_SECTOR(node->index);

  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    freeContext(handle, context);
    return res;
  }

  const struct DirEntryImage * const entry = getEntry(context, node->index);

  switch (type)
  {
    case FS_NODE_METADATA:
    {
      struct FsMetadata * const metadata = data;

      metadata->type = node->type;
      extractShortName(metadata->name, entry);
      break;
    }

    case FS_NODE_NAME:
    {
      extractShortName((char *)data, entry);
      break;
    }

    case FS_NODE_SIZE:
    {
      *(uint64_t *)data = (uint64_t)fromLittleEndian32(entry->size);
      break;
    }

#ifdef CONFIG_FAT_TIME
    case FS_NODE_TIME:
    {
      const uint16_t rawDate = fromLittleEndian16(entry->date);
      const uint16_t rawTime = fromLittleEndian16(entry->time);

      res = rawTimestampToTime((time64_t *)data, rawDate, rawTime);
      break;
    }
#endif

    default:
      res = E_VALUE;
      break;
  }
  freeContext(handle, context);

  return res;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatLink(void *object, const struct FsMetadata *metadata,
    const void *targetBase, void *result)
{
  const struct FatNode * const target = targetBase;
  const struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FatNode allocatedNode;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
    return res;

  if ((context = allocateContext(handle)))
  {
    allocatedNode.payload = target->payload;
    lockHandle(handle);
    res = createNode(context, &allocatedNode, node, metadata);
    unlockHandle(handle);

    freeContext(handle, context);
  }
  else
    res = E_MEMORY;

  if (res == E_OK && result)
    copyNode(result, &allocatedNode);

  freeStaticNode(&allocatedNode);
  return res;
}
#else
static enum result fatLink(void *object __attribute__((unused)),
    const struct FsMetadata *metadata __attribute__((unused)),
    const void *target __attribute__((unused)),
    void *result __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatMake(void *object, const struct FsMetadata *metadata,
    void *result)
{
  const struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FatNode allocatedNode;
  struct CommandContext *context;
  enum result res;

  if (node->type != FS_TYPE_DIR)
    return E_VALUE;
  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
    return res;

  if ((context = allocateContext(handle)))
  {
    allocatedNode.payload = RESERVED_CLUSTER;

    /* Prevent unexpected modifications from other threads */
    lockHandle(handle);

    /* Allocate a cluster chain for the directory */
    if (metadata->type == FS_TYPE_DIR)
    {
      res = allocateCluster(context, handle, &allocatedNode.payload);

      if (res == E_OK)
      {
        /*
         * Coherence checking is not needed during setup of the first cluster
         * because the directory entry will be created on the next step.
         */
        unlockHandle(handle);
        res = setupDirCluster(context, &allocatedNode);
        lockHandle(handle);

        if (res != E_OK)
          freeChain(context, handle, allocatedNode.payload);
      }
    }

    /* Create an entry in the parent directory */
    if (res == E_OK)
    {
      res = createNode(context, &allocatedNode, node, metadata);

      if (res != E_OK && metadata->type == FS_TYPE_DIR)
        freeChain(context, handle, allocatedNode.payload);
    }

    unlockHandle(handle);
    freeContext(handle, context);
  }
  else
    res = E_MEMORY;

  if (res == E_OK && result)
    copyNode(result, &allocatedNode);

  freeStaticNode(&allocatedNode);
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
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  if ((node->access & access) != access)
    return 0;

  switch (node->type)
  {
    case FS_TYPE_DIR:
    {
      const struct FatDirConfig config = {
          .node = object
      };
      struct FatDir *dir = 0;

      lockPools(handle);
#ifdef CONFIG_FAT_POOLS
      if (!queueEmpty(&handle->dirPool.queue))
      {
        queuePop(&handle->dirPool.queue, &dir);
        FatDir->init(dir, &config);
      }
#else
      dir = init(FatDir, &config);
#endif
      unlockPools(handle);

      return dir;
    }
    case FS_TYPE_FILE:
    {
      const struct FatFileConfig config = {
          .node = object,
          .access = access
      };
      struct FatFile *file = 0;
      uint64_t size;

      if (fatGet(node, FS_NODE_SIZE, &size) != E_OK)
        return 0;

      lockPools(handle);
#ifdef CONFIG_FAT_POOLS
      if (!queueEmpty(&handle->filePool.queue))
      {
        queuePop(&handle->filePool.queue, &file);
        FatFile->init(file, &config);
      }
#else
      file = init(FatFile, &config);
#endif
      unlockPools(handle);

      if (file)
        file->size = (uint32_t)size;

      return file;
    }
    default:
      return 0;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatSet(void *object, enum fsNodeData type, const void *data)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  const uint32_t sector = getSector(handle, node->cluster)
      + ENTRY_SECTOR(node->index);

  if ((res = readSector(context, handle, sector)) != E_OK)
    return res;

  struct DirEntryImage * const entry = getEntry(context, node->index);

  switch (type)
  {
    case FS_NODE_ACCESS:
    {
      node->access = *(const access_t *)data;
      if (node->access & FS_ACCESS_WRITE)
        entry->flags &= ~FLAG_RO;
      else
        entry->flags |= FLAG_RO;
      break;
    }

    default:
      return E_VALUE;
  }

  if ((res = writeSector(context, handle, sector)) != E_OK)
    return res;

  return E_OK;
}
#else
static enum result fatSet(void *object __attribute__((unused)),
    enum fsNodeData type __attribute__((unused)),
    const void *data __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatTruncate(void *object)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  if (node->type == FS_TYPE_DIR && node->payload != RESERVED_CLUSTER)
  {
    /* Preserve information */
    const uint32_t cluster = node->cluster;
    const uint16_t index = node->index;
    const enum fsNodeType type = node->type;

    /* Check whether the directory is not empty */
    node->cluster = node->payload;
    node->index = 2; /* Exclude . and .. */ //FIXME Make the position arbitrary

    if ((res = fetchNode(context, node, 0)) == E_OK)
      res = E_EXIST;
    if (res != E_EMPTY && res != E_ENTRY)
    {
      freeContext(handle, context);
      return res;
    }

    /* Restore values changed by node fetching function */
    node->cluster = cluster;
    node->index = index;
    node->type = type;
  }

  /* Mark clusters as free */
  lockHandle(handle);
  res = freeChain(context, handle, node->payload);
  unlockHandle(handle);

  if (res == E_OK)
    node->payload = RESERVED_CLUSTER;

  freeContext(handle, context);
  return res;
}
#else
static enum result fatTruncate(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatUnlink(void *object)
{
  const struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  lockHandle(handle);
  res = markFree(context, node);
  unlockHandle(handle);

  freeContext(handle, context);
  return res;
}
#else
static enum result fatUnlink(void *object __attribute__((unused)))
{
  return E_ERROR;
}
#endif
/*------------------Directory functions---------------------------------------*/
static enum result fatDirInit(void *object, const void *configBase)
{
  const struct FatDirConfig * const config = configBase;
  const struct FatNode * const node = (const struct FatNode *)config->node;
  struct FatDir * const dir = object;

  if (node->type != FS_TYPE_DIR)
    return E_VALUE;

  dir->handle = node->handle;
  dir->payload = node->payload;
  dir->currentCluster = node->payload;
  dir->currentIndex = 0;
  DEBUG_PRINT("Directory allocated, address %08lX\n", (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatDirDeinit(void *object __attribute__((unused)))
{
  DEBUG_PRINT("Directory freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
static enum result fatDirClose(void *object)
{
  const struct FatDir * const dir = object;
  struct FatHandle *handle = (struct FatHandle *)dir->handle;

  lockPools(handle);
#ifdef CONFIG_FAT_POOLS
  FatDir->deinit(object);
  queuePush(&handle->dirPool.queue, object);
#else
  deinit(object);
#endif
  unlockPools(handle);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static bool fatDirEnd(const void *object)
{
  const struct FatDir * const dir = object;

  return dir->currentCluster == RESERVED_CLUSTER;
}
/*----------------------------------------------------------------------------*/
static enum result fatDirFetch(void *object, void *nodeBase)
{
  struct FatNode * const node = nodeBase;
  struct FatDir * const dir = object;
  struct FatHandle * const handle = (struct FatHandle *)dir->handle;
  struct CommandContext *context;
  enum result res;

  if (dir->currentCluster == RESERVED_CLUSTER)
    return E_ENTRY;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  node->cluster = dir->currentCluster;
  node->index = dir->currentIndex;

  res = fetchNode(context, node, 0);
  freeContext(handle, context);

  if (res != E_OK)
  {
    if (res == E_EMPTY || res == E_ENTRY)
    {
      /* Reached the end of the directory */
      dir->currentCluster = RESERVED_CLUSTER;
      dir->currentIndex = 0;

      return E_ENTRY;
    }
    else
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
  struct FatDir * const dir = object;

  if (origin == FS_SEEK_SET)
  {
    if (!offset)
    {
      dir->currentCluster = dir->payload;
      dir->currentIndex = 0;
    }
    else
    {
      dir->currentCluster = (uint32_t)(offset >> 16);
      dir->currentIndex = (uint16_t)offset;
    }
    return E_OK;
  }

  return E_VALUE;
}
/*----------------------------------------------------------------------------*/
static uint64_t fatDirTell(const void *object)
{
  const struct FatDir * const dir = object;

  return (uint64_t)dir->currentIndex | ((uint64_t)dir->currentCluster << 16);
}
/*------------------File functions--------------------------------------------*/
static enum result fatFileInit(void *object, const void *configBase)
{
  const struct FatFileConfig * const config = configBase;
  const struct FatNode * const node = (const struct FatNode *)config->node;
  struct FatFile * const file = object;

  if (node->type != FS_TYPE_FILE)
    return E_VALUE;

  if (config->access & FS_ACCESS_WRITE)
  {
#ifdef CONFIG_FAT_WRITE
    struct FatHandle * const handle = (struct FatHandle *)node->handle;
    enum result res;

    if ((res = listPush(&handle->openedFiles, &file)) != E_OK)
      return res;
#else
    /* Trying to open file for writing on read-only filesystem */
    return E_ACCESS;
#endif
  }

  file->access = config->access;
  file->handle = node->handle;
  file->position = 0;
  file->size = 0;
  file->payload = node->payload;
  file->currentCluster = node->payload;
#ifdef CONFIG_FAT_WRITE
  file->parentCluster = node->cluster;
  file->parentIndex = node->index;
#endif

  DEBUG_PRINT("File allocated, address %08lX\n", (unsigned long)object);
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatFileDeinit(void *object)
{
  const struct FatFile * const file = object;

  if (file->access & FS_ACCESS_WRITE)
  {
#ifdef CONFIG_FAT_WRITE
    struct FatHandle * const handle = (struct FatHandle *)file->handle;
    struct ListNode *current = listFirst(&handle->openedFiles);
    struct FatFile *descriptor;

    while (current)
    {
      listData(&handle->openedFiles, current, &descriptor);
      if (descriptor == file)
      {
        listErase(&handle->openedFiles, current);
        break;
      }
      current = listNext(current);
    }
#endif
  }

  DEBUG_PRINT("File freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
static enum result fatFileClose(void *object)
{
  struct FatFile * const file = object;
  struct FatHandle * const handle = (struct FatHandle *)file->handle;

#ifdef CONFIG_FAT_WRITE
  if (file->access & FS_ACCESS_WRITE)
  {
    struct CommandContext *context;
    enum result res;

    if (!(context = allocateContext(handle)))
      return E_MEMORY;
    res = syncFile(context, file);
    freeContext(handle, context);

    if (res != E_OK)
      return res;
  }
#endif

  lockPools(handle);
#ifdef CONFIG_FAT_POOLS
  FatFile->deinit(file);
  queuePush(&handle->filePool.queue, file);
#else
  deinit(object);
#endif
  unlockPools(handle);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static bool fatFileEnd(const void *object)
{
  const struct FatFile * const file = object;

  return file->position >= file->size;
}
/*----------------------------------------------------------------------------*/
static uint32_t fatFileRead(void *object, void *buffer, uint32_t length)
{
  struct FatFile * const file = object;
  struct FatHandle * const handle = (struct FatHandle *)file->handle;
  struct CommandContext *context;
  uint32_t read = 0;
  uint16_t chunk;
  uint8_t current = 0;

  if (!(file->access & FS_ACCESS_READ))
    return 0;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

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
      if (getNextCluster(context, handle, &file->currentCluster) != E_OK)
      {
        freeContext(handle, context);
        return 0; /* Sector read error or end of file */
      }
      current = 0;
    }

    /* Offset from the beginning of the sector */
    const uint16_t offset = (file->position + read) & (SECTOR_SIZE - 1);

    if (offset || length < SECTOR_SIZE) /* Position within the sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = length < chunk ? length : chunk;

      if (readSector(context, handle, getSector(handle, file->currentCluster)
          + current) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }
      memcpy((uint8_t *)buffer + read, context->buffer + offset, chunk);

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_EXP);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Read data to the buffer directly without additional copying */
      if (readBuffer(handle, getSector(handle, file->currentCluster)
          + current, (uint8_t *)buffer + read, chunk >> SECTOR_EXP) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      current += chunk >> SECTOR_EXP;
    }

    read += chunk;
    length -= chunk;
  }

  freeContext(handle, context);
  file->position += read;

  return read;
}
/*----------------------------------------------------------------------------*/
static enum result fatFileSeek(void *object, uint64_t offset,
    enum fsSeekOrigin origin)
{
  struct FatFile * const file = object;
  struct FatHandle * const handle = (struct FatHandle *)file->handle;
  struct CommandContext *context;
  enum result res;

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
    return E_VALUE;

  uint32_t clusterCount;
  uint32_t current;

  if (offset > file->position)
  {
    current = file->currentCluster;
    clusterCount = offset - file->position;
  }
  else
  {
    current = file->payload;
    clusterCount = offset;
  }
  clusterCount >>= handle->clusterSize + SECTOR_EXP;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  while (clusterCount--)
  {
    if ((res = getNextCluster(context, handle, &current)) != E_OK)
    {
      freeContext(handle, context);
      return res;
    }
  }

  freeContext(handle, context);
  file->position = offset;
  file->currentCluster = current;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static uint64_t fatFileTell(const void *object)
{
  return (uint64_t)((const struct FatFile *)object)->position;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static uint32_t fatFileWrite(void *object, const void *buffer, uint32_t length)
{
  struct FatFile * const file = object;
  struct FatHandle * const handle = (struct FatHandle *)file->handle;
  struct CommandContext *context;
  uint32_t written = 0;
  uint16_t chunk;
  uint8_t current = 0; /* Current sector of the data cluster */

  if (!(file->access & FS_ACCESS_WRITE))
    return 0;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  if (file->payload == RESERVED_CLUSTER)
  {
    enum result res;

    /* Lock handle to prevent table modifications from other threads */
    lockHandle(handle);
    res = allocateCluster(context, handle, &file->payload);
    unlockHandle(handle);

    if (res != E_OK)
    {
      freeContext(handle, context);
      return 0;
    }

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
      res = getNextCluster(context, handle, &file->currentCluster);

      if (res == E_EMPTY)
      {
        /* Prevent table modifications from other threads */
        lockHandle(handle);
        res = allocateCluster(context, handle, &file->currentCluster);
        unlockHandle(handle);
      }

      if (res != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      current = 0;
    }

    /* Offset from the beginning of the sector */
    const uint16_t offset = (file->position + written) & (SECTOR_SIZE - 1);

    if (offset || length < SECTOR_SIZE) /* Position within the sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (length < chunk) ? length : chunk;

      const uint32_t sector = getSector(handle, file->currentCluster) + current;

      if (readSector(context, handle, sector) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      memcpy(context->buffer + offset, (const uint8_t *)buffer + written,
          chunk);
      if (writeSector(context, handle, sector) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_EXP);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Write data from the buffer directly without additional copying */
      if (writeBuffer(handle, getSector(handle, file->currentCluster) + current,
          (const uint8_t *)buffer + written, chunk >> SECTOR_EXP) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      current += chunk >> SECTOR_EXP;
    }

    written += chunk;
    length -= chunk;
  }

  freeContext(handle, context);
  file->position += written;
  if (file->position > file->size)
    file->size = file->position;

  return written;
}
#else
static uint32_t fatFileWrite(void *entry __attribute__((unused)),
    const void *buffer __attribute__((unused)),
    uint32_t length __attribute__((unused)))
{
  return 0;
}
#endif
/*------------------Unimplemented functions-----------------------------------*/
static enum result fatMount(void *object __attribute__((unused)),
    void *handle __attribute__((unused)))
{
  return E_ERROR;
}
/*----------------------------------------------------------------------------*/
static void fatUnmount(void *object __attribute__((unused)))
{

}
/*----------------------------------------------------------------------------*/
static uint32_t fatDirRead(void *object __attribute__((unused)),
    void *buffer __attribute__((unused)),
    uint32_t length __attribute__((unused)))
{
  /* Use fatDirFetch instead */
  return 0;
}
/*----------------------------------------------------------------------------*/
static uint32_t fatDirWrite(void *object __attribute__((unused)),
    const void *buffer __attribute__((unused)),
    uint32_t length __attribute__((unused)))
{
  /* Use fatMake and fatLink instead */
  return 0;
}
/*----------------------------------------------------------------------------*/
static enum result fatFileFetch(void *object __attribute__((unused)),
    void *nodeBase __attribute__((unused)))
{
  return E_ERROR;
}
