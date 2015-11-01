/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <bits.h>
#include <memory.h>
#include <libyaf/debug.h>
#include <libyaf/fat32.h>
#include <libyaf/fat32_defs.h>
#include <libyaf/fat32_inlines.h>
/*----------------------------------------------------------------------------*/
enum cleanup
{
  FREE_ALL,
  FREE_NODE_POOL,
  FREE_FILE_LIST,
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
static uint8_t computeShortNameLength(const struct DirEntryImage *);
static void extractShortName(char *, const struct DirEntryImage *);
static enum result fetchEntry(struct CommandContext *, struct FatNode *);
static enum result fetchNode(struct CommandContext *, struct FatNode *);
static uint32_t fileReadData(struct CommandContext *, struct FatNode *,
    uint32_t, void *, uint32_t);
static enum result fileSeekData(struct CommandContext *, struct FatNode *,
    uint32_t);
static void freeBuffers(struct FatHandle *, enum cleanup);
static void freeContext(struct FatHandle *, const struct CommandContext *);
static void freeNode(struct FatNode *);
static enum result getNextCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum result mountStorage(struct FatHandle *);
static enum result readBuffer(struct FatHandle *, uint32_t, uint8_t *,
    uint32_t);
static enum result readSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_TIME
static enum result rawTimestampToTime(time64_t *, uint16_t, uint16_t);
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_UNICODE
static void extractLongName(char16_t *, const struct DirEntryImage *);
static uint8_t getChecksum(const char *, uint8_t);
static enum result readLongName(struct CommandContext *, char *, uint32_t,
    const struct FatNode *);
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result allocateCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum result clearCluster(struct CommandContext *, struct FatHandle *,
    uint32_t);
static void clearDirtyFlag(struct FatNode *);
static enum result createNode(struct CommandContext *, const struct FatNode *,
    bool, const char *, access_t, uint32_t, time64_t);
static void extractShortBasename(char *, const char *);
static uint32_t fileWriteData(struct CommandContext *, struct FatNode *,
    uint32_t, const void *, uint32_t);
static void fillDirEntry(struct DirEntryImage *, bool, access_t, uint32_t,
    time64_t);
static enum result fillShortName(char *, const char *, bool);
static enum result findGap(struct CommandContext *, struct FatNode *,
    const struct FatNode *, uint8_t);
static enum result freeChain(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum result markFree(struct CommandContext *, const struct FatNode *);
static char processCharacter(char);
static enum result setupDirCluster(struct CommandContext *, struct FatHandle *,
    uint32_t, uint32_t, time64_t);
static enum result syncFile(struct CommandContext *, struct FatNode *);
static enum result truncatePayload(struct CommandContext *, struct FatNode *);
static unsigned int uniqueNameConvert(char *);
static enum result uniqueNamePropose(struct CommandContext *,
    const struct FatNode *, char *);
static enum result updateTable(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum result writeBuffer(struct FatHandle *, uint32_t, const uint8_t *,
    uint32_t);
static enum result writeSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_TIME) && defined(CONFIG_FAT_WRITE)
static uint16_t timeToRawDate(const struct RtDateTime *);
static uint16_t timeToRawTime(const struct RtDateTime *);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) || defined(CONFIG_FAT_WRITE)
static enum result allocateStaticNode(struct FatHandle *, struct FatNode *);
static void freeStaticNode(struct FatNode *);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_UNICODE) && defined(CONFIG_FAT_WRITE)
static void fillLongName(struct DirEntryImage *, const char16_t *, uint8_t);
static void fillLongNameEntry(struct DirEntryImage *, uint8_t, uint8_t,
    uint8_t);
#endif
/*----------------------------------------------------------------------------*/
/* Filesystem handle functions */
static enum result fatHandleInit(void *, const void *);
static void fatHandleDeinit(void *);
static void *fatHandleRoot(void *);
static enum result fatHandleSync(void *);

/* Node functions */
static enum result fatNodeInit(void *, const void *);
static void fatNodeDeinit(void *);
static enum result fatNodeCreate(void *, const struct FsFieldDescriptor *,
    uint8_t);
static void *fatNodeHead(void *);
static void fatNodeFree(void *);
static enum result fatNodeLength(void *, enum fsFieldType, length_t *);
static enum result fatNodeNext(void *);
static enum result fatNodeRead(void *, enum fsFieldType, length_t,
    void *, length_t, length_t *);
static enum result fatNodeRemove(void *, void *);
static enum result fatNodeWrite(void *, enum fsFieldType, length_t,
    const void *, length_t, length_t *);
/*------------------Class descriptors-----------------------------------------*/
static const struct FsHandleClass fatHandleTable = {
    .size = sizeof(struct FatHandle),
    .init = fatHandleInit,
    .deinit = fatHandleDeinit,

    .root = fatHandleRoot,
    .sync = fatHandleSync
};

static const struct FsNodeClass fatNodeTable = {
    .size = sizeof(struct FatNode),
    .init = fatNodeInit,
    .deinit = fatNodeDeinit,

    .create = fatNodeCreate,
    .head = fatNodeHead,
    .free = fatNodeFree,
    .length = fatNodeLength,
    .next = fatNodeNext,
    .read = fatNodeRead,
    .remove = fatNodeRemove,
    .write = fatNodeWrite
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass * const FatHandle = &fatHandleTable;
const struct FsNodeClass * const FatNode = &fatNodeTable;
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

    queuePush(&pool->queue, &data);
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

  DEBUG_PRINT(1, "fat32: context pool:   %u\n", (unsigned int)(count
      * (sizeof(struct CommandContext *) + sizeof(struct CommandContext))));
#else
  count = DEFAULT_THREAD_COUNT;

  /* Allocate single context buffer */
  handle->context = malloc(sizeof(struct CommandContext));
  if (!handle->context)
    return E_MEMORY;
  handle->context->sector = RESERVED_SECTOR;

  DEBUG_PRINT(1, "fat32: context pool:   %u\n",
      (unsigned int)sizeof(struct CommandContext));
#endif /* CONFIG_FAT_THREADS */

#ifdef CONFIG_FAT_WRITE
  res = listInit(&handle->openedFiles, sizeof(struct FatNode *));
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_CONTEXT_POOL);
    return res;
  }
#endif /* CONFIG_FAT_WRITE */

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

  DEBUG_PRINT(1, "fat32: node pool:      %u\n", (unsigned int)(count
      * (sizeof(struct FatNode *) + sizeof(struct FatNode))));
#endif /* CONFIG_FAT_POOLS */

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static struct CommandContext *allocateContext(struct FatHandle *handle)
{
  struct CommandContext *context = 0;

#ifdef CONFIG_FAT_THREADS
  mutexLock(&handle->memoryMutex);
  if (!queueEmpty(&handle->contextPool.queue))
    queuePop(&handle->contextPool.queue, &context);
  mutexUnlock(&handle->memoryMutex);
#else
  context = handle->context;
#endif

  context->sector = RESERVED_SECTOR;
  return context;
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
static uint8_t computeShortNameLength(const struct DirEntryImage *entry)
{
  const char *source;
  uint8_t nameLength = 0;

  source = entry->name;
  for (uint8_t index = 0; index < BASENAME_LENGTH; ++index)
  {
    if (*source++ == ' ')
      break;
    ++nameLength;
  }

  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    ++nameLength;

    source = entry->extension;
    for (uint8_t index = 0; index < EXTENSION_LENGTH; ++index)
    {
      if (*source++ == ' ')
        break;
      ++nameLength;
    }
  }

  return nameLength;
}
/*----------------------------------------------------------------------------*/
static void extractShortName(char *name, const struct DirEntryImage *entry)
{
  char *destination = name;
  const char *source;

  /* Copy entry name */
  source = entry->name;
  for (uint8_t index = 0; *source != ' ' && index < BASENAME_LENGTH; ++index)
    *destination++ = *source++;

  /* Add dot when entry is not directory or extension exists */
  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    *destination++ = '.';

    /* Copy entry extension */
    source = entry->extension;
    for (uint8_t index = 0; *source != ' ' && index < EXTENSION_LENGTH; ++index)
      *destination++ = *source++;
  }
  *destination = '\0';
}
/*----------------------------------------------------------------------------*/
/*
 * Fields handle, parentIndex and parentCluster in node have to be initialized.
 */
static enum result fetchEntry(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const struct DirEntryImage *entry;
  enum result res;

  /* Fields cluster, index and type are updated */
  while (1)
  {
    if (node->parentIndex >= nodeCount(handle))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      if ((res = getNextCluster(context, handle, &node->parentCluster)) != E_OK)
      {
        /* Set index to the last entry in last existing cluster */
        node->parentIndex = nodeCount(handle) - 1;
        return res;
      }
      node->parentIndex = 0;
    }

    const uint32_t sector = getSector(handle, node->parentCluster)
        + ENTRY_SECTOR(node->parentIndex);

    if ((res = readSector(context, handle, sector)) != E_OK)
      return res;
    entry = getEntry(context, node->parentIndex);

    /* Check for the end of the directory */
    if (!entry->name[0])
      return E_ENTRY;

    /* Volume entries are ignored */
    if (!(entry->flags & FLAG_VOLUME) || (entry->flags & MASK_LFN) == MASK_LFN)
      break;
    ++node->parentIndex;
  }

  if (entry->name[0] != E_FLAG_EMPTY && (entry->flags & MASK_LFN) != MASK_LFN)
    node->flags = entry->flags & FLAG_DIR ? FAT_FLAG_DIR : FAT_FLAG_FILE;
  else
    node->flags = 0;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
/*
 * Fields handle, parentIndex and parentCluster in node have to be initialized.
 */
static enum result fetchNode(struct CommandContext *context,
    struct FatNode *node)
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
    entry = getEntry(context, node->parentIndex);

#ifdef CONFIG_FAT_UNICODE
    if (!(entry->ordinal & LFN_DELETED)
        && ((entry->flags & MASK_LFN) == MASK_LFN))
    {
      if (entry->ordinal & LFN_LAST)
      {
        found = 0;
        checksum = entry->checksum;
        chunks = entry->ordinal & ~LFN_LAST;
        node->nameIndex = node->parentIndex;
        node->nameCluster = node->parentCluster;
        node->nameLength = 0;
      }

      if (chunks)
        ++found;

      char16_t nameBuffer[LFN_ENTRY_LENGTH + 1];

      extractLongName(nameBuffer, entry);
      nameBuffer[LFN_ENTRY_LENGTH] = 0;
      node->nameLength += uLengthFromUtf16(nameBuffer);
    }
#endif

    if (node->flags & (FAT_FLAG_DIR | FAT_FLAG_FILE))
      break;
    ++node->parentIndex;
  }
  if (res != E_OK)
    return res;

  node->payloadCluster = makeClusterNumber(entry);
  node->payloadSize = entry->size;

  node->currentCluster = node->payloadCluster;
  node->payloadPosition = 0;

  if (entry->flags & FLAG_RO)
    node->flags |= FAT_FLAG_RO;

#ifdef CONFIG_FAT_UNICODE
  if (!found || found != chunks
      || checksum != getChecksum(entry->filename, NAME_LENGTH))
  {
    /* Wrong checksum or chunk count does not match */
    node->nameIndex = node->parentIndex;
    node->nameCluster = node->parentCluster;
    node->nameLength = computeShortNameLength(entry);
  }
#else
  node->nameLength = computeShortNameLength(entry);
#endif

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static uint32_t fileReadData(struct CommandContext *context,
    struct FatNode *node, uint32_t position, void *buffer, uint32_t length)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  uint32_t read = 0;
  uint16_t chunk;
  uint8_t current = 0;

  if (length > node->payloadSize - position)
    length = node->payloadSize - position;

  /* Seek to the requested position */
  if (position != node->payloadPosition)
  {
    if (fileSeekData(context, node, position) != E_OK)
      return 0;
  }

  /* Calculate current sector index in the cluster */
  if (position)
  {
    current = sectorInCluster(handle, node->payloadPosition);
    if (!current && !(node->payloadPosition & (SECTOR_SIZE - 1)))
      current = 1 << handle->clusterSize;
  }

  while (length)
  {
    if (current >= (1 << handle->clusterSize))
    {
      if (getNextCluster(context, handle, &node->currentCluster) != E_OK)
        return 0; /* Sector read error or end of file */

      current = 0;
    }

    /* Offset from the beginning of the sector */
    const uint16_t offset = (node->payloadPosition + read) & (SECTOR_SIZE - 1);

    if (offset || length < SECTOR_SIZE) /* Position within the sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = length < chunk ? length : chunk;

      const uint32_t sector = getSector(handle, node->currentCluster) + current;

      if (readSector(context, handle, sector) != E_OK)
        return 0;

      memcpy((uint8_t *)buffer + read, context->buffer + offset, chunk);

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_EXP);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Read data to the buffer directly without additional copying */
      if (readBuffer(handle, getSector(handle, node->currentCluster)
          + current, (uint8_t *)buffer + read, chunk >> SECTOR_EXP) != E_OK)
      {
        return 0;
      }

      current += chunk >> SECTOR_EXP;
    }

    read += chunk;
    length -= chunk;
  }

  node->payloadPosition += read;

  return read;
}
/*----------------------------------------------------------------------------*/
static enum result fileSeekData(struct CommandContext *context,
    struct FatNode *node, uint32_t offset)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  enum result res;

  uint32_t clusterCount;
  uint32_t current;

  if (offset > node->payloadPosition)
  {
    current = node->currentCluster;
    clusterCount = offset - node->payloadPosition;
  }
  else
  {
    current = node->payloadCluster;
    clusterCount = offset;
  }
  clusterCount >>= handle->clusterSize + SECTOR_EXP;

  while (clusterCount--)
  {
    if ((res = getNextCluster(context, handle, &current)) != E_OK)
      return res;
  }

  node->payloadPosition = offset;
  node->currentCluster = current;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void freeBuffers(struct FatHandle *handle, enum cleanup step)
{
  switch (step)
  {
    case FREE_ALL:
    case FREE_NODE_POOL:
#ifdef CONFIG_FAT_POOLS
      freePool(&handle->nodePool);
#endif
    case FREE_FILE_LIST:
#ifdef CONFIG_FAT_WRITE
      listDeinit(&handle->openedFiles);
#endif
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
  queuePush(&handle->contextPool.queue, &context);
  mutexUnlock(&handle->memoryMutex);
}
#else
static void freeContext(struct FatHandle *handle __attribute__((unused)),
    const struct CommandContext *context __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
static void freeNode(struct FatNode *node)
{
#ifdef CONFIG_FAT_POOLS
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  FatNode->deinit(node);

  lockPools(handle);
  queuePush(&handle->nodePool.queue, &node);
  unlockPools(handle);
#else
  deinit(node);
#endif
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
static enum result mountStorage(struct FatHandle *handle)
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
  if (fromBigEndian16(boot->bootSignature) != 0x55AA)
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
      + boot->tableCount * fromLittleEndian32(boot->tableSize);
  handle->rootCluster = fromLittleEndian32(boot->rootCluster);

  DEBUG_PRINT(0, "fat32: cluster size:   %u\n",
      (unsigned int)(1 << handle->clusterSize));
  DEBUG_PRINT(0, "fat32: table sector:   %u\n",
      (unsigned int)handle->tableSector);
  DEBUG_PRINT(0, "fat32: data sector:    %u\n",
      (unsigned int)handle->dataSector);

#ifdef CONFIG_FAT_WRITE
  handle->tableCount = boot->tableCount;
  handle->tableSize = fromLittleEndian32(boot->tableSize);
  handle->clusterCount = ((fromLittleEndian32(boot->partitionSize)
      - handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = fromLittleEndian16(boot->infoSector);

  DEBUG_PRINT(0, "fat32: info sector:    %u\n",
      (unsigned int)handle->infoSector);
  DEBUG_PRINT(0, "fat32: table copies:   %u\n",
      (unsigned int)handle->tableCount);
  DEBUG_PRINT(0, "fat32: table size:     %u\n",
      (unsigned int)handle->tableSize);
  DEBUG_PRINT(0, "fat32: cluster count:  %u\n",
      (unsigned int)handle->clusterCount);
  DEBUG_PRINT(0, "fat32: sectors count:  %u\n",
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

  DEBUG_PRINT(0, "fat32: free clusters:  %u\n",
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
static enum result rawTimestampToTime(time64_t *converted, uint16_t date,
    uint16_t time)
{
  const struct RtDateTime timestamp = {
      .second = time & 0x1F,
      .minute = (time >> 5) & 0x3F,
      .hour = (time >> 11) & 0x1F,
      .day = date & 0x1F,
      .month = (date >> 5) & 0x0F,
      .year = ((date >> 9) & 0x7F) + 1980
  };

  return rtMakeEpochTime(converted, &timestamp);
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
    uint32_t length, const struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FatNode allocatedNode;
  enum result res;

  if (length > CONFIG_FILENAME_LENGTH)
  {
    /* Size of the temporary buffer is greater than allowed by configuration */
    return E_VALUE;
  }
  if (length <= node->nameLength)
  {
    /* Buffer is shorter than entry name */
    return E_MEMORY;
  }

  /* Initialize temporary data */
  if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
    return res;

  allocatedNode.parentCluster = node->nameCluster;
  allocatedNode.parentIndex = node->nameIndex;

  const uint8_t maxChunks = (length - 1) / 2 / LFN_ENTRY_LENGTH;
  uint8_t chunks = 0;
  char nameBuffer[length];

  while ((res = fetchEntry(context, &allocatedNode)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    const struct DirEntryImage * const entry = getEntry(context,
        allocatedNode.parentIndex);

    if ((entry->flags & MASK_LFN) != MASK_LFN)
      break;

    const uint16_t offset = entry->ordinal & ~LFN_LAST;

    /* Compare resulting file name length and name buffer capacity */
    if (offset > maxChunks)
    {
      res = E_MEMORY;
      break;
    }

    if (entry->ordinal & LFN_LAST)
      *((char16_t *)nameBuffer + offset * LFN_ENTRY_LENGTH) = 0;

    extractLongName((char16_t *)nameBuffer + (offset - 1) * LFN_ENTRY_LENGTH,
        entry);
    ++chunks;
    ++allocatedNode.parentIndex;
  }

  /*
   * Long file name entries always precede data entry thus
   * processing of return values others than success is not needed.
   */
  if (res == E_OK)
  {
    if (chunks)
    {
      uFromUtf16(name, (const char16_t *)nameBuffer, sizeof(nameBuffer));
    }
    else
    {
      res = E_ENTRY;
    }
  }

  freeStaticNode(&allocatedNode);
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

      DEBUG_PRINT(1, "fat32: allocated cluster: %u, parent %u\n",
          current, *cluster);
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

  DEBUG_PRINT(0, "fat32: allocation error, partition may be full\n");
  return E_FULL;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result clearCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t sector = getSector(handle, cluster + 1);

  memset(context->buffer, 0, SECTOR_SIZE);

  do
  {
    const enum result res = writeSector(context, handle, --sector);

    if (res != E_OK)
      return res;
  }
  while (sector & ((1 << handle->clusterSize) - 1));

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static void clearDirtyFlag(struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct ListNode *current = listFirst(&handle->openedFiles);
  struct FatNode *descriptor;

  while (current)
  {
    listData(&handle->openedFiles, current, &descriptor);
    if (descriptor == node)
    {
      listErase(&handle->openedFiles, current);
      break;
    }
    current = listNext(current);
  }

  node->flags &= ~FAT_FLAG_DIRTY;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result createNode(struct CommandContext *context,
    const struct FatNode *root, bool directory, const char *nodeName,
    access_t nodeAccess, uint32_t nodePayloadCluster, time64_t nodeAccessTime)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  char shortName[NAME_LENGTH];
  uint8_t chunks = 0;
  enum result res;

  res = fillShortName(shortName, nodeName, !directory);
  if (res != E_OK && res != E_VALUE)
    return res;

#ifdef CONFIG_FAT_UNICODE
  const uint16_t nameLength = uLengthToUtf16(nodeName) + 1;
  const bool longNameRequired = res == E_VALUE;

  if (nameLength > CONFIG_FILENAME_LENGTH / 2)
    return E_VALUE;

  char16_t nameBuffer[nameLength];
#endif

  /* Propose new short name when selected name already exists */
  if ((res = uniqueNamePropose(context, root, shortName)) != E_OK)
    return res;

#ifdef CONFIG_FAT_UNICODE
  if (longNameRequired)
  {
    uToUtf16(nameBuffer, nodeName, nameLength);
    chunks = (nameLength - 1) / LFN_ENTRY_LENGTH;
    /* Append additional entry when last chunk is incomplete */
    if ((nameLength - 1) > chunks * LFN_ENTRY_LENGTH)
      ++chunks;
  }
#endif

  struct FatNode node;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &node)) != E_OK)
    return res;
  /* Find suitable space within the directory */
  res = findGap(context, &node, root, chunks + 1);

#ifdef CONFIG_FAT_UNICODE
  if (chunks && res == E_OK)
  {
    /* Save start cluster and index values before filling the chain */
    node.nameCluster = node.parentCluster;
    node.nameIndex = node.parentIndex;

    const uint8_t checksum = getChecksum(shortName, NAME_LENGTH);

    for (uint8_t current = chunks; current; --current)
    {
      const uint32_t sector = getSector(handle, node.parentCluster)
          + ENTRY_SECTOR(node.parentIndex);

      if ((res = readSector(context, handle, sector)) != E_OK)
        break;

      struct DirEntryImage * const entry = getEntry(context, node.parentIndex);
      const uint16_t offset = (current - 1) * LFN_ENTRY_LENGTH;
      const uint16_t left = nameLength - 1 - offset;

      fillLongName(entry, nameBuffer + offset,
          left > LFN_ENTRY_LENGTH ? LFN_ENTRY_LENGTH : left);
      fillLongNameEntry(entry, current, chunks, checksum);

      ++node.parentIndex;

      if (!(node.parentIndex & (ENTRY_EXP - 1)))
      {
        /* Write back updated sector when switching sectors */
        if ((res = writeSector(context, handle, sector)) != E_OK)
          break;
      }

      if ((res = fetchEntry(context, &node)) == E_ENTRY)
        res = E_OK;

      if (res != E_OK)
        break;
    }
  }
#endif

  if (res == E_OK)
  {
    struct DirEntryImage * const entry = getEntry(context, node.parentIndex);

    memcpy(entry->filename, shortName, NAME_LENGTH);
    fillDirEntry(entry, directory, nodeAccess, nodePayloadCluster,
        nodeAccessTime);

    /* Buffer is already filled with actual sector data */
    const uint32_t sector = getSector(handle, node.parentCluster)
        + ENTRY_SECTOR(node.parentIndex);

    freeStaticNode(&node);
    res = writeSector(context, handle, sector);
  }

  return res;
}
#endif
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
#ifdef CONFIG_FAT_WRITE
static uint32_t fileWriteData(struct CommandContext *context,
    struct FatNode *node, uint32_t position, const void *buffer,
    uint32_t length)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  uint32_t written = 0;
  uint16_t chunk;
  uint8_t current = 0; /* Current sector of the data cluster */
  enum result res;

  if (node->payloadCluster == RESERVED_CLUSTER)
  {
    /* Lock handle to prevent table modifications from other threads */
    lockHandle(handle);
    res = allocateCluster(context, handle, &node->payloadCluster);
    unlockHandle(handle);

    if (res != E_OK)
      return 0;

    node->currentCluster = node->payloadCluster;
  }

  /* Checking file size limit (4 GiB - 1) */
  if (length > FILE_SIZE_MAX - node->payloadSize)
    length = FILE_SIZE_MAX - node->payloadSize;

  /* Seek to the requested position */
  if (position != node->payloadPosition)
  {
    if (fileSeekData(context, node, position) != E_OK)
      return 0;
  }

  /* Calculate current sector index in the cluster */
  if (position)
  {
    current = sectorInCluster(handle, node->payloadPosition);
    if (!current && !(node->payloadPosition & (SECTOR_SIZE - 1)))
      current = 1 << handle->clusterSize;
  }

  if (!(node->flags & FAT_FLAG_DIRTY))
  {
    lockHandle(handle);
    res = listPush(&handle->openedFiles, &node);
    unlockHandle(handle);

    if (res != E_OK)
      return 0;

    node->flags |= FAT_FLAG_DIRTY;
  }

  while (length)
  {
    if (current >= (1 << handle->clusterSize))
    {
      /* Allocate new cluster when next cluster does not exist */
      res = getNextCluster(context, handle, &node->currentCluster);

      if (res == E_EMPTY)
      {
        /* Prevent table modifications from other threads */
        lockHandle(handle);
        res = allocateCluster(context, handle, &node->currentCluster);
        unlockHandle(handle);
      }

      if (res != E_OK)
        return 0;

      current = 0;
    }

    /* Offset from the beginning of the sector */
    const uint16_t offset = (node->payloadPosition + written)
        & (SECTOR_SIZE - 1);

    if (offset || length < SECTOR_SIZE) /* Position within the sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (length < chunk) ? length : chunk;

      const uint32_t sector = getSector(handle, node->currentCluster) + current;

      if (readSector(context, handle, sector) != E_OK)
        return 0;

      memcpy(context->buffer + offset, (const uint8_t *)buffer + written,
          chunk);
      if (writeSector(context, handle, sector) != E_OK)
        return 0;

      if (chunk + offset >= SECTOR_SIZE)
        ++current;
    }
    else /* Position is aligned along the first byte of the sector */
    {
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_EXP);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Write data from the buffer directly without additional copying */
      if (writeBuffer(handle, getSector(handle, node->currentCluster) + current,
          (const uint8_t *)buffer + written, chunk >> SECTOR_EXP) != E_OK)
      {
        return 0;
      }

      current += chunk >> SECTOR_EXP;
    }

    written += chunk;
    length -= chunk;
  }

  node->payloadPosition += written;
  if (node->payloadPosition > node->payloadSize)
    node->payloadSize = node->payloadPosition;

  return written;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static void fillDirEntry(struct DirEntryImage *entry, bool directory,
    access_t access, uint32_t payloadCluster, time64_t accessTime)
{
  /* Clear unused fields */
  entry->unused0 = 0;
  entry->unused1 = 0;
  memset(entry->unused2, 0, sizeof(entry->unused2));

  entry->flags = 0;
  if (directory)
    entry->flags |= FLAG_DIR;
  if (!(access & FS_ACCESS_WRITE))
    entry->flags |= FLAG_RO;

  entry->clusterHigh = toLittleEndian16((uint16_t)(payloadCluster >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)payloadCluster);
  entry->size = 0;

#ifdef CONFIG_FAT_TIME
  struct RtDateTime currentTime;

  rtMakeTime(&currentTime, accessTime);
  entry->date = toLittleEndian16(timeToRawDate(&currentTime));
  entry->time = toLittleEndian16(timeToRawTime(&currentTime));
#else
  /* Suppress warning */
  (void)accessTime;

  entry->date = 0;
  entry->time = 0;
#endif
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
/* Returns success when node has a valid short name */
static enum result fillShortName(char *shortName, const char *name,
    bool extension)
{
  const char *dot;
  enum result res = E_OK;

  /* Find start position of the extension */
  const uint16_t length = strlen(name);

  if (!length)
    return E_ERROR;

  if (extension)
  {
    for (dot = name + length - 1; dot >= name && *dot != '.'; --dot);
  }

  if (!extension || dot < name)
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

  memset(shortName, ' ', NAME_LENGTH);
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
  uint32_t parent = root->payloadCluster;
  uint16_t index = 0;
  uint8_t chunks = 0;
  enum result res;

  node->parentCluster = root->payloadCluster;
  node->parentIndex = 0;

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
        index = node->parentIndex;
        parent = node->parentCluster;
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
          - (int16_t)(nodeCount(handle) - node->parentIndex);

      while (chunksLeft > 0)
      {
        res = allocateCluster(context, handle, &node->parentCluster);
        if (res != E_OK)
          return res;
        res = clearCluster(context, handle, node->parentCluster);
        if (res != E_OK)
        {
          /* Directory may contain erroneous entries when clearing fails */
          return res;
        }

        if (allocateParent)
        {
          /* Place new entry in the beginning of the allocated cluster */
          parent = node->parentCluster;
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
    const struct DirEntryImage * const entry = getEntry(context,
        node->parentIndex);

    /* Empty node, deleted long file name node or deleted node */
    if (!entry->name[0] || ((entry->flags & MASK_LFN) == MASK_LFN
        && (entry->ordinal & LFN_DELETED)) || entry->name[0] == E_FLAG_EMPTY)
    {
      if (!chunks) /* When first free node found */
      {
        index = node->parentIndex;
        parent = node->parentCluster;
      }
      ++chunks;
    }
    else
      chunks = 0;

    ++node->parentIndex;
  }

  node->parentCluster = parent;
  node->parentIndex = index;
  node->flags = 0;

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
    DEBUG_PRINT(1, "fat32: cleared cluster: %u\n", current);
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
  allocatedNode.parentCluster = node->nameCluster;
  allocatedNode.parentIndex = node->nameIndex;
#endif

  const uint32_t lastSector = getSector(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);

  while ((res = fetchEntry(context, &allocatedNode)) == E_OK)
  {
    const uint32_t sector = getSector(handle, allocatedNode.parentCluster)
        + ENTRY_SECTOR(allocatedNode.parentIndex);
    const bool lastEntry = sector == lastSector
        && allocatedNode.parentIndex == node->parentIndex;

    /* Sector is already loaded */
    struct DirEntryImage * const entry = getEntry(context,
        allocatedNode.parentIndex);

    /* Mark entry as empty by changing first byte of the name */
    entry->name[0] = E_FLAG_EMPTY;

    if (lastEntry || !(allocatedNode.parentIndex & (ENTRY_EXP - 1)))
    {
      /* Write back updated sector when switching sectors or last entry freed */
      if ((res = writeSector(context, handle, sector)) != E_OK)
        break;
    }

    ++allocatedNode.parentIndex;

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
  static const char forbidden[] = {
      0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x5B, 0x5C, 0x5D, 0x7C
  };

  /* Convert lower case characters to upper case */
  if (value >= 'a' && value <= 'z')
    return value - ('a' - 'A');
  /* Remove spaces */
  if (value == ' ')
    return 0;
  if (value > 0x20 && value < 0x7F && !(value >= 0x3A && value <= 0x3F))
  {
    bool found = false;

    for (uint8_t index = 0; index < sizeof(forbidden); ++index)
    {
      if (value == forbidden[index])
      {
        found = true;
        break;
      }
    }
    if (!found)
      return value;
  }

  /* Replace all other characters with underscore */
  return '_';
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result truncatePayload(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  enum result res;

  if ((node->flags & FAT_FLAG_DIR) && node->payloadCluster != RESERVED_CLUSTER)
  {
    struct FatNode allocatedNode;

    /* Initialize temporary data */
    if ((res = allocateStaticNode(handle, &allocatedNode)) != E_OK)
      return res;

    allocatedNode.parentCluster = node->payloadCluster;
    allocatedNode.parentIndex = 0;

    /* Check whether the directory is not empty */
    while ((res = fetchNode(context, &allocatedNode)) == E_OK)
    {
      const struct DirEntryImage * const entry = getEntry(context,
          allocatedNode.parentIndex);
      char nameBuffer[BASENAME_LENGTH + 1];

      extractShortBasename(nameBuffer, entry->name);
      if (strcmp(nameBuffer, ".") && strcmp(nameBuffer, ".."))
      {
        res = E_EXIST;
        break;
      }

      ++allocatedNode.parentIndex;
    }

    freeStaticNode(&allocatedNode);

    if (res != E_EMPTY && res != E_ENTRY)
      return res;
  }

  /* Mark clusters as free */
  lockHandle(handle);
  res = freeChain(context, handle, node->payloadCluster);
  unlockHandle(handle);

  if (res == E_OK)
    node->payloadCluster = RESERVED_CLUSTER;

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static unsigned int uniqueNameConvert(char *shortName)
{
  unsigned int nameIndex = 0;
  uint8_t end = 0;
  uint8_t start = 0;

  for (uint8_t position = strlen(shortName) - 1; position; --position)
  {
    const char value = shortName[position];

    if (value == '~')
    {
      start = position++;

      while (position <= end)
      {
        const uint8_t currentNumber = shortName[position] - '0';

        nameIndex = (nameIndex * 10) + (unsigned int)currentNumber;
        ++position;
      }
      break;
    }
    else if (value >= '0' && value <= '9')
    {
      if (!end)
        end = position;
    }
    else
    {
      break;
    }
  }

  if (nameIndex)
    shortName[start] = '\0';

  return nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result uniqueNamePropose(struct CommandContext *context,
    const struct FatNode *root, char *shortName)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  struct FatNode node;
  char currentName[BASENAME_LENGTH + 1];
  enum result res;

  /* Initialize temporary node */
  if ((res = allocateStaticNode(handle, &node)) != E_OK)
    return res;

  extractShortBasename(currentName, shortName);
  node.parentCluster = root->payloadCluster;
  node.parentIndex = 0;

  unsigned int proposed = 0;

  while ((res = fetchEntry(context, &node)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    const struct DirEntryImage * const entry = getEntry(context,
        node.parentIndex);

    if (entry->name[0] == E_FLAG_EMPTY || (entry->flags & MASK_LFN) == MASK_LFN)
    {
      ++node.parentIndex;
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

    ++node.parentIndex;
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

    DEBUG_PRINT(1, "fat32: proposed short name: \"%.8s\"\n", shortName);

    res = E_OK;
  }
  else
  {
    res = E_EXIST;
  }

  freeStaticNode(&node);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result setupDirCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t parentCluster, uint32_t payloadCluster,
    time64_t accessTime)
{
  enum result res;

  if ((res = clearCluster(context, handle, payloadCluster)) != E_OK)
    return res;

  struct DirEntryImage *entry = getEntry(context, 0);

  /* Current directory entry . */
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = '.';
  fillDirEntry(entry, true, FS_ACCESS_READ | FS_ACCESS_WRITE,
      payloadCluster, accessTime);

  /* Parent directory entry .. */
  ++entry;
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = entry->name[1] = '.';
  fillDirEntry(entry, true, FS_ACCESS_READ | FS_ACCESS_WRITE,
      payloadCluster, accessTime);
  if (parentCluster != handle->rootCluster)
  {
    entry->clusterHigh = toLittleEndian16((uint16_t)(payloadCluster >> 16));
    entry->clusterLow = toLittleEndian16((uint16_t)payloadCluster);
  }
  else
    entry->clusterLow = entry->clusterHigh = 0;

  res = writeSector(context, handle, getSector(handle, payloadCluster));
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result syncFile(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = getSector(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  enum result res;

  /* Lock handle to prevent directory modifications from other threads */
  lockHandle(handle);
  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    unlockHandle(handle);
    return res;
  }

  /* Pointer to entry position in sector */
  struct DirEntryImage * const entry = getEntry(context, node->parentIndex);

  /* Update first cluster when writing to empty file or truncating file */
  entry->clusterHigh = toLittleEndian16((uint16_t)(node->payloadCluster >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)node->payloadCluster);
  /* Update file size */
  entry->size = toLittleEndian32(node->payloadSize);

#ifdef CONFIG_FAT_TIME
  if (handle->clock)
  {
    struct RtDateTime currentTime;

    rtMakeTime(&currentTime, rtTime(handle->clock));
    entry->date = toLittleEndian16(timeToRawDate(&currentTime));
    entry->time = toLittleEndian16(timeToRawTime(&currentTime));
  }
#endif

  res = writeSector(context, handle, sector);
  unlockHandle(handle);

  if (res == E_OK)
    clearDirtyFlag(node);

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
#if defined(CONFIG_FAT_TIME) && defined(CONFIG_FAT_WRITE)
static uint16_t timeToRawDate(const struct RtDateTime *value)
{
  return value->day | (value->month << 5) | ((value->year - 1980) << 9);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_FAT_TIME) && defined(CONFIG_FAT_WRITE)
static uint16_t timeToRawTime(const struct RtDateTime *value)
{
  return (value->second >> 1) | (value->minute << 5) | (value->hour << 11);
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
static void fillLongName(struct DirEntryImage *entry, const char16_t *name,
    uint8_t count)
{
  char16_t buffer[LFN_ENTRY_LENGTH];

  memcpy(buffer, name, count);
  if (count < LFN_ENTRY_LENGTH)
    memset(buffer + count, 0, LFN_ENTRY_LENGTH - count);

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
  handle->clock = config->clock;
  if (!handle->clock)
    DEBUG_PRINT(0, "fat32: real-time clock is not initialized\n");
#endif

  if ((res = mountStorage(handle)) != E_OK)
  {
    freeBuffers(handle, FREE_ALL);
    return res;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatHandleDeinit(void *object)
{
  struct FatHandle * const handle = object;

  freeBuffers(handle, FREE_ALL);
}
/*----------------------------------------------------------------------------*/
static void *fatHandleRoot(void *object)
{
  struct FatHandle * const handle = object;
  struct FatNode *node;

  if (!(node = allocateNode(handle)))
    return 0;

  /* Resulting node is the root node */
  node->parentCluster = RESERVED_CLUSTER;
  node->parentIndex = 0;

  node->payloadCluster = handle->rootCluster;
  node->currentCluster = node->payloadCluster;

  node->flags = FAT_FLAG_DIR;

  return node;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatHandleSync(void *object)
{
  struct FatHandle * const handle = object;
  struct CommandContext *context;
  struct FatNode *node;
  enum result res = E_OK;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;
  lockHandle(handle);

  const struct ListNode *current = listFirst(&handle->openedFiles);

  while (current)
  {
    listData(&handle->openedFiles, current, &node);
    if ((res = syncFile(context, node)) != E_OK)
      break;
    current = listNext(current);
  }

  unlockHandle(handle);
  freeContext(handle, context);

  return res;
}
#else
static enum result fatHandleSync(void *object __attribute__((unused)))
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

  node->parentCluster = RESERVED_CLUSTER;
  node->parentIndex = 0;
#ifdef CONFIG_FAT_UNICODE
  node->nameCluster = 0;
  node->nameIndex = 0;
#endif

  node->payloadSize = 0;
  node->payloadCluster = RESERVED_CLUSTER;

  node->currentCluster = RESERVED_CLUSTER;
  node->payloadPosition = 0;
  node->nameLength = 0;

  node->flags = 0;

  DEBUG_PRINT(2, "fat32: node allocated, address %08lX\n",
      (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatNodeDeinit(void *object)
{
  struct FatNode * const node = object;

  if (node->flags & FAT_FLAG_DIRTY)
  {
#ifdef CONFIG_FAT_WRITE
    struct FatHandle * const handle = (struct FatHandle *)node->handle;
    struct CommandContext *context;
    enum result res = E_MEMORY;

    if ((context = allocateContext(handle)))
    {
      res = syncFile(context, node);
      freeContext(handle, context);
    }

    if (res != E_OK)
    {
      DEBUG_PRINT(0, "fat32: unrecoverable sync error\n");

      /* Clear flag and remove from the list anyway */
      clearDirtyFlag(node);
      return;
    }
#endif
  }

  DEBUG_PRINT(2, "fat32: node freed, address %08lX\n", (unsigned long)object);
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatNodeCreate(void *object,
    const struct FsFieldDescriptor *descriptors, uint8_t number)
{
  const struct FatNode * const parent = object;
  struct FatHandle * const handle = (struct FatHandle *)parent->handle;
  struct CommandContext *context;

  if (!(parent->flags & FAT_FLAG_DIR))
    return E_VALUE;
  if (parent->flags & FAT_FLAG_RO)
    return E_ACCESS;

  time64_t nodeTime = 0;
  access_t nodeAccess = FS_ACCESS_READ | FS_ACCESS_WRITE;

  const struct FsFieldDescriptor *nameDesc = 0;
  const struct FsFieldDescriptor *payloadDesc = 0;

  for (uint8_t index = 0; index < number; ++index)
  {
    const struct FsFieldDescriptor * const desc = descriptors + index;

    switch (desc->type)
    {
      case FS_NODE_ACCESS:
        if (desc->length != sizeof(access_t))
          return E_VALUE;
        memcpy(&nodeAccess, desc->data, sizeof(access_t));
        break;

      case FS_NODE_DATA:
        payloadDesc = desc;
        break;

      case FS_NODE_NAME:
        if (desc->length > CONFIG_FILENAME_LENGTH
            || desc->length != strlen(desc->data) + 1)
        {
          return E_VALUE;
        }
        nameDesc = desc;
        break;

      case FS_NODE_TIME:
        if (desc->length != sizeof(time64_t))
          return E_VALUE;
        memcpy(&nodeTime, desc->data, sizeof(time64_t));
        break;

      default:
        break;
    }
  }

  if (!nameDesc)
    return E_INVALID; /* Node cannot be left unnamed */

#ifdef CONFIG_FAT_TIME
  if (!nodeTime && handle->clock)
    nodeTime = rtTime(handle->clock); //FIXME Check whether user time is 0
#endif

  const bool directory = payloadDesc == 0;
  uint32_t nodePayloadCluster = RESERVED_CLUSTER;
  enum result res = E_OK;

  if ((context = allocateContext(handle)))
  {
    /* Prevent unexpected modifications from other threads */
    lockHandle(handle);

    /* Allocate a cluster chain for the directory */
    if (directory)
    {
      res = allocateCluster(context, handle, &nodePayloadCluster);
      if (res == E_OK)
      {
        /*
         * Coherence checking is not needed during setup of the first cluster
         * because the directory entry will be created on the next step.
         */
        unlockHandle(handle);
        res = setupDirCluster(context, handle, parent->payloadCluster,
            nodePayloadCluster, nodeTime);
        lockHandle(handle);

        if (res != E_OK)
          freeChain(context, handle, nodePayloadCluster);
      }
    }

    /* Create an entry in the parent directory */
    if (res == E_OK)
    {
      res = createNode(context, parent, directory, nameDesc->data, nodeAccess,
          nodePayloadCluster, nodeTime);

      if (res != E_OK && directory)
        freeChain(context, handle, nodePayloadCluster);
    }

    unlockHandle(handle);
    freeContext(handle, context);
  }
  else
    res = E_MEMORY;

  return res;
}
#else
static enum result fatNodeCreate(void *object __attribute__((unused)),
    const struct FsFieldDescriptor *descriptors __attribute__((unused)),
    uint8_t count __attribute__((unused)))
{
  return E_INVALID;
}
#endif
/*----------------------------------------------------------------------------*/
static void *fatNodeHead(void *object)
{
  struct FatNode * const parent = object;
  struct FatHandle * const handle = (struct FatHandle *)parent->handle;
  struct CommandContext *context;
  struct FatNode *node;
  enum result res;

  if (!(parent->flags & FAT_FLAG_DIR))
    return 0; /* Current node is not directory */
  if (parent->currentCluster == RESERVED_CLUSTER)
    return 0; /* Iterator reached the end of the parent directory */

  if (!(node = allocateNode(handle)))
    return 0;

  node->parentCluster = parent->payloadCluster;
  node->parentIndex = 0;

  if ((context = allocateContext(handle)))
  {
    res = fetchNode(context, node);
    freeContext(handle, context);
  }
  else
  {
    res = E_MEMORY;
  }

  if (res != E_OK)
  {
    freeNode(node);
    return 0;
  }
  else
  {
    return node;
  }
}
/*----------------------------------------------------------------------------*/
static void fatNodeFree(void *object)
{
  struct FatNode * const node = object;

  freeNode(node);
}
/*----------------------------------------------------------------------------*/
static enum result fatNodeLength(void *object, enum fsFieldType type,
    length_t *length)
{
  struct FatNode * const node = object;

  switch (type)
  {
    case FS_NODE_ACCESS:
      if (length)
        *length = sizeof(access_t);
      break;

    case FS_NODE_DATA:
      if (node->flags & FAT_FLAG_FILE)
      {
        if (length)
          *length = node->payloadSize;
      }
      else
        return E_INVALID;
      break;

    case FS_NODE_ID:
      if (length)
        *length = sizeof(uint64_t);
      break;

    case FS_NODE_NAME:
      if (length)
        *length = node->nameLength + 1;
      break;

    case FS_NODE_TIME:
      if (length)
        *length = sizeof(time64_t);
      break;

    default:
      return E_INVALID;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result fatNodeNext(void *object)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (node->parentCluster == RESERVED_CLUSTER)
    return E_ENTRY;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  ++node->parentIndex;
  res = fetchNode(context, node);

  freeContext(handle, context);

  if (res != E_OK)
  {
    if (res == E_EMPTY || res == E_ENTRY)
    {
      /* Reached the end of the directory */
      node->parentCluster = RESERVED_CLUSTER;
      node->parentIndex = 0;
      return E_ENTRY;
    }
    else
      return res;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result fatNodeRead(void *object, enum fsFieldType type,
    length_t position, void *buffer, length_t length, length_t *read)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  length_t bytesRead = 0;
  enum result res = E_OK;

  /* Read fields that do not require reading from the interface */
  switch (type)
  {
    case FS_NODE_ACCESS:
    {
      if (position)
        return E_INVALID;
      if (length < sizeof(access_t))
        return E_VALUE;

      access_t value = FS_ACCESS_READ;

      if (!(node->flags & FAT_FLAG_RO))
        value |= FS_ACCESS_WRITE;
      memcpy(buffer, &value, sizeof(value));
      bytesRead = sizeof(value);
      break;
    }

    case FS_NODE_ID:
    {
      if (position)
        return E_INVALID;
      if (length < sizeof(uint64_t))
        return E_VALUE;

      uint64_t nodeId = (uint64_t)node->parentIndex
          | ((uint64_t)node->parentCluster << 16);

      memcpy(buffer, &nodeId, sizeof(nodeId));
      bytesRead = sizeof(nodeId);
      break;
    }

    default:
      break;
  }

  if (res != E_OK)
    return res;
  if (bytesRead)
  {
    if (read)
      *read = bytesRead;
    return E_OK;
  }

  /* Read fields that require reading from the interface */
  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  switch (type)
  {
    case FS_NODE_DATA:
    {
      if (!(node->flags & FAT_FLAG_FILE))
      {
        res = E_INVALID;
        break;
      }
      if (node->flags & FAT_FLAG_RO)
      {
        res = E_ACCESS;
        break;
      }
      if (position > node->payloadSize)
      {
        res = E_VALUE;
        break;
      }
      if (!length)
        break;

      bytesRead = fileReadData(context, node, position, buffer, length);
      if (!bytesRead)
        res = E_EMPTY;
      break;
    }

#ifdef CONFIG_FAT_UNICODE
    case FS_NODE_NAME:
    {
      if (position)
        return E_INVALID;
      if (!hasLongName(node))
        break;
      if (length <= node->nameLength)
      {
        res = E_VALUE;
        break;
      }

      if ((res = readLongName(context, buffer, length, node)) == E_OK)
      {
        /* Include terminating character */
        bytesRead = node->nameLength + 1;
      }
      break;
    }
#endif

    default:
      break;
  }

  if (res != E_OK)
  {
    freeContext(handle, context);
    return res;
  }
  if (bytesRead)
  {
    if (read)
      *read = bytesRead;
    freeContext(handle, context);
    return E_OK;
  }

  /* Read fields that require access to the directory entry */
  const uint32_t sector = getSector(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);

  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    freeContext(handle, context);
    return res;
  }

  const struct DirEntryImage * const entry =
      getEntry(context, node->parentIndex);

  switch (type)
  {
    case FS_NODE_NAME:
    {
      /* Check whether the buffer has enough space for names in 8.3 format */
      if (length <= node->nameLength)
      {
        res = E_VALUE;
        break;
      }

      extractShortName(buffer, entry);
      /* Include terminating character */
      bytesRead = node->nameLength + 1;
      break;
    }

#ifdef CONFIG_FAT_TIME
    case FS_NODE_TIME:
    {
      if (position)
        return E_INVALID;
      if (length < sizeof(time64_t))
      {
        res = E_VALUE;
        break;
      }

      const uint16_t rawDate = fromLittleEndian16(entry->date);
      const uint16_t rawTime = fromLittleEndian16(entry->time);

      if ((res = rawTimestampToTime(buffer, rawDate, rawTime)) == E_OK)
        bytesRead = sizeof(time64_t);
      break;
    }
#endif

    default:
      res = E_INVALID;
      break;
  }

  freeContext(handle, context);

  if (res == E_OK && read)
    *read = bytesRead;

  return res;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatNodeRemove(void *parentObject, void *object)
{
  struct FatNode * const parent = parentObject;
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)parent->handle;
  struct CommandContext *context;
  enum result res;

  if ((parent->flags & FAT_FLAG_RO) || (node->flags & FAT_FLAG_RO))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  res = truncatePayload(context, node);

  if (res == E_OK)
  {
    lockHandle(handle);
    res = markFree(context, node);
    unlockHandle(handle);
  }

  freeContext(handle, context);

  return res;
}
#else
static enum result fatNodeRemove(void *parentObject __attribute__((unused)),
    void *object __attribute__((unused)))
{
  return E_INVALID;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_FAT_WRITE
static enum result fatNodeWrite(void *object, enum fsFieldType type,
    length_t position, const void *buffer, length_t length, length_t *written)
{
  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  length_t bytesWritten = 0;
  enum result res = E_OK;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  switch (type)
  {
    case FS_NODE_DATA:
    {
      if (!(node->flags & FAT_FLAG_FILE))
      {
        res = E_INVALID;
        break;
      }
      if (node->flags & FAT_FLAG_RO)
      {
        res = E_ACCESS;
        break;
      }
      if (position > node->payloadSize)
      {
        res = E_VALUE;
        break;
      }
      if (!length)
        break;

      bytesWritten = fileWriteData(context, node, position, buffer, length);
      if (!bytesWritten)
        res = E_EMPTY;
      break;
    }

    default:
      res = E_INVALID;
      break;
  }

  freeContext(handle, context);

  if (res == E_OK && written)
    *written = bytesWritten;

  return res;
}
#else
static enum result fatNodeWrite(void *object __attribute__((unused)),
    enum fsFieldType type __attribute__((unused)),
    length_t position __attribute__((unused)),
    const void *buffer __attribute__((unused)),
    length_t length __attribute__((unused)),
    length_t *written __attribute__((unused)))
{
  return E_INVALID;
}
#endif
