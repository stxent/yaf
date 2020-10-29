/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <yaf/debug.h>
#include <yaf/fat32.h>
#include <yaf/fat32_defs.h>
#include <yaf/fat32_inlines.h>
#include <xcore/bits.h>
#include <xcore/memory.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
enum Cleanup
{
  FREE_ALL,
  FREE_NODE_POOL,
  FREE_FILE_LIST,
  FREE_LOCKS
};
/*----------------------------------------------------------------------------*/
static enum Result allocateBuffers(struct FatHandle *,
    const struct Fat32Config * const);
static struct CommandContext *allocateContext(struct FatHandle *);
static void *allocateNode(struct FatHandle *);
static unsigned int computeShortNameLength(const struct DirEntryImage *);
static void extractShortName(char *, const struct DirEntryImage *);
static enum Result fetchEntry(struct CommandContext *, struct FatNode *);
static enum Result fetchNode(struct CommandContext *, struct FatNode *);
static void freeBuffers(struct FatHandle *, enum Cleanup);
static void freeContext(struct FatHandle *, struct CommandContext *);
static void freeNode(struct FatNode *);
static enum Result getNextCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum Result mountStorage(struct FatHandle *);
static enum Result rawDateTimeToTimestamp(time64_t *, uint16_t, uint16_t);
static enum Result readBuffer(struct FatHandle *, uint32_t, uint8_t *,
    uint32_t);
static enum Result readClusterChain(struct CommandContext *,
    struct FatNode *, uint32_t, uint8_t *, uint32_t);
static void readNodeAccess(struct FatNode *, void *);
static void readNodeId(struct FatNode *, void *);
static enum Result readNodeData(struct CommandContext *, struct FatNode *,
    FsLength, void *, size_t, size_t *);
static enum Result readNodeName(struct CommandContext *, struct FatNode *,
    void *, size_t, size_t *);
static enum Result readNodeTime(struct CommandContext *, struct FatNode *,
    void *);
static enum Result readSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum Result seekClusterChain(struct CommandContext *, struct FatHandle *,
    uint32_t, uint32_t, uint32_t, uint32_t *);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
static void extractLongName(char16_t *, const struct DirEntryImage *);
static uint8_t getChecksum(const char *, size_t);
static enum Result readLongName(struct CommandContext *, char *, size_t,
    const struct FatNode *);
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result allocateCluster(struct CommandContext *, struct FatHandle *,
    uint32_t *);
static enum Result clearCluster(struct CommandContext *, struct FatHandle *,
    uint32_t);
static void clearDirtyFlag(struct FatNode *);
static enum Result createNode(struct CommandContext *, const struct FatNode *,
    bool, const char *, FsAccess, uint32_t, time64_t);
static void extractShortBasename(char *, const char *);
static void fillDirEntry(struct DirEntryImage *, bool, FsAccess, uint32_t,
    time64_t);
static enum Result fillShortName(char *, const char *, bool);
static enum Result findGap(struct CommandContext *, struct FatNode *,
    const struct FatNode *, unsigned int);
static enum Result freeChain(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum Result markFree(struct CommandContext *, const struct FatNode *);
static char processCharacter(char);
static enum Result setupDirCluster(struct CommandContext *, struct FatHandle *,
    uint32_t, uint32_t, time64_t);
static enum Result syncFile(struct CommandContext *, struct FatNode *);
static uint16_t timeToRawDate(const struct RtDateTime *);
static uint16_t timeToRawTime(const struct RtDateTime *);
static enum Result truncatePayload(struct CommandContext *, struct FatNode *);
static unsigned int uniqueNameConvert(char *);
static enum Result uniqueNamePropose(struct CommandContext *,
    const struct FatNode *, char *);
static enum Result updateTable(struct CommandContext *, struct FatHandle *,
    uint32_t);
static enum Result writeBuffer(struct FatHandle *, uint32_t, const uint8_t *,
    uint32_t);
static enum Result writeClusterChain(struct CommandContext *,
    struct FatNode *, uint32_t, const uint8_t *, uint32_t);
static enum Result writeNodeAccess(struct CommandContext *, struct FatNode *,
    FsAccess);
static enum Result writeNodeData(struct CommandContext *, struct FatNode *,
    FsLength, const void *, size_t, size_t *);
static enum Result writeNodeTime(struct CommandContext *, struct FatNode *,
    time64_t);
static enum Result writeSector(struct CommandContext *, struct FatHandle *,
    uint32_t);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_POOLS) || defined(CONFIG_THREADS)
static enum Result allocatePool(struct Pool *, size_t, size_t, const void *);
static void freePool(struct Pool *);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) || defined(CONFIG_WRITE)
static void allocateStaticNode(struct FatHandle *, struct FatNode *);
static void freeStaticNode(struct FatNode *);
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
static void fillLongName(struct DirEntryImage *, const char16_t *, size_t);
static void fillLongNameEntry(struct DirEntryImage *, uint8_t, uint8_t,
    uint8_t);
#endif
/*----------------------------------------------------------------------------*/
/* Filesystem handle functions */
static enum Result fatHandleInit(void *, const void *);
static void fatHandleDeinit(void *);
static void *fatHandleRoot(void *);
static enum Result fatHandleSync(void *);

/* Node functions */
static enum Result fatNodeInit(void *, const void *);
static void fatNodeDeinit(void *);
static enum Result fatNodeCreate(void *, const struct FsFieldDescriptor *,
    size_t);
static void *fatNodeHead(void *);
static void fatNodeFree(void *);
static enum Result fatNodeLength(void *, enum FsFieldType, FsLength *);
static enum Result fatNodeNext(void *);
static enum Result fatNodeRead(void *, enum FsFieldType, FsLength,
    void *, size_t, size_t *);
static enum Result fatNodeRemove(void *, void *);
static enum Result fatNodeWrite(void *, enum FsFieldType, FsLength,
    const void *, size_t, size_t *);
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
static enum Result allocateBuffers(struct FatHandle *handle,
    const struct Fat32Config * const config)
{
#if defined(CONFIG_THREADS) || defined(CONFIG_POOLS)
  enum Result res;
#else
  /* Suppress warning */
  (void)config;
#endif

#ifdef CONFIG_THREADS
  const size_t threadCount = config->threads ?
      config->threads : DEFAULT_THREAD_COUNT;

  /* Create consistency and memory locks */
  res = mutexInit(&handle->consistencyMutex);
  if (res != E_OK)
    return res;

  res = mutexInit(&handle->memoryMutex);
  if (res != E_OK)
  {
    mutexDeinit(&handle->consistencyMutex);
    return res;
  }

  /* Allocate context pool */
  res = allocatePool(&handle->contextPool, threadCount,
      sizeof(struct CommandContext), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_LOCKS);
    return res;
  }

  /* Custom initialization of default sector values */
  struct CommandContext *contextBase = handle->contextPool.data;

  for (size_t index = 0; index < threadCount; ++index, ++contextBase)
    contextBase->sector = RESERVED_SECTOR;

  DEBUG_PRINT(2, "fat32: context pool:   %zu\n", (sizeof(struct CommandContext)
      + threadCount * (sizeof(struct CommandContext *))));
#else
  /* Allocate single context buffer */
  handle->context = malloc(sizeof(struct CommandContext));
  if (!handle->context)
    return E_MEMORY;
  handle->context->sector = RESERVED_SECTOR;

  DEBUG_PRINT(2, "fat32: context pool:   %zu\n", sizeof(struct CommandContext));
#endif /* CONFIG_THREADS */

#ifdef CONFIG_WRITE
  pointerListInit(&handle->openedFiles);
#endif /* CONFIG_WRITE */

#ifdef CONFIG_POOLS
  /* Allocate and fill node pool */
  const size_t nodeCount = config->nodes ? config->nodes : DEFAULT_NODE_COUNT;

  res = allocatePool(&handle->nodePool, nodeCount,
      sizeof(struct FatNode), FatNode);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_FILE_LIST);
    return res;
  }

  DEBUG_PRINT(2, "fat32: node pool:      %zu\n", (sizeof(struct FatNode)
      + nodeCount * (sizeof(struct FatNode *))));
#endif /* CONFIG_POOLS */

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static struct CommandContext *allocateContext(struct FatHandle *handle)
{
  struct CommandContext *context = 0;

#ifdef CONFIG_THREADS
  mutexLock(&handle->memoryMutex);
  if (!pointerQueueEmpty(&handle->contextPool.queue))
  {
    context = pointerQueueFront(&handle->contextPool.queue);
    pointerQueuePopFront(&handle->contextPool.queue);
  }
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
#ifdef CONFIG_POOLS
  if (!pointerQueueEmpty(&handle->nodePool.queue))
  {
    node = pointerQueueFront(&handle->nodePool.queue);
    pointerQueuePopFront(&handle->nodePool.queue);
    FatNode->init(node, &config);
  }
#else
  node = init(FatNode, &config);
#endif
  unlockPools(handle);

  return node;
}
/*----------------------------------------------------------------------------*/
static unsigned int computeShortNameLength(const struct DirEntryImage *entry)
{
  unsigned int nameLength = 0;
  const char *source;

  source = entry->name;
  for (unsigned int index = 0; index < BASENAME_LENGTH; ++index)
  {
    if (*source++ != ' ')
      ++nameLength;
    else
      break;
  }

  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    ++nameLength;

    source = entry->extension;
    for (unsigned int index = 0; index < EXTENSION_LENGTH; ++index)
    {
      if (*source++ != ' ')
        ++nameLength;
      else
        break;
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
  for (unsigned int index = 0; index < BASENAME_LENGTH; ++index)
  {
    if (*source != ' ')
      *destination++ = *source++;
    else
      break;
  }

  /* Add dot when entry is not directory or extension exists */
  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    *destination++ = '.';

    /* Copy entry extension */
    source = entry->extension;
    for (unsigned int index = 0; index < EXTENSION_LENGTH; ++index)
    {
      if (*source != ' ')
        *destination++ = *source++;
      else
        break;
    }
  }
  *destination = '\0';
}
/*----------------------------------------------------------------------------*/
/*
 * Fields handle, parentIndex and parentCluster in node have to be initialized.
 */
static enum Result fetchEntry(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const struct DirEntryImage *entry;

  /* Fields cluster, index and type are updated */
  while (1)
  {
    if (node->parentIndex >= calcNodeCount(handle))
    {
      /* Check clusters until end of directory (EOC entry in FAT) */
      const enum Result res = getNextCluster(context, handle,
          &node->parentCluster);

      if (res != E_OK)
      {
        /* Set index to the last entry in last existing cluster */
        node->parentIndex = calcNodeCount(handle) - 1;
        return res;
      }
      node->parentIndex = 0;
    }

    const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
        + ENTRY_SECTOR(node->parentIndex);
    const enum Result res = readSector(context, handle, sector);

    if (res != E_OK)
      return res;

    entry = getDirEntry(context, node->parentIndex);

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
static enum Result fetchNode(struct CommandContext *context,
    struct FatNode *node)
{
  const struct DirEntryImage *entry;
  enum Result res;

#ifdef CONFIG_UNICODE
  uint8_t checksum = 0;
  uint8_t found = 0; /* LFN chunks found */
#endif

  while ((res = fetchEntry(context, node)) == E_OK)
  {
    /* Sector reload is not needed in current context */
    entry = getDirEntry(context, node->parentIndex);

#ifdef CONFIG_UNICODE
    if (!(entry->ordinal & LFN_DELETED)
        && ((entry->flags & MASK_LFN) == MASK_LFN))
    {
      char16_t nameChunk[LFN_ENTRY_LENGTH + 1];

      if (entry->ordinal & LFN_LAST)
      {
        checksum = entry->checksum;
        found = 0;

        node->lfn = entry->ordinal & ~LFN_LAST;
        node->nameIndex = node->parentIndex;
        node->nameCluster = node->parentCluster;
        node->nameLength = 0;
      }
      ++found;

      extractLongName(nameChunk, entry);
      nameChunk[LFN_ENTRY_LENGTH] = 0;
      node->nameLength += uLengthFromUtf16(nameChunk);
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

#ifdef CONFIG_UNICODE
  if (!found || found != node->lfn
      || checksum != getChecksum(entry->filename, NAME_LENGTH))
  {
    /* Wrong checksum or chunk count does not match */
    node->lfn = 0;
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
static void freeBuffers(struct FatHandle *handle, enum Cleanup step)
{
  switch (step)
  {
    case FREE_ALL:
    case FREE_NODE_POOL:
#ifdef CONFIG_POOLS
      freePool(&handle->nodePool);
#endif
      /* Falls through */

    case FREE_FILE_LIST:
#ifdef CONFIG_WRITE
      pointerListDeinit(&handle->openedFiles);
#endif

#ifdef CONFIG_THREADS
      freePool(&handle->contextPool);
#else
      free(handle->context);
#endif
      /* Falls through */

    case FREE_LOCKS:
#ifdef CONFIG_THREADS
      mutexDeinit(&handle->memoryMutex);
      mutexDeinit(&handle->consistencyMutex);
#endif
      break;

    default:
      break;
  }
}
/*----------------------------------------------------------------------------*/
static void freeContext(struct FatHandle *handle,
    struct CommandContext *context)
{
#ifdef CONFIG_THREADS
  mutexLock(&handle->memoryMutex);
  pointerQueuePushBack(&handle->contextPool.queue, context);
  mutexUnlock(&handle->memoryMutex);
#else
  (void)handle;
  (void)context;
#endif
}
/*----------------------------------------------------------------------------*/
static void freeNode(struct FatNode *node)
{
#ifdef CONFIG_POOLS
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  FatNode->deinit(node);

  lockPools(handle);
  pointerQueuePushBack(&handle->nodePool.queue, node);
  unlockPools(handle);
#else
  deinit(node);
#endif
}
/*----------------------------------------------------------------------------*/
static enum Result getNextCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
{
  const enum Result res = readSector(context, handle, handle->tableSector
      + (*cluster >> CELL_COUNT));

  if (res != E_OK)
    return res;

  uint32_t next;

  memcpy(&next, context->buffer + CELL_OFFSET(*cluster), sizeof(next));
  next = fromLittleEndian32(next);

  if (isClusterUsed(next))
  {
    *cluster = next;
    return E_OK;
  }
  else
    return E_EMPTY;
}
/*----------------------------------------------------------------------------*/
static enum Result mountStorage(struct FatHandle *handle)
{
  struct CommandContext * const context = allocateContext(handle);
  enum Result res;

  if (!context)
    return E_MEMORY;

  /* Read first sector */
  res = readSector(context, handle, 0);
  if (res != E_OK)
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

  DEBUG_PRINT(1, "fat32: cluster size:   %u\n", (1 << handle->clusterSize));
  DEBUG_PRINT(1, "fat32: table sector:   %"PRIu32"\n", handle->tableSector);
  DEBUG_PRINT(1, "fat32: data sector:    %"PRIu32"\n", handle->dataSector);

#ifdef CONFIG_WRITE
  handle->tableCount = boot->tableCount;
  handle->tableSize = fromLittleEndian32(boot->tableSize);
  handle->clusterCount = ((fromLittleEndian32(boot->partitionSize)
      - handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = fromLittleEndian16(boot->infoSector);

  DEBUG_PRINT(1, "fat32: info sector:    %"PRIu16"\n", handle->infoSector);
  DEBUG_PRINT(1, "fat32: table copies:   %"PRIu8"\n", handle->tableCount);
  DEBUG_PRINT(1, "fat32: table size:     %"PRIu32"\n", handle->tableSize);
  DEBUG_PRINT(1, "fat32: cluster count:  %"PRIu32"\n", handle->clusterCount);
  DEBUG_PRINT(1, "fat32: sectors count:  %"PRIu32"\n",
      fromLittleEndian32(boot->partitionSize));

  /* Read information sector */
  res = readSector(context, handle, handle->infoSector);
  if (res != E_OK)
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

  DEBUG_PRINT(1, "fat32: free clusters:  %"PRIu32"\n",
      fromLittleEndian32(info->freeClusters));
#endif

exit:
  freeContext(handle, context);
  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result rawDateTimeToTimestamp(time64_t *timestamp,
    uint16_t date, uint16_t time)
{
  const struct RtDateTime dateTime = {
      .second = time & 0x1F,
      .minute = (time >> 5) & 0x3F,
      .hour = (time >> 11) & 0x1F,
      .day = date & 0x1F,
      .month = (date >> 5) & 0x0F,
      .year = ((date >> 9) & 0x7F) + 1980
  };

  time64_t unixTime;
  const enum Result res = rtMakeEpochTime(&unixTime, &dateTime);

  if (res == E_OK)
    *timestamp = unixTime * 1000000;

  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result readBuffer(struct FatHandle *handle, uint32_t sector,
    uint8_t *buffer, uint32_t count)
{
  const uint64_t position = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = count << SECTOR_EXP;
  enum Result res;

  ifSetParam(handle->interface, IF_ACQUIRE, 0);

  res = ifSetParam(handle->interface, IF_POSITION_64, &position);
  if (res == E_OK)
  {
    if (ifRead(handle->interface, buffer, length) != length)
    {
      res = ifGetParam(handle->interface, IF_STATUS, 0);
      assert(res != E_OK && res != E_INVALID);
    }
  }

  ifSetParam(handle->interface, IF_RELEASE, 0);
  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result readClusterChain(struct CommandContext *context,
    struct FatNode *node, uint32_t dataPosition, uint8_t *dataBuffer,
    uint32_t dataLength)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  uint32_t currentCluster = node->currentCluster;
  uint32_t currentPosition = node->payloadPosition;
  uint8_t currentSector;

  /* Seek to the requested position */
  if (currentPosition != dataPosition)
  {
    const enum Result res = seekClusterChain(context, handle, currentPosition,
        dataPosition, node->payloadCluster, &currentCluster);

    if (res != E_OK)
      return res;
    currentPosition = dataPosition;
  }

  /* Calculate index of the sector in a cluster */
  if (currentPosition > 0)
  {
    currentSector = sectorInCluster(handle, currentPosition);
    if (!currentSector && !(currentPosition & (SECTOR_SIZE - 1)))
      currentSector = 1 << handle->clusterSize;
  }
  else
    currentSector = 0;

  while (dataLength)
  {
    if (currentSector >= (1 << handle->clusterSize))
    {
      /* Try to load the next cluster */
      const enum Result res = getNextCluster(context, handle, &currentCluster);

      if (res != E_OK)
        return res;

      currentSector = 0;
    }

    /* Offset from the beginning of the sector */
    const uint32_t sector = calcSectorNumber(handle, currentCluster)
        + currentSector;
    const uint16_t offset = currentPosition & (SECTOR_SIZE - 1);
    uint16_t chunk;

    if (offset || dataLength < SECTOR_SIZE)
    {
      /* Position within the sector */
      chunk = SECTOR_SIZE - offset;
      chunk = MIN(chunk, dataLength);

      const enum Result res = readSector(context, handle, sector);

      if (res != E_OK)
        return res;

      memcpy(dataBuffer, context->buffer + offset, chunk);

      if (chunk + offset >= SECTOR_SIZE)
        ++currentSector;
    }
    else
    {
      /* Position is aligned along the first byte of the sector */
      chunk = ((1 << handle->clusterSize) - currentSector) << SECTOR_EXP;
      chunk = MIN(chunk, dataLength);
      chunk &= ~(SECTOR_SIZE - 1); /* Align along sector boundary */

      /* Read data to the buffer directly without additional copying */
      const enum Result res = readBuffer(handle, sector, dataBuffer,
          chunk >> SECTOR_EXP);

      if (res != E_OK)
        return res;

      currentSector += chunk >> SECTOR_EXP;
    }

    dataBuffer += chunk;
    dataLength -= chunk;
    currentPosition += chunk;
  }

  node->payloadPosition = currentPosition;
  node->currentCluster = currentCluster;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void readNodeAccess(struct FatNode *node, void *buffer)
{
  FsAccess value = FS_ACCESS_READ;

  if (!(node->flags & FAT_FLAG_RO))
    value |= FS_ACCESS_WRITE;

  memcpy(buffer, &value, sizeof(value));
}
/*----------------------------------------------------------------------------*/
static void readNodeId(struct FatNode *node, void *buffer)
{
  const uint64_t value = (uint64_t)node->parentIndex
      | ((uint64_t)node->parentCluster << 16);

  memcpy(buffer, &value, sizeof(value));
}
/*----------------------------------------------------------------------------*/
static enum Result readNodeData(struct CommandContext *context,
    struct FatNode *node, FsLength position, void *buffer, size_t length,
    size_t *read)
{
  if (!(node->flags & FAT_FLAG_FILE))
    return E_INVALID;
  if (position > node->payloadSize)
    return E_VALUE;

  if (length > node->payloadSize - position)
    length = (size_t)(node->payloadSize - position);

  if (length)
  {
    const enum Result res = readClusterChain(context, node,
        position, buffer, (uint32_t)length);

    if (res == E_OK)
      *read = length;

    return res;
  }
  else
  {
    *read = 0;
    return E_OK;
  }
}
/*----------------------------------------------------------------------------*/
static enum Result readNodeName(struct CommandContext *context,
    struct FatNode *node, void *buffer, size_t length, size_t *read)
{
  if (length <= node->nameLength)
    return E_VALUE;

#ifdef CONFIG_UNICODE
  if (hasLongName(node))
  {
    const enum Result res = readLongName(context, buffer, length, node);

    if (res == E_OK)
    {
      /* Include terminating character */
      *read = node->nameLength + 1;
    }
    return res;
  }
#endif

  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  const enum Result res = readSector(context, handle, sector);

  if (res != E_OK)
    return res;

  const struct DirEntryImage * const entry =
      getDirEntry(context, node->parentIndex);

  extractShortName(buffer, entry);
  /* Include terminating character */
  *read = node->nameLength + 1;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum Result readNodeTime(struct CommandContext *context,
    struct FatNode *node, void *buffer)
{
  /* Timestamp reading requires access to the directory entry */
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  const enum Result res = readSector(context, handle, sector);

  if (res != E_OK)
    return res;

  const struct DirEntryImage * const entry =
      getDirEntry(context, node->parentIndex);

  const uint16_t rawDate = fromLittleEndian16(entry->date);
  const uint16_t rawTime = fromLittleEndian16(entry->time);

  return rawDateTimeToTimestamp(buffer, rawDate, rawTime);
}
/*----------------------------------------------------------------------------*/
static enum Result readSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  if (context->sector == sector)
    return E_OK;

  const uint64_t position = (uint64_t)sector << SECTOR_EXP;
  enum Result res;

  ifSetParam(handle->interface, IF_ACQUIRE, 0);

  res = ifSetParam(handle->interface, IF_POSITION_64, &position);
  if (res == E_OK)
  {
    if (ifRead(handle->interface, context->buffer, SECTOR_SIZE) == SECTOR_SIZE)
    {
      context->sector = sector;
    }
    else
    {
      res = ifGetParam(handle->interface, IF_STATUS, 0);
      assert(res != E_OK && res != E_INVALID);
    }
  }

  ifSetParam(handle->interface, IF_RELEASE, 0);
  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result seekClusterChain(struct CommandContext *context,
    struct FatHandle *handle, uint32_t currentPosition, uint32_t nextPosition,
    uint32_t startCluster, uint32_t *currentCluster)
{
  uint32_t clusterCount;
  uint32_t clusterNumber;

  if (currentPosition > nextPosition)
  {
    clusterNumber = startCluster;
    clusterCount = nextPosition;
  }
  else
  {
    clusterNumber = *currentCluster;
    clusterCount = nextPosition - currentPosition;
  }
  clusterCount >>= handle->clusterSize + SECTOR_EXP;

  if (clusterCount && !sectorInCluster(handle, nextPosition)
      && !(nextPosition & (SECTOR_SIZE - 1)))
  {
    --clusterCount;
  }

  while (clusterCount--)
  {
    const enum Result res = getNextCluster(context, handle, &clusterNumber);

    if (res != E_OK)
      return res;
  }

  *currentCluster = clusterNumber;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
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
#ifdef CONFIG_UNICODE
/* Calculate entry name checksum for long file name entries support */
static uint8_t getChecksum(const char *name, size_t length)
{
  uint8_t sum = 0;

  for (size_t index = 0; index < length; ++index)
    sum = ((sum >> 1) | (sum << 7)) + *name++;

  return sum;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
static enum Result readLongName(struct CommandContext *context, char *name,
    size_t length, const struct FatNode *node)
{
  if (node->lfn > calcLfnCount(CONFIG_NAME_LENGTH))
  {
    /* System buffer is shorter than entry name */
    return E_MEMORY;
  }

  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct FatNode staticNode;

  allocateStaticNode(handle, &staticNode);
  staticNode.parentCluster = node->nameCluster;
  staticNode.parentIndex = node->nameIndex;

  unsigned int chunks = 0;
  char16_t nameBuffer[CONFIG_NAME_LENGTH / 2];
  enum Result res;

  while ((res = fetchEntry(context, &staticNode)) == E_OK)
  {
    /* Sector is already loaded during entry reading */
    const struct DirEntryImage * const entry = getDirEntry(context,
        staticNode.parentIndex);

    if ((entry->flags & MASK_LFN) != MASK_LFN)
      break;

    const unsigned int offset = entry->ordinal & ~LFN_LAST;

    if (offset > node->lfn)
    {
      /* Incorrect LFN entry */
      res = E_MEMORY;
      break;
    }

    if (entry->ordinal & LFN_LAST)
      nameBuffer[offset * LFN_ENTRY_LENGTH] = 0;

    extractLongName(&nameBuffer[(offset - 1) * LFN_ENTRY_LENGTH], entry);
    ++chunks;
    ++staticNode.parentIndex;
  }

  /*
   * Long file name entries always precede data entry thus
   * processing of return values others than success is not needed.
   */
  if (res == E_OK)
  {
    if (chunks && chunks == node->lfn)
      uFromUtf16(name, nameBuffer, length);
    else
      res = E_ENTRY;
  }

  freeStaticNode(&staticNode);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result allocateCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
{
  uint32_t current = handle->lastAllocated + 1;
  enum Result res;

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
    if (isClusterFree(fromLittleEndian32(*address)))
    {
      const uint32_t eoc = toLittleEndian32(CLUSTER_EOC_VAL);
      const uint32_t allocatedCluster = toLittleEndian32(current);
      const uint16_t parentOffset = *cluster >> CELL_COUNT;

      /* Mark cluster as busy */
      memcpy(address, &eoc, sizeof(*address));

      /*
       * Save changes to the allocation table when previous cluster
       * is not available or parent cluster entry is located in other sector.
       */
      if (!*cluster || parentOffset != currentOffset)
      {
        res = updateTable(context, handle, currentOffset);
        if (res != E_OK)
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

        res = updateTable(context, handle, parentOffset);
        if (res != E_OK)
          return res;
      }

      DEBUG_PRINT(2, "fat32: allocated cluster: %"PRIu32", parent %"PRIu32"\n",
          current, *cluster);
      handle->lastAllocated = current;
      *cluster = current;

      /* Update information sector */
      res = readSector(context, handle, handle->infoSector);
      if (res != E_OK)
        return res;

      struct InfoSectorImage * const info =
          (struct InfoSectorImage *)context->buffer;

      info->lastAllocated = toLittleEndian32(current);
      info->freeClusters =
          toLittleEndian32(fromLittleEndian32(info->freeClusters) - 1);

      return writeSector(context, handle, handle->infoSector);
    }

    ++current;
  }

  DEBUG_PRINT(1, "fat32: allocation error, partition may be full\n");
  return E_FULL;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result clearCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t sector = calcSectorNumber(handle, cluster + 1);

  memset(context->buffer, 0, SECTOR_SIZE);

  do
  {
    const enum Result res = writeSector(context, handle, --sector);

    if (res != E_OK)
      return res;
  }
  while (sector & ((1 << handle->clusterSize) - 1));

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static void clearDirtyFlag(struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  pointerListErase(&handle->openedFiles, node);
  node->flags &= ~FAT_FLAG_DIRTY;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result createNode(struct CommandContext *context,
    const struct FatNode *root, bool directory, const char *nodeName,
    FsAccess nodeAccess, uint32_t nodePayloadCluster, time64_t nodeAccessTime)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  char shortName[NAME_LENGTH];
  unsigned int chunks = 0;
  enum Result res;

  res = fillShortName(shortName, nodeName, !directory);
  if (res != E_OK && res != E_VALUE)
    return res;

#ifdef CONFIG_UNICODE
  const size_t nameLength = uLengthToUtf16(nodeName) + 1;
  char16_t nameBuffer[CONFIG_NAME_LENGTH / 2];
  const bool longNameRequired = res == E_VALUE;

  if (nameLength > ARRAY_SIZE(nameBuffer))
    return E_VALUE;
#endif

  /* Propose new short name when selected name already exists */
  res = uniqueNamePropose(context, root, shortName);
  if (res != E_OK)
    return res;

#ifdef CONFIG_UNICODE
  if (longNameRequired)
  {
    /* Name length contains terminating character */
    uToUtf16(nameBuffer, nodeName, nameLength);
    /* Append additional entry when last chunk is incomplete */
    chunks = (nameLength + LFN_ENTRY_LENGTH - 2) / LFN_ENTRY_LENGTH;
  }
#endif

  struct FatNode staticNode;
  allocateStaticNode(handle, &staticNode);

  /* Find suitable space within the directory */
  res = findGap(context, &staticNode, root, chunks + 1);

#ifdef CONFIG_UNICODE
  if (chunks && res == E_OK)
  {
    const uint8_t checksum = getChecksum(shortName, NAME_LENGTH);

    /* Save start cluster and index values before filling the chain */
    staticNode.nameCluster = staticNode.parentCluster;
    staticNode.nameIndex = staticNode.parentIndex;

    for (unsigned int current = chunks; current && res == E_OK; --current)
    {
      const uint32_t sector = calcSectorNumber(handle, staticNode.parentCluster)
          + ENTRY_SECTOR(staticNode.parentIndex);

      res = readSector(context, handle, sector);
      if (res != E_OK)
        break;

      struct DirEntryImage * const entry =
          getDirEntry(context, staticNode.parentIndex);
      const uint16_t offset = (current - 1) * LFN_ENTRY_LENGTH;
      const uint16_t left = nameLength - 1 - offset;

      fillLongName(entry, nameBuffer + offset,
          left > LFN_ENTRY_LENGTH ? LFN_ENTRY_LENGTH : left);
      fillLongNameEntry(entry, current, chunks, checksum);

      ++staticNode.parentIndex;

      if (!(staticNode.parentIndex & (ENTRY_EXP - 1)))
      {
        /* Write back updated sector when switching sectors */
        res = writeSector(context, handle, sector);
        if (res != E_OK)
          break;
      }

      res = fetchEntry(context, &staticNode);
      if (res == E_ENTRY)
        res = E_OK;
    }
  }
#endif

  if (res == E_OK)
  {
    struct DirEntryImage * const entry =
        getDirEntry(context, staticNode.parentIndex);

    memcpy(entry->filename, shortName, NAME_LENGTH);
    fillDirEntry(entry, directory, nodeAccess, nodePayloadCluster,
        nodeAccessTime);

    /* Buffer is already filled with actual sector data */
    const uint32_t sector = calcSectorNumber(handle, staticNode.parentCluster)
        + ENTRY_SECTOR(staticNode.parentIndex);

    freeStaticNode(&staticNode);
    res = writeSector(context, handle, sector);
  }

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static void extractShortBasename(char *baseName, const char *shortName)
{
  size_t index;

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
#ifdef CONFIG_WRITE
static void fillDirEntry(struct DirEntryImage *entry, bool directory,
    FsAccess access, uint32_t payloadCluster, time64_t timestamp)
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

  struct RtDateTime dateTime;

  timestamp /= 1000000;
  rtMakeTime(&dateTime, timestamp);
  entry->date = toLittleEndian16(timeToRawDate(&dateTime));
  entry->time = toLittleEndian16(timeToRawTime(&dateTime));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
/* Returns success when node has a valid short name */
static enum Result fillShortName(char *shortName, const char *name,
    bool extension)
{
  const size_t length = strlen(name);

  if (!length)
    return E_ERROR;

  /* Find start position of the extension */
  const char *dot = name + length - 1;
  enum Result res = E_OK;

  if (extension)
  {
    for (; dot >= name && *dot != '.'; --dot);
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
    const size_t nameLength = dot - name;
    const size_t extensionLength = length - nameLength;

    /* Check whether file name and extension have adequate length */
    if (extensionLength > EXTENSION_LENGTH + 1 || nameLength > BASENAME_LENGTH)
      res = E_VALUE;
  }

  size_t position = 0;

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
#ifdef CONFIG_WRITE
/* Allocate single node or node chain inside root node chain. */
static enum Result findGap(struct CommandContext *context, struct FatNode *node,
    const struct FatNode *root, unsigned int chainLength)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  unsigned int chunks = 0;
  uint32_t parent = root->payloadCluster;
  uint16_t index = 0;

  node->parentCluster = root->payloadCluster;
  node->parentIndex = 0;

  while (chunks != chainLength)
  {
    enum Result res = fetchEntry(context, node);
    bool allocateParent = false;

    switch (res)
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
      int chunksLeft = (chainLength - chunks)
          - (calcNodeCount(handle) - node->parentIndex);

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
        chunksLeft -= calcNodeCount(handle);
      }
      break;
    }

    /*
     * Entry processing will be executed only after entry fetching so
     * in this case sector reloading is redundant.
     */
    const struct DirEntryImage * const entry = getDirEntry(context,
        node->parentIndex);

    /* Empty node, deleted long file name node or deleted node */
    if (!entry->name[0] || ((entry->flags & MASK_LFN) == MASK_LFN
        && (entry->ordinal & LFN_DELETED)) || entry->name[0] == E_FLAG_EMPTY)
    {
      if (!chunks)
      {
        /* First free node found */
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
#ifdef CONFIG_WRITE
static enum Result freeChain(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t current = cluster;
  uint32_t released = 0;
  enum Result res = E_ERROR;

  if (current == RESERVED_CLUSTER)
    return E_OK; /* Already empty */

  while (isClusterUsed(current))
  {
    const uint32_t sector = handle->tableSector + (current >> CELL_COUNT);

    /* Read allocation table sector with next cluster value */
    res = readSector(context, handle, sector);
    if (res != E_OK)
      break;

    uint32_t * const address =
        (uint32_t *)(context->buffer + CELL_OFFSET(current));
    uint32_t next;

    memcpy(&next, address, sizeof(*address));
    next = fromLittleEndian32(next);
    memset(address, 0, sizeof(*address));

    /* Update table when switching table sectors */
    if (current >> CELL_COUNT != next >> CELL_COUNT)
    {
      res = updateTable(context, handle, current >> CELL_COUNT);
      if (res != E_OK)
        break;
    }

    ++released;
    DEBUG_PRINT(2, "fat32: cleared cluster: %"PRIu32"\n", current);
    current = next;
  }

  if (res != E_OK)
    return res;

  /* Update information sector */
  res = readSector(context, handle, handle->infoSector);
  if (res != E_OK)
    return res;

  struct InfoSectorImage * const info =
      (struct InfoSectorImage *)context->buffer;

  /* Set free clusters count */
  info->freeClusters = toLittleEndian32(
      fromLittleEndian32(info->freeClusters) + released);

  /* TODO Cache information sector updates */
  return writeSector(context, handle, handle->infoSector);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result markFree(struct CommandContext *context,
    const struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t lastSector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  struct FatNode staticNode;
  enum Result res;

  allocateStaticNode(handle, &staticNode);

#ifdef CONFIG_UNICODE
  staticNode.parentCluster = node->nameCluster;
  staticNode.parentIndex = node->nameIndex;
#endif

  while ((res = fetchEntry(context, &staticNode)) == E_OK)
  {
    const uint32_t sector = calcSectorNumber(handle, staticNode.parentCluster)
        + ENTRY_SECTOR(staticNode.parentIndex);
    const bool lastEntry = sector == lastSector
        && staticNode.parentIndex == node->parentIndex;

    /* Sector is already loaded */
    struct DirEntryImage * const entry = getDirEntry(context,
        staticNode.parentIndex);

    /* Mark entry as empty by changing first byte of the name */
    entry->name[0] = E_FLAG_EMPTY;

    if (lastEntry || !(staticNode.parentIndex & (ENTRY_EXP - 1)))
    {
      /* Write back updated sector when switching sectors or last entry freed */
      res = writeSector(context, handle, sector);
      if (res != E_OK)
        break;
    }

    ++staticNode.parentIndex;

    if (lastEntry)
      break;
  }

  freeStaticNode(&staticNode);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
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
#ifdef CONFIG_WRITE
static enum Result truncatePayload(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  enum Result res;

  if ((node->flags & FAT_FLAG_DIR) && node->payloadCluster != RESERVED_CLUSTER)
  {
    struct FatNode staticNode;
    allocateStaticNode(handle, &staticNode);

    staticNode.parentCluster = node->payloadCluster;
    staticNode.parentIndex = 0;

    /* Check whether the directory is not empty */
    while ((res = fetchNode(context, &staticNode)) == E_OK)
    {
      const struct DirEntryImage * const entry = getDirEntry(context,
          staticNode.parentIndex);
      char nameBuffer[BASENAME_LENGTH + 1];

      extractShortBasename(nameBuffer, entry->name);
      if (strcmp(nameBuffer, ".") && strcmp(nameBuffer, ".."))
      {
        res = E_EXIST;
        break;
      }

      ++staticNode.parentIndex;
    }

    freeStaticNode(&staticNode);

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
#ifdef CONFIG_WRITE
static unsigned int uniqueNameConvert(char *shortName)
{
  size_t position = strlen(shortName);

  if (!position--)
    return 0;

  size_t begin = 0;
  size_t end = 0;
  unsigned int nameIndex = 0;

  for (; position > 0; --position)
  {
    const char value = shortName[position];

    if (value == '~')
    {
      begin = position++;

      while (position <= end)
      {
        const unsigned int currentNumber = shortName[position] - '0';

        nameIndex = (nameIndex * 10) + currentNumber;
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
      break;
  }

  if (nameIndex)
    shortName[begin] = '\0';

  return nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result uniqueNamePropose(struct CommandContext *context,
    const struct FatNode *root, char *shortName)
{
  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  struct FatNode staticNode;

  allocateStaticNode(handle, &staticNode);
  staticNode.parentCluster = root->payloadCluster;
  staticNode.parentIndex = 0;

  char currentName[BASENAME_LENGTH + 1];
  unsigned int proposed = 0;
  enum Result res;

  extractShortBasename(currentName, shortName);

  while ((res = fetchEntry(context, &staticNode)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    const struct DirEntryImage * const entry = getDirEntry(context,
        staticNode.parentIndex);

    if (entry->name[0] == E_FLAG_EMPTY || (entry->flags & MASK_LFN) == MASK_LFN)
    {
      ++staticNode.parentIndex;
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

    ++staticNode.parentIndex;
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

    const unsigned int proposedLength = position - suffix;
    const unsigned int remainingSpace = BASENAME_LENGTH - proposedLength - 1;
    unsigned int baseLength = strlen(currentName);

    if (baseLength > remainingSpace)
      baseLength = remainingSpace;

    memset(shortName + baseLength, ' ', BASENAME_LENGTH - baseLength);
    shortName[baseLength] = '~';

    for (unsigned int index = 1; index <= proposedLength; ++index)
      shortName[baseLength + index] = suffix[proposedLength - index];

    DEBUG_PRINT(2, "fat32: proposed short name: \"%.8s\"\n", shortName);

    res = E_OK;
  }
  else
    res = E_EXIST;

  freeStaticNode(&staticNode);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result setupDirCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t parentCluster, uint32_t payloadCluster,
    time64_t timestamp)
{
  struct DirEntryImage *entry = getDirEntry(context, 0);
  const enum Result res = clearCluster(context, handle, payloadCluster);

  if (res != E_OK)
    return res;

  /* Current directory entry . */
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = '.';
  fillDirEntry(entry, true, FS_ACCESS_READ | FS_ACCESS_WRITE,
      payloadCluster, timestamp);

  /* Parent directory entry .. */
  ++entry;
  memset(entry->filename, ' ', NAME_LENGTH);
  entry->name[0] = entry->name[1] = '.';
  fillDirEntry(entry, true, FS_ACCESS_READ | FS_ACCESS_WRITE,
      payloadCluster, timestamp);
  if (parentCluster != handle->rootCluster)
  {
    entry->clusterHigh = toLittleEndian16((uint16_t)(payloadCluster >> 16));
    entry->clusterLow = toLittleEndian16((uint16_t)payloadCluster);
  }
  else
    entry->clusterLow = entry->clusterHigh = 0;

  return writeSector(context, handle, calcSectorNumber(handle, payloadCluster));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result syncFile(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  enum Result res;

  /* Lock handle to prevent directory modifications from other threads */
  lockHandle(handle);

  res = readSector(context, handle, sector);
  if (res != E_OK)
  {
    unlockHandle(handle);
    return res;
  }

  /* Pointer to entry position in sector */
  struct DirEntryImage * const entry = getDirEntry(context, node->parentIndex);

  /* Update first cluster when writing to empty file or truncating file */
  entry->clusterHigh = toLittleEndian16((uint16_t)(node->payloadCluster >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)node->payloadCluster);
  /* Update file size */
  entry->size = toLittleEndian32(node->payloadSize);

  res = writeSector(context, handle, sector);
  if (res == E_OK)
    clearDirtyFlag(node);

  unlockHandle(handle);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static uint16_t timeToRawDate(const struct RtDateTime *value)
{
  return value->day | (value->month << 5) | ((value->year - 1980) << 9);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static uint16_t timeToRawTime(const struct RtDateTime *value)
{
  return (value->second >> 1) | (value->minute << 5) | (value->hour << 11);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
/* Copy current sector into FAT sectors located at offset */
static enum Result updateTable(struct CommandContext *context,
    struct FatHandle *handle, uint32_t offset)
{
  for (size_t fat = 0; fat < handle->tableCount; ++fat)
  {
    const enum Result res = writeSector(context, handle, offset
        + handle->tableSector + handle->tableSize * fat);

    if (res != E_OK)
      return res;
  }

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeBuffer(struct FatHandle *handle,
    uint32_t sector, const uint8_t *buffer, uint32_t count)
{
  const uint64_t position = (uint64_t)sector << SECTOR_EXP;
  const uint32_t length = count << SECTOR_EXP;
  enum Result res;

  ifSetParam(handle->interface, IF_ACQUIRE, 0);

  res = ifSetParam(handle->interface, IF_POSITION_64, &position);
  if (res == E_OK)
  {
    if (ifWrite(handle->interface, buffer, length) != length)
    {
      res = ifGetParam(handle->interface, IF_STATUS, 0);
      assert(res != E_OK && res != E_INVALID);
    }
  }

  ifSetParam(handle->interface, IF_RELEASE, 0);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeClusterChain(struct CommandContext *context,
    struct FatNode *node, uint32_t dataPosition, const uint8_t *dataBuffer,
    uint32_t dataLength)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  uint32_t currentCluster;
  uint32_t currentPosition = node->payloadPosition;
  uint8_t currentSector;

  /* Allocate first cluster when the chain is empty */
  if (node->payloadCluster == RESERVED_CLUSTER)
  {
    lockHandle(handle);
    const enum Result res = allocateCluster(context, handle,
        &node->payloadCluster);
    unlockHandle(handle);

    if (res != E_OK)
      return res;

    currentCluster = node->payloadCluster;
  }
  else
    currentCluster = node->currentCluster;

  /* Seek to the requested position */
  if (currentPosition != dataPosition)
  {
    const enum Result res = seekClusterChain(context, handle, currentPosition,
        dataPosition, node->payloadCluster, &currentCluster);

    if (res != E_OK)
      return res;
    currentPosition = dataPosition;
  }

  /* Calculate index of the sector in a cluster */
  if (currentPosition > 0)
  {
    currentSector = sectorInCluster(handle, currentPosition);
    if (!currentSector && !(currentPosition & (SECTOR_SIZE - 1)))
      currentSector = 1 << handle->clusterSize;
  }
  else
    currentSector = 0;

  while (dataLength)
  {
    if (currentSector >= (1 << handle->clusterSize))
    {
      /* Try to load the next cluster */
      enum Result res;

      res = getNextCluster(context, handle, &currentCluster);
      if (res == E_EMPTY)
      {
        /* Allocate new cluster when the cluster is empty */
        lockHandle(handle);
        res = allocateCluster(context, handle, &currentCluster);
        unlockHandle(handle);
      }

      if (res != E_OK)
        return res;

      currentSector = 0;
    }

    /* Offset from the beginning of the sector */
    const uint32_t sector = calcSectorNumber(handle, currentCluster)
        + currentSector;
    const uint16_t offset = currentPosition & (SECTOR_SIZE - 1);
    uint16_t chunk;

    if (offset || dataLength < SECTOR_SIZE)
    {
      /* Position within the sector */
      enum Result res;

      chunk = SECTOR_SIZE - offset;
      chunk = MIN(chunk, dataLength);

      res = readSector(context, handle, sector);
      if (res != E_OK)
        return res;

      memcpy(context->buffer + offset, dataBuffer, chunk);

      res = writeSector(context, handle, sector);
      if (res != E_OK)
        return res;

      if (chunk + offset == SECTOR_SIZE)
        ++currentSector;
    }
    else
    {
      /* Position is aligned along the first byte of the sector */
      chunk = ((1 << handle->clusterSize) - currentSector) << SECTOR_EXP;
      chunk = MIN(chunk, dataLength);
      chunk &= ~(SECTOR_SIZE - 1); /* Align along sector boundary */

      /* Write data from the buffer directly without additional copying */
      const enum Result res = writeBuffer(handle, sector, dataBuffer,
          chunk >> SECTOR_EXP);
      if (res != E_OK)
        return res;

      currentSector += chunk >> SECTOR_EXP;
    }

    dataBuffer += chunk;
    dataLength -= chunk;
    currentPosition += chunk;
  }

  if (currentPosition > node->payloadSize)
    node->payloadSize = currentPosition;
  node->currentCluster = currentCluster;
  node->payloadPosition = currentPosition;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeNodeAccess(struct CommandContext *context,
    struct FatNode *node, FsAccess access)
{
  if (!(access & FS_ACCESS_READ))
    return E_VALUE;

  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  enum Result res;

  /* Lock handle to prevent directory modifications from other threads */
  lockHandle(handle);

  res = readSector(context, handle, sector);
  if (res != E_OK)
  {
    unlockHandle(handle);
    return res;
  }

  /* Pointer to the entry position in a sector */
  struct DirEntryImage * const entry = getDirEntry(context, node->parentIndex);
  const uint8_t oldFlags = entry->flags;

  /* Update file access */
  if (access & FS_ACCESS_WRITE)
  {
    entry->flags &= ~FLAG_RO;
    node->flags &= ~FAT_FLAG_RO;
  }
  else
  {
    entry->flags |= FLAG_RO;
    node->flags |= FAT_FLAG_RO;
  }

  if (entry->flags != oldFlags)
    res = writeSector(context, handle, sector);

  unlockHandle(handle);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeNodeData(struct CommandContext *context,
    struct FatNode *node, FsLength position, const void *buffer, size_t length,
    size_t *written)
{
  if (!(node->flags & FAT_FLAG_FILE))
    return E_INVALID;
  if (node->flags & FAT_FLAG_RO)
    return E_ACCESS;
  if (position > node->payloadSize)
    return E_VALUE;

  if (length > FILE_SIZE_MAX - position)
    length = (size_t)(FILE_SIZE_MAX - position);

  if (length)
  {
    struct FatHandle * const handle = (struct FatHandle *)node->handle;
    enum Result res;

    if (!(node->flags & FAT_FLAG_DIRTY))
    {
      lockHandle(handle);
      if (pointerListPushFront(&handle->openedFiles, node))
        res = E_OK;
      else
        res = E_MEMORY;
      unlockHandle(handle);

      if (res == E_OK)
        node->flags |= FAT_FLAG_DIRTY;
      else
        return E_ERROR;
    }

    res = writeClusterChain(context, node, position, buffer, (uint32_t)length);
    if (res == E_OK)
      *written = length;

    return res;
  }
  else
  {
    *written = 0;
    return E_OK;
  }
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeNodeTime(struct CommandContext *context,
    struct FatNode *node, time64_t timestamp)
{
  /* Timestamp writing requires access to the directory entry */
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  const uint32_t sector = calcSectorNumber(handle, node->parentCluster)
      + ENTRY_SECTOR(node->parentIndex);
  enum Result res;

  /* Lock handle to prevent directory modifications from other threads */
  lockHandle(handle);

  res = readSector(context, handle, sector);
  if (res != E_OK)
  {
    unlockHandle(handle);
    return res;
  }

  struct DirEntryImage * const entry = getDirEntry(context, node->parentIndex);
  const uint16_t oldDate = entry->date;
  const uint16_t oldTime = entry->time;
  struct RtDateTime dateTime;

  timestamp /= 1000000;
  rtMakeTime(&dateTime, timestamp);
  entry->date = toLittleEndian16(timeToRawDate(&dateTime));
  entry->time = toLittleEndian16(timeToRawTime(&dateTime));

  if (entry->date != oldDate || entry->time != oldTime)
    res = writeSector(context, handle, sector);

  unlockHandle(handle);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static enum Result writeSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  const uint64_t position = (uint64_t)sector << SECTOR_EXP;
  enum Result res;

  ifSetParam(handle->interface, IF_ACQUIRE, 0);

  res = ifSetParam(handle->interface, IF_POSITION_64, &position);
  if (res == E_OK)
  {
    if (ifWrite(handle->interface, context->buffer, SECTOR_SIZE) == SECTOR_SIZE)
    {
      context->sector = sector;
    }
    else
    {
      res = ifGetParam(handle->interface, IF_STATUS, 0);
      assert(res != E_OK && res != E_INVALID);
    }
  }

  ifSetParam(handle->interface, IF_RELEASE, 0);
  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_POOLS) || defined(CONFIG_THREADS)
static enum Result allocatePool(struct Pool *pool, size_t capacity,
    size_t width, const void *initializer)
{
  uint8_t *data = malloc(width * capacity);

  if (!data)
    return E_MEMORY;

  if (!pointerQueueInit(&pool->queue, capacity))
  {
    free(data);
    return E_MEMORY;
  }

  pool->data = data;
  for (size_t index = 0; index < capacity; ++index)
  {
    if (initializer)
      ((struct Entity *)data)->descriptor = initializer;

    pointerQueuePushBack(&pool->queue, data);
    data += width;
  }

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_POOLS) || defined(CONFIG_THREADS)
static void freePool(struct Pool *pool)
{
  pointerQueueDeinit(&pool->queue);
  free(pool->data);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) || defined(CONFIG_WRITE)
static void allocateStaticNode(struct FatHandle *handle, struct FatNode *node)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };

  /* Initialize class descriptor manually */
  ((struct Entity *)node)->descriptor = FatNode;

  /* Call constructor for the statically allocated object */
  FatNode->init(node, &config);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) || defined(CONFIG_WRITE)
static void freeStaticNode(struct FatNode *node)
{
  FatNode->deinit(node);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
/* Save Unicode characters to long file name entry */
static void fillLongName(struct DirEntryImage *entry, const char16_t *name,
    size_t count)
{
  char16_t buffer[LFN_ENTRY_LENGTH];
  const char16_t *position = buffer;

  memcpy(buffer, name, count * sizeof(char16_t));
  if (count < LFN_ENTRY_LENGTH)
  {
    buffer[count] = '\0';

    if (count + 1 < LFN_ENTRY_LENGTH)
    {
      const size_t padding = LFN_ENTRY_LENGTH - 1 - count;
      memset(&buffer[count + 1], 0xFF, padding * sizeof(char16_t));
    }
  }

  memcpy(entry->longName0, position, sizeof(entry->longName0));
  position += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(entry->longName1, position, sizeof(entry->longName1));
  position += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(entry->longName2, position, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
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
static enum Result fatHandleInit(void *object, const void *configBase)
{
  const struct Fat32Config * const config = configBase;
  struct FatHandle * const handle = object;
  enum Result res;

  res = allocateBuffers(handle, config);
  if (res != E_OK)
    return res;

  handle->interface = config->interface;

  res = mountStorage(handle);
  if (res != E_OK)
    freeBuffers(handle, FREE_ALL);

  return res;
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
  struct FatNode * const node = allocateNode(handle);

  if (!node)
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
static enum Result fatHandleSync(void *object)
{
#ifdef CONFIG_WRITE
  struct FatHandle * const handle = object;
  struct CommandContext * const context = allocateContext(handle);

  if (!context)
    return E_MEMORY;

  lockHandle(handle);

  PointerListNode *current = pointerListFront(&handle->openedFiles);
  enum Result res = E_OK;

  while (current)
  {
    struct FatNode * const node = *pointerListData(current);
    const enum Result latest = syncFile(context, node);

    if (res == E_OK && latest != E_OK)
      res = latest;
    current = pointerListNext(current);
  }

  unlockHandle(handle);
  freeContext(handle, context);

  return res;
#else
  (void)object;
  return E_OK;
#endif
}
/*------------------Node functions--------------------------------------------*/
static enum Result fatNodeInit(void *object, const void *configBase)
{
  const struct FatNodeConfig * const config = configBase;
  struct FatNode * const node = object;

  node->handle = config->handle;

  node->parentCluster = RESERVED_CLUSTER;
  node->parentIndex = 0;

#ifdef CONFIG_UNICODE
  node->nameCluster = 0;
  node->nameIndex = 0;
#endif

  node->payloadSize = 0;
  node->payloadCluster = RESERVED_CLUSTER;

  node->currentCluster = RESERVED_CLUSTER;
  node->payloadPosition = 0;
  node->nameLength = 0;

  node->flags = 0;
  node->lfn = 0;

  DEBUG_PRINT(3, "fat32: node allocated, address %p\n", object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatNodeDeinit(void *object)
{
  struct FatNode * const node = object;

  if (node->flags & FAT_FLAG_DIRTY)
  {
#ifdef CONFIG_WRITE
    struct FatHandle * const handle = (struct FatHandle *)node->handle;
    struct CommandContext * const context = allocateContext(handle);
    enum Result res = E_MEMORY;

    if (context)
    {
      res = syncFile(context, node);
      freeContext(handle, context);
    }

    if (res != E_OK)
    {
      DEBUG_PRINT(1, "fat32: unrecoverable sync error\n");

      /* Clear flag and remove from the list anyway */
      clearDirtyFlag(node);
      return;
    }
#endif
  }

  DEBUG_PRINT(3, "fat32: node freed, address %p\n", object);
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeCreate(void *rootObject,
    const struct FsFieldDescriptor *descriptors, size_t number)
{
#ifdef CONFIG_WRITE
  const struct FatNode * const root = rootObject;
  struct FatHandle * const handle = (struct FatHandle *)root->handle;

  if (!(root->flags & FAT_FLAG_DIR))
    return E_VALUE;
  if (root->flags & FAT_FLAG_RO)
    return E_ACCESS;

  time64_t nodeTime = 0;
  FsAccess nodeAccess = FS_ACCESS_READ | FS_ACCESS_WRITE;

  const struct FsFieldDescriptor *dataDesc = 0;
  const struct FsFieldDescriptor *nameDesc = 0;

  for (size_t index = 0; index < number; ++index)
  {
    const struct FsFieldDescriptor * const desc = descriptors + index;

    switch (desc->type)
    {
      case FS_NODE_ACCESS:
        if (desc->length == sizeof(FsAccess))
        {
          memcpy(&nodeAccess, desc->data, sizeof(FsAccess));
          break;
        }
        else
          return E_VALUE;

      case FS_NODE_DATA:
        dataDesc = desc;
        break;

      case FS_NODE_NAME:
        if (desc->length <= CONFIG_NAME_LENGTH
            && desc->length == strlen(desc->data) + 1)
        {
          nameDesc = desc;
          break;
        }
        else
          return E_VALUE;

      case FS_NODE_TIME:
        if (desc->length == sizeof(nodeTime))
        {
          memcpy(&nodeTime, desc->data, sizeof(nodeTime));
          break;
        }
        else
          return E_VALUE;

      default:
        break;
    }
  }

  if (!nameDesc)
    return E_INVALID; /* Node cannot be left unnamed */

  struct CommandContext * const context = allocateContext(handle);
  uint32_t nodePayloadCluster = RESERVED_CLUSTER;
  enum Result res = E_OK;

  if (context)
  {
    /* Allocate a cluster chain for the directory */
    if (!dataDesc)
    {
      /* Prevent unexpected table modifications from other threads */
      lockHandle(handle);
      res = allocateCluster(context, handle, &nodePayloadCluster);
      unlockHandle(handle);

      if (res == E_OK)
      {
        res = setupDirCluster(context, handle, root->payloadCluster,
            nodePayloadCluster, nodeTime);
      }
    }
    else if (dataDesc->length)
    {
      struct FatNode staticNode;
      allocateStaticNode(handle, &staticNode);

      res = writeClusterChain(context, &staticNode, 0,
          dataDesc->data, dataDesc->length);
      if (res == E_OK)
        nodePayloadCluster = staticNode.payloadCluster;
    }

    /* Create an entry in the parent directory */
    if (res == E_OK)
    {
      const bool isDirNode = dataDesc == 0;

      lockHandle(handle);
      res = createNode(context, root, isDirNode, nameDesc->data,
          nodeAccess, nodePayloadCluster, nodeTime);
      unlockHandle(handle);
    }

    if (res != E_OK && nodePayloadCluster != RESERVED_CLUSTER)
    {
      lockHandle(handle);
      freeChain(context, handle, nodePayloadCluster);
      unlockHandle(handle);
    }

    freeContext(handle, context);
  }
  else
    res = E_MEMORY;

  return res;
#else
  (void)rootObject;
  (void)descriptors;
  (void)number;

  return E_INVALID;
#endif
}
/*----------------------------------------------------------------------------*/
static void *fatNodeHead(void *object)
{
  struct FatNode * const root = object;

  if (!(root->flags & FAT_FLAG_DIR))
    return 0; /* Current node is not directory */

  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  struct FatNode * const node = allocateNode(handle);

  if (!node)
    return 0;

  node->parentCluster = root->payloadCluster;
  node->parentIndex = 0;

  struct CommandContext * const context = allocateContext(handle);
  enum Result res;

  if (context)
  {
    res = fetchNode(context, node);
    freeContext(handle, context);
  }
  else
    res = E_MEMORY;

  if (res != E_OK)
  {
    freeNode(node);
    return 0;
  }
  else
    return node;
}
/*----------------------------------------------------------------------------*/
static void fatNodeFree(void *object)
{
  struct FatNode * const node = object;
  freeNode(node);
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeLength(void *object, enum FsFieldType type,
    FsLength *length)
{
  struct FatNode * const node = object;
  FsLength len = 0;

  switch (type)
  {
    case FS_NODE_ACCESS:
      len = sizeof(FsAccess);
      break;

    case FS_NODE_DATA:
      if (node->flags & FAT_FLAG_FILE)
        len = node->payloadSize;
      else
        return E_INVALID;
      break;

    case FS_NODE_ID:
      len = sizeof(uint64_t);
      break;

    case FS_NODE_NAME:
      len = node->nameLength + 1;
      break;

    case FS_NODE_TIME:
      len = sizeof(time64_t);
      break;

    default:
      return E_INVALID;
  }

  if (length)
    *length = len;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeNext(void *object)
{
  struct FatNode * const node = object;

  if (node->parentCluster == RESERVED_CLUSTER)
    return E_ENTRY;

  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext * const context = allocateContext(handle);
  enum Result res;

  if (context)
  {
    ++node->parentIndex;
    res = fetchNode(context, node);
    freeContext(handle, context);

    if (res == E_EMPTY || res == E_ENTRY)
    {
      /* Reached the end of the directory */
      node->parentCluster = RESERVED_CLUSTER;
      node->parentIndex = 0;
      res = E_ENTRY;
    }
  }
  else
    res = E_MEMORY;

  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeRead(void *object, enum FsFieldType type,
    FsLength position, void *buffer, size_t length, size_t *read)
{
  struct FatNode * const node = object;
  size_t bytesRead = 0;
  enum Result res = E_INVALID;
  bool processed = false;

  /* Read fields that do not require reading from the interface */
  switch (type)
  {
    case FS_NODE_ACCESS:
      if (position > 0 || length < sizeof(FsAccess))
      {
        res = E_VALUE;
      }
      else
      {
        readNodeAccess(node, buffer);
        bytesRead = sizeof(FsAccess);
        res = E_OK;
      }
      processed = true;
      break;

    case FS_NODE_ID:
      if (position > 0 || length < sizeof(uint64_t))
      {
        res = E_VALUE;
      }
      else
      {
        readNodeId(node, buffer);
        bytesRead = sizeof(uint64_t);
        res = E_OK;
      }
      processed = true;
      break;

    case FS_NODE_DATA:
    case FS_NODE_NAME:
    case FS_NODE_TIME:
      break;

    default:
      return E_INVALID;
  }

  if (processed)
  {
    if (res == E_OK && read)
      *read = bytesRead;
    return res;
  }

  /* Read fields that require reading from the interface */
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext * const context = allocateContext(handle);

  if (!context)
    return E_MEMORY;

  if (type == FS_NODE_DATA)
  {
    res = readNodeData(context, node, position, buffer, length, &bytesRead);
  }
  else if (type == FS_NODE_NAME)
  {
    if (position == 0)
      res = readNodeName(context, node, buffer, length, &bytesRead);
    else
      res = E_VALUE;
  }
  else if (type == FS_NODE_TIME)
  {
    if (position > 0 || length < sizeof(time64_t))
    {
      res = E_VALUE;
    }
    else
    {
      res = readNodeTime(context, node, buffer);
      if (res == E_OK)
        bytesRead = sizeof(time64_t);
    }
  }

  freeContext(handle, context);

  if (res == E_OK && read)
    *read = bytesRead;
  return res;
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeRemove(void *rootObject, void *object)
{
#ifdef CONFIG_WRITE
  struct FatNode * const root = rootObject;
  struct FatNode * const node = object;

  if ((root->flags & FAT_FLAG_RO) || (node->flags & FAT_FLAG_RO))
    return E_ACCESS;

  struct FatHandle * const handle = (struct FatHandle *)root->handle;
  struct CommandContext * const context = allocateContext(handle);

  if (!context)
    return E_MEMORY;

  enum Result res = truncatePayload(context, node);

  if (res == E_OK)
  {
    lockHandle(handle);
    res = markFree(context, node);
    unlockHandle(handle);
  }

  freeContext(handle, context);
  return res;
#else
  (void)rootObject;
  (void)object;

  return E_INVALID;
#endif
}
/*----------------------------------------------------------------------------*/
static enum Result fatNodeWrite(void *object, enum FsFieldType type,
    FsLength position, const void *buffer, size_t length, size_t *written)
{
#ifdef CONFIG_WRITE
  switch (type)
  {
    case FS_NODE_ACCESS:
    case FS_NODE_DATA:
    case FS_NODE_TIME:
      break;

    default:
      return E_INVALID;
  }

  struct FatNode * const node = object;
  struct FatHandle * const handle = (struct FatHandle *)node->handle;
  struct CommandContext * const context = allocateContext(handle);

  if (!context)
    return E_MEMORY;

  size_t bytesWritten = 0;
  enum Result res = E_INVALID;

  if (type == FS_NODE_ACCESS)
  {
    if (position > 0 || length < sizeof(FsAccess))
    {
      res = E_VALUE;
    }
    else
    {
      FsAccess access;
      memcpy(&access, buffer, sizeof(access));

      res = writeNodeAccess(context, node, access);
      if (res == E_OK)
        bytesWritten = sizeof(access);
    }
  }
  else if (type == FS_NODE_DATA)
  {
    res = writeNodeData(context, node, position, buffer, length, &bytesWritten);
  }
  else if (type == FS_NODE_TIME)
  {
    if (position > 0 || length < sizeof(time64_t))
    {
      res = E_VALUE;
    }
    else
    {
      time64_t timestamp;
      memcpy(&timestamp, buffer, sizeof(timestamp));

      res = writeNodeTime(context, node, timestamp);
      if (res == E_OK)
        bytesWritten = sizeof(timestamp);
    }
  }

  freeContext(handle, context);

  if (res == E_OK && written)
    *written = bytesWritten;
  return res;
#else
  (void)object;
  (void)type;
  (void)position;
  (void)buffer;
  (void)length;
  (void)written;

  return E_INVALID;
#endif
}
