/*
 * yaf/tests/shared/helpers.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "helpers.h"
#include <yaf/fat32_defs.h>
#include <check.h>
/*----------------------------------------------------------------------------*/
void changeLastAllocatedCluster(void *object, uint32_t number)
{
#ifdef CONFIG_WRITE
  struct FatHandle * const handle = object;
  handle->lastAllocated = number;
#else
  (void)object;
  (void)number;
#endif
}
/*----------------------------------------------------------------------------*/
void changeLfnCount(void *object, uint8_t count)
{
  struct FatNode * const node = object;
  node->lfn = count;
}
/*----------------------------------------------------------------------------*/
PointerQueue drainContextPool(void *object)
{
  struct FatHandle * const handle = object;
  PointerQueue contexts;

  const bool res = pointerQueueInit(&contexts,
      pointerQueueSize(&handle->pools.contexts.queue));
  ck_assert(res);

  while (!pointerQueueEmpty(&handle->pools.contexts.queue))
  {
    void * const pointer = pointerQueueFront(&handle->pools.contexts.queue);
    pointerQueuePopFront(&handle->pools.contexts.queue);
    pointerQueuePushBack(&contexts, pointer);
  }

  return contexts;
}
/*----------------------------------------------------------------------------*/
PointerQueue drainNodePool(void *object)
{
  struct FatHandle * const handle = object;
  PointerQueue nodes;

  const bool res = pointerQueueInit(&nodes,
      pointerQueueSize(&handle->pools.nodes.queue));
  ck_assert(res);

  while (!pointerQueueEmpty(&handle->pools.nodes.queue))
  {
    void * const pointer = pointerQueueFront(&handle->pools.nodes.queue);
    pointerQueuePopFront(&handle->pools.nodes.queue);
    pointerQueuePushBack(&nodes, pointer);
  }

  return nodes;
}
/*----------------------------------------------------------------------------*/
size_t getMaxEntriesPerCluster(const void *object)
{
  const struct FatHandle * const handle = object;
  return 1 << ENTRY_EXP << handle->clusterSize;
}
/*----------------------------------------------------------------------------*/
size_t getMaxEntriesPerSector(void)
{
  return 1 << ENTRY_EXP;
}
/*----------------------------------------------------------------------------*/
size_t getMaxSimilarNamesCount(void)
{
  return MAX_SIMILAR_NAMES;
}
/*----------------------------------------------------------------------------*/
size_t getTableEntriesPerSector(void)
{
  return 1 << CELL_COUNT;
}
/*----------------------------------------------------------------------------*/
void restoreContextPool(void *object, PointerQueue *contexts)
{
  struct FatHandle * const handle = object;

  while (!pointerQueueEmpty(contexts))
  {
    void * const pointer = pointerQueueFront(contexts);
    pointerQueuePopFront(contexts);
    pointerQueuePushBack(&handle->pools.contexts.queue, pointer);
  }

  pointerQueueDeinit(contexts);
}
/*----------------------------------------------------------------------------*/
void restoreNodePool(void *object, PointerQueue *nodes)
{
  struct FatHandle * const handle = object;

  while (!pointerQueueEmpty(nodes))
  {
    void * const pointer = pointerQueueFront(nodes);
    pointerQueuePopFront(nodes);
    pointerQueuePushBack(&handle->pools.nodes.queue, pointer);
  }

  pointerQueueDeinit(nodes);
}
