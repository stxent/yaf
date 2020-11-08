/*
 * fat32_pools.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <yaf/fat32_defs.h>
#include <yaf/fat32_pools.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
static inline void lockPools(struct FatHandle *);
static inline void unlockPools(struct FatHandle *);
/*----------------------------------------------------------------------------*/
bool allocatePool(struct Pool *pool, size_t capacity, size_t width)
{
  assert(capacity > 0);

  uint8_t *data = malloc(width * capacity);

  if (!data)
    return false;

  if (!pointerQueueInit(&pool->queue, capacity))
  {
    free(data);
    return false;
  }

  pool->data = data;
  data += capacity * width;

  do
  {
    data -= width;
    pointerQueuePushBack(&pool->queue, data);
  }
  while (data != pool->data);

  return true;
}
/*----------------------------------------------------------------------------*/
struct CommandContext *allocatePoolContext(struct FatHandle *handle)
{
  struct CommandContext *context = 0;

  lockPools(handle);
  if (!pointerQueueEmpty(&handle->pools.contexts.queue))
  {
    context = pointerQueueFront(&handle->pools.contexts.queue);
    pointerQueuePopFront(&handle->pools.contexts.queue);
  }
  unlockPools(handle);

  if (context)
    context->sector = RESERVED_SECTOR;
  return context;
}
/*----------------------------------------------------------------------------*/
void *allocatePoolNode(struct FatHandle *handle)
{
  struct FatNode *node = 0;

  lockPools(handle);
  if (!pointerQueueEmpty(&handle->pools.nodes.queue))
  {
    node = pointerQueueFront(&handle->pools.nodes.queue);
    pointerQueuePopFront(&handle->pools.nodes.queue);
  }
  unlockPools(handle);

  if (node)
    allocateStaticNode(handle, node);

  return node;
}
/*----------------------------------------------------------------------------*/
void allocateStaticNode(struct FatHandle *handle, struct FatNode *node)
{
  const struct FatNodeConfig config = {
      .handle = (struct FsHandle *)handle
  };

  /* Initialize class descriptor manually */
  ((struct Entity *)node)->descriptor = FatNode;

  /* Call constructor for the statically allocated object */
  FatNode->init(node, &config);
}
/*----------------------------------------------------------------------------*/
void freePool(struct Pool *pool)
{
  pointerQueueDeinit(&pool->queue);
  free(pool->data);
}
/*----------------------------------------------------------------------------*/
void freePoolContext(struct FatHandle *handle, struct CommandContext *context)
{
  lockPools(handle);
  pointerQueuePushBack(&handle->pools.contexts.queue, context);
  unlockPools(handle);
}
/*----------------------------------------------------------------------------*/
void freePoolNode(struct FatNode *node)
{
  struct FatHandle * const handle = (struct FatHandle *)node->handle;

  freeStaticNode(node);

  lockPools(handle);
  pointerQueuePushBack(&handle->pools.nodes.queue, node);
  unlockPools(handle);
}
/*----------------------------------------------------------------------------*/
void freeStaticNode(struct FatNode *node)
{
  FatNode->deinit(node);
}
/*----------------------------------------------------------------------------*/
static inline void lockPools(struct FatHandle *handle)
{
#ifdef CONFIG_THREADS
  mutexLock(&handle->memoryMutex);
#else
  (void)handle;
#endif
}
/*----------------------------------------------------------------------------*/
static inline void unlockPools(struct FatHandle *handle)
{
#ifdef CONFIG_THREADS
  mutexUnlock(&handle->memoryMutex);
#else
  (void)handle;
#endif
}
