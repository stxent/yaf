/*
 * fat32.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <stdlib.h>
#include <string.h>
#include <libyaf/fat32.h>
#include <libyaf/fat32_defs.h>
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif
/*----------------------------------------------------------------------------*/
/* Get size of an array placed in structure */
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
    .sync = fatDirSync,
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
    .sync = fatFileSync,
    .tell = fatFileTell,
    .write = fatFileWrite,

    /* Stubs */
    .fetch = fatFileFetch
};
/*----------------------------------------------------------------------------*/
const struct FsHandleClass *FatHandle = (void *)&fatHandleTable;
const struct FsNodeClass *FatNode = (void *)&fatNodeTable;
const struct FsEntryClass *FatDir = (void *)&fatDirTable;
const struct FsEntryClass *FatFile = (void *)&fatFileTable;
/*----------------------------------------------------------------------------*/
static enum result allocatePool(struct Pool *pool, unsigned int capacity,
    unsigned int width, const void *initializer)
{
  uint8_t *data;
  enum result res;

  if (!(data = malloc(width * capacity)))
    return E_MEMORY;

  res = queueInit(&pool->queue, sizeof(struct Entity *), capacity);
  if (res != E_OK)
  {
    free(data);
    return E_MEMORY;
  }

  pool->data = data;
  for (unsigned int index = 0; index < capacity; ++index)
  {
    if (initializer)
      ((struct Entity *)data)->type = (const struct EntityClass *)initializer;

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
#if defined(FAT_POOLS) || defined(FAT_WRITE)
  uint16_t number;
#endif
  enum result res;

#ifdef FAT_THREADS
  if (!config->threads)
    return E_VALUE;

  /* Create context mutex and allocate context pool */
  if ((res = mutexInit(&handle->contextLock)) != E_OK)
    return res;

  /* Allocate context pool */
  res = allocatePool(&handle->contextPool, config->threads,
      sizeof(struct CommandContext), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_LOCK);
    return res;
  }

  /* Custom initialization of default sector values */
  struct CommandContext *contextPtr = handle->contextPool.data;
  for (unsigned int index = 0; index < config->threads; ++index, ++contextPtr)
    contextPtr->sector = RESERVED_SECTOR;

  DEBUG_PRINT("Context pool:   %u\n", (unsigned int)(config->threads
      * (sizeof(struct CommandContext *) + sizeof(struct CommandContext))));

  /* Allocate metadata pool */
  res = allocatePool(&handle->metadataPool, 2 * config->threads,
      sizeof(struct FsMetadata), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_CONTEXT_POOL);
    return res;
  }

  DEBUG_PRINT("Metadata pool:  %u\n", (unsigned int)(2 * config->threads
      * (sizeof(struct FsMetadata *) + sizeof(struct FsMetadata))));
#else
  /* Allocate single context buffer */
  handle->context = malloc(sizeof(struct CommandContext));
  if (!handle->context)
    return E_MEMORY;
  handle->context->sector = RESERVED_SECTOR;

  DEBUG_PRINT("Context pool:   %u\n",
      (unsigned int)sizeof(struct CommandContext));

  /* Allocate metadata pool */
  res = allocatePool(&handle->metadataPool, 2, sizeof(struct FsMetadata), 0);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_CONTEXT_POOL);
    return res;
  }

  DEBUG_PRINT("Metadata pool:  %u\n", (unsigned int)(2
      * (sizeof(struct FsMetadata *) + sizeof(struct FsMetadata))));
#endif /* FAT_THREADS */

#ifdef FAT_WRITE
  number = config->files ? config->files : FILE_POOL_SIZE;
  res = listInit(&handle->openedFiles, sizeof(struct FatFile *), number);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_METADATA_POOL);
    return res;
  }

  DEBUG_PRINT("File register:  %u\n", (unsigned int)(number
      * (sizeof(struct ListNode *) + sizeof(struct FatFile *))));
#endif

#ifdef FAT_POOLS
  /* Allocate and fill node pool */
  number = config->nodes ? config->nodes : NODE_POOL_SIZE;
  res = allocatePool(&handle->nodePool, number, sizeof(struct FatNode),
      FatNode);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_FILE_LIST);
    return res;
  }

  DEBUG_PRINT("Node pool:      %u\n", (unsigned int)(number
      * (sizeof(struct FatNode *) + sizeof(struct FatNode))));

  /* Allocate and fill directory entry pool */
  number = config->directories ? config->directories : DIR_POOL_SIZE;
  res = allocatePool(&handle->dirPool, number, sizeof(struct FatDir), FatDir);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_NODE_POOL);
    return res;
  }

  DEBUG_PRINT("Directory pool: %u\n", (unsigned int)(number
      * (sizeof(struct FatDir *) + sizeof(struct FatDir))));

  /* Allocate and fill file entry pool */
  number = config->files ? config->files : FILE_POOL_SIZE;
  res = allocatePool(&handle->filePool, number, sizeof(struct FatFile),
      FatFile);
  if (res != E_OK)
  {
    freeBuffers(handle, FREE_DIR_POOL);
    return res;
  }

  DEBUG_PRINT("File pool:      %u\n", (unsigned int)(number
      * (sizeof(struct FatFile *) + sizeof(struct FatFile))));
#endif /* FAT_POOLS */

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static struct CommandContext *allocateContext(struct FatHandle *handle)
{
#ifdef FAT_THREADS
  struct CommandContext *context = 0;

  mutexLock(&handle->contextLock);
  if (!queueEmpty(&handle->contextPool.queue))
    queuePop(&handle->contextPool.queue, &context);
  mutexUnlock(&handle->contextLock);

  return context;
#else
  return handle->context;
#endif
}
/*----------------------------------------------------------------------------*/
static void *allocateNode(struct FatHandle *handle)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };
  struct FatNode *node = 0;

  lockHandle(handle);
#ifdef FAT_POOLS
  if (!queueEmpty(&handle->nodePool.queue))
  {
    queuePop(&handle->nodePool.queue, &node);
    FatNode->init(node, &config);
  }
#else
  node = init(FatNode, &config);
#endif
  unlockHandle(handle);

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
/* Fields node->index and node->cluster have to be initialized */
static enum result fetchEntry(struct CommandContext *context,
    struct FatNode *node)
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
      if ((res = getNextCluster(context, handle, &node->cluster)) != E_OK)
      {
        /* Set index to the last entry in last existing cluster */
        node->index = nodeCount(handle) - 1;
        return res;
      }
      node->index = 0;
    }

    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    if ((res = readSector(context, handle, sector)) != E_OK)
      return res;
    ptr = (struct DirEntryImage *)(context->buffer + E_OFFSET(node->index));

    /* Check for the end of the directory */
    if (!ptr->name[0])
      return E_ENTRY;

    /* Volume entries are ignored */
    if (!((ptr->flags & FLAG_VOLUME) && (ptr->flags & MASK_LFN) != MASK_LFN))
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
/* Fields cluster and index should be initialized */
static enum result fetchNode(struct CommandContext *context,
    struct FatNode *node, struct FsMetadata *metadata)
{
  struct DirEntryImage *ptr;
  enum result res;
#ifdef FAT_LFN
  uint8_t checksum;
  uint8_t chunks = 0; /* LFN chunks required */
  uint8_t found = 0; /* LFN chunks found */
#endif

  while ((res = fetchEntry(context, node)) == E_OK)
  {
    /* There is no need to reload sector in current context */
    ptr = (struct DirEntryImage *)(context->buffer + E_OFFSET(node->index));

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

  if (metadata)
  {
    metadata->type = node->type;
#ifdef FAT_LFN
    if (hasLongName(node))
    {
      if ((res = readLongName(context, node, metadata->name)) != E_OK)
        return res;
    }
    else
      extractShortName(ptr, metadata->name);
#else
    extractShortName(ptr, metadata->name);
#endif
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static const char *followPath(struct CommandContext *context,
    struct FatNode *node, const char *path, const struct FatNode *root)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct FsMetadata *currentNode = 0, *pathPart = 0;

  /* Allocate temporary metadata buffers */
  lockHandle(handle);
  if (queueSize(&handle->metadataPool.queue) >= 2)
  {
    queuePop(&handle->metadataPool.queue, &currentNode);
    queuePop(&handle->metadataPool.queue, &pathPart);
  }
  unlockHandle(handle);

  if (!currentNode || !pathPart)
    return 0;

  path = getChunk(path, pathPart->name);
  if (!strlen(pathPart->name))
  {
    path = 0;
    goto exit;
  }

  node->index = 0;
  if (!root)
  {
    if (pathPart->name[0] == '/')
    {
      node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
      node->cluster = RESERVED_CLUSTER;
      node->payload = ((struct FatHandle *)node->handle)->rootCluster;
      node->type = FS_TYPE_DIR;
      goto exit;
    }
    node->cluster = node->payload;
  }
  else
    node->cluster = root->payload;

  while (fetchNode(context, node, currentNode) == E_OK)
  {
    if (!strcmp(pathPart->name, currentNode->name))
      goto exit;
    ++node->index;
  }

  path = 0;

exit:
  /* Return buffers to metadata pool */
  lockHandle(handle);
  queuePush(&handle->metadataPool.queue, pathPart);
  queuePush(&handle->metadataPool.queue, currentNode);
  unlockHandle(handle);

  return path;
}
/*----------------------------------------------------------------------------*/
static void freeBuffers(struct FatHandle *handle, enum cleanup step)
{
  switch (step)
  {
    case FREE_ALL:
    case FREE_FILE_POOL:
#ifdef FAT_POOLS
      freePool(&handle->filePool);
#endif
    case FREE_DIR_POOL:
#ifdef FAT_POOLS
      freePool(&handle->dirPool);
#endif
    case FREE_NODE_POOL:
#ifdef FAT_POOLS
      freePool(&handle->nodePool);
#endif
    case FREE_FILE_LIST:
#ifdef FAT_WRITE
      listDeinit(&handle->openedFiles);
#endif
    case FREE_METADATA_POOL:
#ifdef FAT_LFN
      freePool(&handle->metadataPool);
#endif
    case FREE_CONTEXT_POOL:
#ifdef FAT_THREADS
      freePool(&handle->contextPool);
#else
      free(handle->context);
#endif
    case FREE_LOCK:
#ifdef FAT_THREADS
      mutexDeinit(&handle->contextLock);
#endif
      break;
    default:
      break;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_THREADS
static void freeContext(struct FatHandle *handle,
    const struct CommandContext *context)
{
  mutexLock(&handle->contextLock);
  queuePush(&handle->contextPool.queue, context);
  mutexUnlock(&handle->contextLock);
}
#else
static void freeContext(struct FatHandle *handle __attribute__((unused)),
    const struct CommandContext *context __attribute__((unused)))
{

}
#endif
/*----------------------------------------------------------------------------*/
/* Output string buffer should be at least FS_NAME_LENGTH characters long */
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
static enum result getNextCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
{
  enum result res = readSector(context, handle, handle->tableSector
      + (*cluster >> TE_COUNT));
  if (res != E_OK)
    return res;

  uint32_t nextCluster = *(uint32_t *)(context->buffer + TE_OFFSET(*cluster));
  if (clusterUsed(nextCluster))
  {
    *cluster = nextCluster;
    return E_OK;
  }

  return E_FAULT;
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
    goto error;

  struct BootSectorImage *boot = (struct BootSectorImage *)context->buffer;

  /* Check boot sector signature (55AA at 0x01FE) */
  if (boot->bootSignature != 0xAA55)
  {
    res = E_ERROR;
    goto error;
  }

  /* Check sector size, fixed size of 2 ^ SECTOR_POW allowed */
  if (boot->bytesPerSector != SECTOR_SIZE)
  {
    res = E_ERROR;
    goto error;
  }

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
  handle->tableNumber = boot->fatCopies;
  handle->tableSize = boot->fatSize;
  handle->clusterCount = ((boot->partitionSize
      - handle->dataSector) >> handle->clusterSize) + 2;
  handle->infoSector = boot->infoSector;

  DEBUG_PRINT("Info sector:    %u\n", (unsigned int)handle->infoSector);
  DEBUG_PRINT("Table copies:   %u\n", (unsigned int)handle->tableNumber);
  DEBUG_PRINT("Table size:     %u\n", (unsigned int)handle->tableSize);
  DEBUG_PRINT("Cluster count:  %u\n", (unsigned int)handle->clusterCount);
  DEBUG_PRINT("Sectors count:  %u\n", (unsigned int)boot->partitionSize);

  /* Read information sector */
  if ((res = readSector(context, handle, handle->infoSector)) != E_OK)
    goto error;
  struct InfoSectorImage *info = (struct InfoSectorImage *)context->buffer;

  /* Check info sector signatures (RRaA at 0x0000 and rrAa at 0x01E4) */
  if (info->firstSignature != 0x41615252 || info->infoSignature != 0x61417272)
    goto error;
  handle->lastAllocated = info->lastAllocated;

  DEBUG_PRINT("Free clusters:  %u\n", (unsigned int)info->freeClusters);
#endif

  return E_OK;

error:
  freeContext(handle, context);
  return res;
}
/*----------------------------------------------------------------------------*/
static enum result readBuffer(struct FatHandle *handle, uint32_t sector,
    uint8_t *buffer, uint32_t count)
{
  const uint64_t address = (uint64_t)sector << SECTOR_POW;
  const uint32_t length = count << SECTOR_POW;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifRead(handle->interface, buffer, length) != length)
    return E_INTERFACE;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static enum result readSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  if (context->sector == sector)
    return E_OK;

  const uint64_t address = (uint64_t)sector << SECTOR_POW;
  const uint32_t length = SECTOR_SIZE;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifRead(handle->interface, context->buffer, length) != length)
    return E_INTERFACE;
  context->sector = sector;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_LFN
/* Extract 13 Unicode characters from long file name entry */
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
static enum result readLongName(struct CommandContext *context,
    struct FatNode *node, char *name)
{
  uint32_t cluster = node->cluster; /* Preserve cluster and index values */
  uint16_t index = node->index;
  uint8_t chunks = 0;
  enum fsNodeType type = node->type;
  enum result res;

  node->cluster = node->nameCluster;
  node->index = node->nameIndex;

  while ((res = fetchEntry(context, node)) == E_OK)
  {
    /* Sector is already loaded during entry fetching */
    struct DirEntryImage *ptr = (struct DirEntryImage *)(context->buffer
        + E_OFFSET(node->index));

    if ((ptr->flags & MASK_LFN) != MASK_LFN)
      break;

    uint16_t offset = ((ptr->ordinal & ~LFN_LAST) - 1);

    /* Compare resulting file name length and name buffer capacity */
    if (offset > ((FS_NAME_LENGTH - 1) / LFN_ENTRY_LENGTH / 2) - 1)
      return E_MEMORY;

    extractLongName(ptr, (char16_t *)name + offset * LFN_ENTRY_LENGTH);
    ++chunks;
    ++node->index;
  }

  /*
   * Long file name entries always precede data entry thus
   * processing of return values others than successful result is not needed.
   */
  if (res == E_OK)
  {
    if (chunks)
      uFromUtf16(name, (char16_t *)name, FS_NAME_LENGTH);
    else
      res = E_ENTRY;
  }

  /* Restore values changed during directory processing */
  node->cluster = cluster;
  node->index = index;
  node->type = type;

  return res;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result allocateCluster(struct CommandContext *context,
    struct FatHandle *handle, uint32_t *cluster)
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

    res = readSector(context, handle, handle->tableSector
        + (current >> TE_COUNT));
    if (res != E_OK)
      return res;

    /* Check whether the cluster is free */
    uint16_t offset = (current & ((1 << TE_COUNT) - 1)) << 2;
    if (clusterFree(*(uint32_t *)(context->buffer + offset)))
    {
      /* Mark cluster as busy */
      *(uint32_t *)(context->buffer + offset) = CLUSTER_EOC_VAL;

      /*
       * Save changes to allocation table when reference is not available
       * or cluster reference located in another sector.
       */
      if (!*cluster || (*cluster >> TE_COUNT != current >> TE_COUNT))
      {
        if ((res = updateTable(context, handle, current >> TE_COUNT)) != E_OK)
          return res;
      }

      /* Update reference cluster when reference is available */
      if (*cluster)
      {
        res = readSector(context, handle, handle->tableSector
            + (*cluster >> TE_COUNT));
        if (res != E_OK)
          return res;

        *(uint32_t *)(context->buffer + TE_OFFSET(*cluster)) = current;
        if ((res = updateTable(context, handle, *cluster >> TE_COUNT)) != E_OK)
          return res;
      }

      DEBUG_PRINT("Allocated new cluster: %u, source %u\n", current, *cluster);
      handle->lastAllocated = current;
      *cluster = current;

      /* Update information sector */
      if ((res = readSector(context, handle, handle->infoSector)))
        return res;

      struct InfoSectorImage *info = (struct InfoSectorImage *)context->buffer;
      info->lastAllocated = current;
      --info->freeClusters;

      if ((res = writeSector(context, handle, handle->infoSector)))
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
#ifdef FAT_WRITE
static enum result createNode(struct CommandContext *context,
    struct FatNode *node, const struct FatNode *root,
    const struct FsMetadata *metadata)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct DirEntryImage *ptr;
  uint32_t sector;
  uint8_t chunks = 0;
  char shortName[sizeof(ptr->filename)];
  enum result res;

#ifdef FAT_LFN
  struct FsMetadata *nameBuffer = 0;
  uint8_t checksum;
#endif

#ifdef FAT_LFN
  /* Allocate temporary metadata buffer */
  lockHandle(handle);
  if (!queueEmpty(&handle->metadataPool.queue))
    queuePop(&handle->metadataPool.queue, &nameBuffer);
  unlockHandle(handle);
  if (!nameBuffer)
    return E_MEMORY;
#endif

  /* TODO Check for duplicates */
  res = fillShortName(shortName, metadata->name);
  if ((metadata->type & FS_TYPE_DIR))
    memset(shortName + sizeof(ptr->name), ' ', sizeof(ptr->extension));

#ifdef FAT_LFN
  /* Check whether the file name is valid for use as short name */
  if (res != E_OK)
  {
    uint16_t length = 1 + uToUtf16((char16_t *)nameBuffer->name,
        metadata->name, FS_NAME_LENGTH);

    chunks = length / LFN_ENTRY_LENGTH;
    if (length > chunks * LFN_ENTRY_LENGTH) /* When fractional part exists */
      ++chunks;
    checksum = getChecksum(shortName, sizeof(ptr->filename));
  }
#endif

  /* Find suitable space within the directory */
  if ((res = findGap(context, node, root, chunks + 1)) != E_OK)
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
    if ((res = readSector(context, handle, sector)) != E_OK)
      break;
    ptr = (struct DirEntryImage *)(context->buffer + E_OFFSET(node->index));

    if (!current)
      break;

#ifdef FAT_LFN
    fillLongName(ptr, (char16_t *)nameBuffer->name + (current - 1)
        * LFN_ENTRY_LENGTH);
    fillLongNameEntry(ptr, current, chunks, checksum);

    --current;
    ++node->index;

    if (!(node->index & (E_POW - 1)))
    {
      /* Write back updated sector when switching sectors */
      if ((res = writeSector(context, handle, sector)) != E_OK)
        break;
    }
#endif
  }
  while ((res = fetchEntry(context, node)) == E_OK);

#ifdef FAT_LFN
  /* Return buffer to metadata pool */
  lockHandle(handle);
  queuePush(&handle->metadataPool.queue, nameBuffer);
  unlockHandle(handle);
#endif

  if (res != E_OK)
    return res;

  /* Fill uninitialized node fields */
  node->access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  node->type = metadata->type;
  memcpy(ptr->filename, shortName, sizeof(ptr->filename));
  /* Node data pointer is already initialized */

  fillDirEntry(ptr, node);

  if ((res = writeSector(context, handle, sector)) != E_OK)
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

  ptr->clusterHigh = (uint16_t)(node->payload >> 16);
  ptr->clusterLow = (uint16_t)node->payload;
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
static enum result findGap(struct CommandContext *context, struct FatNode *node,
    const struct FatNode *root, uint8_t chainLength)
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
    switch ((res = fetchEntry(context, node)))
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
        if ((res = allocateCluster(context, handle, &node->cluster)) != E_OK)
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

    /*
     * Entry processing will be executed only after entry fetching so
     * in this case sector reloading is redundant.
     */
    struct DirEntryImage *ptr = (struct DirEntryImage *)(context->buffer
        + E_OFFSET(node->index));

    /* Empty node, deleted node or deleted long file name node */
    if (((ptr->flags & MASK_LFN) == MASK_LFN && (ptr->ordinal & LFN_DELETED))
        || !ptr->name[0] || ptr->name[0] == E_FLAG_EMPTY)
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
#ifdef FAT_WRITE
static enum result freeChain(struct CommandContext *context,
    struct FatHandle *handle, uint32_t cluster)
{
  uint32_t current = cluster, next, released = 0;
  enum result res;

  if (current == RESERVED_CLUSTER)
    return E_OK; /* Already empty */

  while (clusterUsed(current))
  {
    /* Get FAT sector with next cluster value */
    res = readSector(context, handle, handle->tableSector
        + (current >> TE_COUNT));
    if (res != E_OK)
      return res;

    next = *(uint32_t *)(context->buffer + TE_OFFSET(current));
    *(uint32_t *)(context->buffer + TE_OFFSET(current)) = 0;

    /* Update table when switching table sectors */
    if (current >> TE_COUNT != next >> TE_COUNT)
    {
      if ((res = updateTable(context, handle, current >> TE_COUNT)) != E_OK)
        return res;
    }

    ++released;
    DEBUG_PRINT("Cleared cluster: %u\n", current);
    current = next;
  }

  /* Update information sector */
  if ((res = readSector(context, handle, handle->infoSector)) != E_OK)
    return res;

  struct InfoSectorImage *info = (struct InfoSectorImage *)context->buffer;
  /* Set free clusters count */
  info->freeClusters += released;

  if ((res = writeSector(context, handle, handle->infoSector)) != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result markFree(struct CommandContext *context,
    struct FatNode *node)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  uint32_t cluster = node->cluster;
  uint32_t lastSector;
  uint16_t index = node->index;
  bool lastEntry = false;
  enum fsNodeType type = node->type;
  enum result res;

  lastSector = getSector(handle, node->cluster) + E_SECTOR(node->index);
#ifdef FAT_LFN
  node->index = node->nameIndex;
  node->cluster = node->nameCluster;
#endif

  while (!lastEntry && (res = fetchEntry(context, node)) == E_OK)
  {
    uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
    lastEntry = sector == lastSector && node->index == index;
    /* Sector is already loaded */
    struct DirEntryImage *ptr = (struct DirEntryImage *)(context->buffer
        + E_OFFSET(node->index));

    /* Mark entry as empty by changing first byte of the name */
    ptr->name[0] = E_FLAG_EMPTY;

    if (lastEntry || !(node->index & (E_POW - 1)))
    {
      /* Write back updated sector when switching sectors or last entry freed */
      if ((res = writeSector(context, handle, sector)) != E_OK)
        break;
    }

    ++node->index;
  }

  /* Restore node state changed during entry fetching */
  node->index = index;
  node->cluster = cluster;
  node->type = type;

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
static enum result setupDirCluster(struct CommandContext *context,
    const struct FatNode *node)
{
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  enum result res;

  if ((res = clearCluster(context, handle, node->payload)) != E_OK)
    return res;

  struct DirEntryImage *ptr;
  /* Current directory entry . */
  ptr = (struct DirEntryImage *)context->buffer;
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

  res = writeSector(context, handle, getSector(handle, node->payload));
  if (res != E_OK)
    return res;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
/* Copy current sector into FAT sectors located at offset */
static enum result updateTable(struct CommandContext *context,
    struct FatHandle *handle, uint32_t offset)
{
  for (uint8_t fat = 0; fat < handle->tableNumber; ++fat)
  {
    enum result res = writeSector(context, handle, offset + handle->tableSector
        + handle->tableSize * fat);
    if (res != E_OK)
      return res;
  }
  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result writeBuffer(struct FatHandle *handle,
    uint32_t sector, const uint8_t *buffer, uint32_t count)
{
  const uint64_t address = (uint64_t)sector << SECTOR_POW;
  const uint32_t length = count << SECTOR_POW;
  enum result res;

  if ((res = ifSet(handle->interface, IF_ADDRESS, &address)) != E_OK)
    return res;
  if (ifWrite(handle->interface, buffer, length) != length)
    return E_INTERFACE;

  return E_OK;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result writeSector(struct CommandContext *context,
    struct FatHandle *handle, uint32_t sector)
{
  const uint64_t address = (uint64_t)sector << SECTOR_POW;
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
#if defined(FAT_LFN) && defined(FAT_WRITE)
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
#if defined(FAT_LFN) && defined(FAT_WRITE)
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
  struct FatHandle *handle = object;
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
  struct FatHandle *handle =
      (struct FatHandle *)((struct FatNode *)object)->handle;

  lockHandle(handle);
#ifdef FAT_POOLS
  FatNode->deinit(object);
  queuePush(&handle->nodePool.queue, object);
#else
  deinit(object);
#endif
  unlockHandle(handle);
}
/*----------------------------------------------------------------------------*/
static enum result fatGet(void *object, enum fsNodeData type, void *data)
{
  const struct DirEntryImage *ptr;
  struct FatNode *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  switch (type)
  {
#ifdef FAT_LFN
    case FS_NODE_METADATA:
    {
      if (!hasLongName(node))
        break; /* Short name cannot be read without sector reload */
      if (!(context = allocateContext(handle)))
        return E_MEMORY;

      struct FsMetadata *metadata = data;

      metadata->type = node->type;
      res = readLongName(context, node, metadata->name);
      freeContext(handle, context);
      return res;
    }
    case FS_NODE_NAME:
    {
      if (!hasLongName(node))
        break;
      if (!(context = allocateContext(handle)))
        return E_MEMORY;

      res = readLongName(context, node, (char *)data);
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
      + E_SECTOR(node->index);
  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    freeContext(handle, context);
    return res;
  }
  ptr = (struct DirEntryImage *)(context->buffer + E_OFFSET(node->index));

  res = E_OK;
  switch (type)
  {
    case FS_NODE_METADATA:
    {
      struct FsMetadata *metadata = data;

      metadata->type = node->type;
      extractShortName(ptr, metadata->name);
      break;
    }
    case FS_NODE_NAME:
    {
      extractShortName(ptr, (char *)data);
      break;
    }
    case FS_NODE_SIZE:
    {
      *(uint64_t *)data = (uint64_t)ptr->size;
      break;
    }
#ifdef FAT_TIME
    case FS_NODE_TIME:
    {
      struct Time time;

      time.sec = ptr->time & 0x1F;
      time.min = (ptr->time >> 5) & 0x3F;
      time.hour = (ptr->time >> 11) & 0x1F;
      time.day = ptr->date & 0x1F;
      time.mon = (ptr->date >> 5) & 0x0F;
      time.year = ((ptr->date >> 9) & 0x7F) + 1980;
      *(int64_t *)data = unixTime(&time); //FIXME Rewrite
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
#ifdef FAT_WRITE
static enum result fatLink(void *object, const struct FsMetadata *metadata,
    const void *targetPtr, void *result)
{
  const struct FatNode *target = targetPtr;
  struct FatNode *allocatedNode, *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!result)
  {
    if (!(allocatedNode = allocateNode((struct FatHandle *)node->handle)))
      return E_MEMORY;
  }
  else
    allocatedNode = result; /* Use a node provided by user */

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  allocatedNode->payload = target->payload;

  res = createNode(context, allocatedNode, node, metadata);
  freeContext(handle, context);

  if (res != E_OK)
  {
    fatFree(allocatedNode);
    return res;
  }

  if (!result)
    fatFree(allocatedNode);

  return E_OK;
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
#ifdef FAT_WRITE
static enum result fatMake(void *object, const struct FsMetadata *metadata,
    void *result)
{
  struct FatNode *allocatedNode, *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!result)
  {
    if (!(allocatedNode = allocateNode(handle)))
      return E_MEMORY;
  }
  else
    allocatedNode = result; /* Use a node provided by user */

  if (!(context = allocateContext(handle)))
  {
    res = E_MEMORY;
    goto free_node;
  }

  allocatedNode->payload = RESERVED_CLUSTER;
  if (metadata->type == FS_TYPE_DIR)
  {
    res = allocateCluster(context, handle, &allocatedNode->payload);
    if (res != E_OK)
      goto free_context;
  }

  if ((res = createNode(context, allocatedNode, node, metadata)) != E_OK)
    goto creation_error;

  if (metadata->type == FS_TYPE_DIR)
  {
    if ((res = setupDirCluster(context, allocatedNode)) != E_OK)
      goto setup_error;
  }

  goto free_context;

setup_error:
  markFree(context, allocatedNode);
creation_error:
  freeChain(context, handle, allocatedNode->payload);
free_context:
  freeContext(handle, context);
free_node:
  if (!result)
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
      struct FatHandle *handle = (struct FatHandle *)node->handle;
      struct FatDir *dir = 0;

      lockHandle(handle);
#ifdef FAT_POOLS
      if (!queueEmpty(&handle->dirPool.queue))
      {
        queuePop(&handle->dirPool.queue, &dir);
        FatDir->init(dir, &config);
      }
#else
      dir = init(FatDir, &config);
#endif
      unlockHandle(handle);

      return dir;
    }
    case FS_TYPE_FILE:
    {
      struct FatFileConfig config = {
          .node = object,
          .access = access
      };
      struct FatHandle *handle = (struct FatHandle *)node->handle;
      struct FatFile *file = 0;
      uint64_t size;

      if (fatGet(node, FS_NODE_SIZE, &size) != E_OK)
        return 0;

      lockHandle(handle);
#ifdef FAT_POOLS
      if (!queueEmpty(&handle->filePool.queue))
      {
        queuePop(&handle->filePool.queue, &file);
        FatFile->init(file, &config);
      }
#else
      file = init(FatFile, &config);
#endif
      unlockHandle(handle);

      if (file)
        file->size = (uint32_t)size;

      return file;
    }
    default:
      return 0;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef FAT_WRITE
static enum result fatSet(void *object, enum fsNodeData type, const void *data)
{
  struct FatNode *node = object;
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  uint32_t sector = getSector(handle, node->cluster) + E_SECTOR(node->index);
  if ((res = readSector(context, handle, sector)) != E_OK)
    return res;
  struct DirEntryImage *ptr = (struct DirEntryImage *)(context->buffer
      + E_OFFSET(node->index));

  switch (type)
  {
    case FS_NODE_ACCESS:
    {
      node->access = *(access_t *)data;
      if (node->access & FS_ACCESS_WRITE)
        ptr->flags &= ~FLAG_RO;
      else
        ptr->flags |= FLAG_RO;
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
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_ERROR;

  if (node->type == FS_TYPE_DIR && node->payload != RESERVED_CLUSTER)
  {
    /* Preserve values */
    uint32_t cluster = node->cluster;
    uint16_t index = node->index;
    enum fsNodeType type = node->type;

    /* Check if directory not empty */
    node->cluster = node->payload;
    node->index = 2; /* Exclude . and .. */

    res = fetchNode(context, node, 0);
    if (res == E_OK)
      res = E_VALUE;
    if (res != E_ENTRY && res != E_FAULT)
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
  res = freeChain(context, handle, node->payload);
  if (res != E_OK)
    return res;
  freeContext(handle, context);

  node->payload = RESERVED_CLUSTER;
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
  struct FatHandle *handle = (struct FatHandle *)node->handle;
  struct CommandContext *context;
  enum result res;

  if (!(node->access & FS_ACCESS_WRITE))
    return E_ACCESS;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  res = markFree(context, node);
  freeContext(handle, context);
  if (res != E_OK)
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
  struct FatHandle *handle =
      (struct FatHandle *)((struct FatNode *)object)->handle;

  lockHandle(handle);
#ifdef FAT_POOLS
  FatDir->deinit(object);
  queuePush(&handle->dirPool.queue, object);
#else
  deinit(object);
#endif
  unlockHandle(handle);

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
  struct FatHandle *handle = (struct FatHandle *)dir->handle;
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
    if (res == E_ENTRY || res == E_FAULT)
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
  struct FatDir *dir = object;

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
static uint64_t fatDirTell(void *object)
{
  struct FatDir *dir = object;

  return (uint64_t)dir->currentIndex | ((uint64_t)dir->currentCluster << 16);
}
/*------------------File functions--------------------------------------------*/
static enum result fatFileInit(void *object, const void *configPtr)
{
  const struct FatFileConfig * const config = configPtr;
  const struct FatNode *node = (struct FatNode *)config->node;
  struct FatFile *file = object;

  if (node->type != FS_TYPE_FILE)
    return E_VALUE;

  if (config->access & FS_ACCESS_WRITE)
  {
#ifdef FAT_WRITE
    struct FatHandle *handle = (struct FatHandle *)node->handle;

    if (listFull(&handle->openedFiles))
      return E_MEMORY; /* Descriptor list is full */

    listPush(&handle->openedFiles, file);
    DEBUG_PRINT("File descriptor inserted\n");
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
#ifdef FAT_WRITE
  file->parentCluster = node->cluster;
  file->parentIndex = node->index;
#endif
  DEBUG_PRINT("File allocated, address %08lX\n", (unsigned long)object);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void fatFileDeinit(void *object)
{
  struct FatFile *file = object;

  if (file->access & FS_ACCESS_WRITE)
  {
#ifdef FAT_WRITE
    struct FatHandle *handle = (struct FatHandle *)file->handle;
    struct FatFile *descriptor;
    struct ListNode *current;

    current = listFirst(&handle->openedFiles);
    while (current)
    {
      listData(&handle->openedFiles, current, &descriptor);
      if (descriptor == file)
      {
        listErase(&handle->openedFiles, current);
        DEBUG_PRINT("File descriptor found and erased\n");
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
#ifdef FAT_WRITE
  if (((struct FatFile *)object)->access & FS_ACCESS_WRITE)
    fatFileSync(object);
#endif

  struct FatHandle *handle =
      (struct FatHandle *)((struct FatNode *)object)->handle;

  lockHandle(handle);
#ifdef FAT_POOLS
  FatFile->deinit(object);
  queuePush(&handle->filePool.queue, object);
#else
  deinit(object);
#endif
  unlockHandle(handle);

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
    uint16_t offset = (file->position + read) & (SECTOR_SIZE - 1);
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
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_POW);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Read data to the buffer directly without additional copying */
      if (readBuffer(handle, getSector(handle, file->currentCluster)
          + current, (uint8_t *)buffer + read, chunk >> SECTOR_POW) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      current += chunk >> SECTOR_POW;
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
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;
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

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  while (--clusterCount)
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
#ifdef FAT_WRITE
static enum result fatFileSync(void *object)
{
  struct FatFile *file = object;
  struct FatHandle *handle = (struct FatHandle *)file->handle;
  struct CommandContext *context;
  uint32_t sector;
  enum result res;

  if (!(file->access & FS_ACCESS_WRITE))
    return E_ERROR;

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  sector = getSector(handle, file->parentCluster) + E_SECTOR(file->parentIndex);
  if ((res = readSector(context, handle, sector)) != E_OK)
  {
    freeContext(handle, context);
    return res;
  }

  /* Pointer to entry position in sector */
  struct DirEntryImage *ptr = (struct DirEntryImage *)(context->buffer
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

  res = writeSector(context, handle, sector);
  freeContext(handle, context);

  return res;
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
    if (allocateCluster(context, handle, &file->payload) != E_OK)
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
      if ((res != E_FAULT && res != E_OK) || (res == E_FAULT
          && allocateCluster(context, handle, &file->currentCluster) != E_OK))
      {
        freeContext(handle, context);
        return 0;
      }
      current = 0;
    }

    /* Offset from the beginning of the sector */
    uint16_t offset = (file->position + written) & (SECTOR_SIZE - 1);
    if (offset || length < SECTOR_SIZE) /* Position within the sector */
    {
      /* Length of remaining sector space */
      chunk = SECTOR_SIZE - offset;
      chunk = (length < chunk) ? length : chunk;

      uint32_t sector = getSector(handle, file->currentCluster) + current;
      if (readSector(context, handle, sector) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      memcpy(context->buffer + offset, (uint8_t *)buffer + written, chunk);
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
      chunk = (SECTOR_SIZE << handle->clusterSize) - (current << SECTOR_POW);
      chunk = (length < chunk) ? length & ~(SECTOR_SIZE - 1) : chunk;

      /* Write data from the buffer directly without additional copying */
      if (writeBuffer(handle, getSector(handle, file->currentCluster) + current,
          (uint8_t *)buffer + written, chunk >> SECTOR_POW) != E_OK)
      {
        freeContext(handle, context);
        return 0;
      }

      current += chunk >> SECTOR_POW;
    }

    written += chunk;
    length -= chunk;
  }

  freeContext(handle, context);
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
  struct CommandContext *context;
  uint32_t *count = malloc(sizeof(uint32_t) * handle->tableNumber);
  uint32_t empty = 0;

  if (!count)
    return 0; /* Memory allocation problem */

  if (!(context = allocateContext(handle)))
    return E_MEMORY;

  for (uint8_t fat = 0; fat < handle->tableNumber; ++fat)
  {
    count[fat] = 0;
    for (uint32_t current = 0; current < handle->clusterCount; ++current)
    {
      if (readSector(context, handle, handle->tableSector
          + (current >> TE_COUNT)) != E_OK)
      {
        freeContext(handle, context);
        return empty;
      }
      uint16_t offset = (current & ((1 << TE_COUNT) - 1)) << 2;
      if (clusterFree(*(uint32_t *)(context->buffer + offset)))
        ++count[fat];
    }
  }
  for (uint8_t i = 0; i < handle->tableNumber; ++i)
    for (uint8_t j = 0; j < handle->tableNumber; ++j)
      if (i != j && count[i] != count[j])
      {
        DEBUG_PRINT("FAT records count differs: %u and %u\n", count[i],
            count[j]);
      }
  empty = count[0];

  freeContext(handle, context);
  free(count);

  return empty;
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
static enum result fatDirSync(void *object __attribute__((unused)))
{
  return E_ERROR;
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
    void *nodePtr __attribute__((unused)))
{
  return E_ERROR;
}
