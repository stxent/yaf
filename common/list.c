/*
 * list.c
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
/*----------------------------------------------------------------------------*/
enum result listInit(struct List *list, unsigned int width,
    unsigned int capacity)
{
  struct ListNode *node;

  if (!capacity || capacity > USHRT_MAX)
    return E_VALUE;

  list->data = malloc((sizeof(struct ListNode *) + width) * capacity);
  list->first = list->pool = 0;
  list->width = width;

  for (unsigned int pos = 0; pos < capacity; ++pos)
  {
    node = (struct ListNode *)((char *)list->data
        + (sizeof(struct ListNode *) + width) * pos);
    node->next = list->pool;
    list->pool = node;
  }

  return E_OK;
}
/*----------------------------------------------------------------------------*/
void listDeinit(struct List *list)
{
  free(list->data);
}
/*----------------------------------------------------------------------------*/
void listClear(struct List *list)
{
  struct ListNode *current = list->first;

  while (current && current->next)
    current = current->next;

  if (current)
  {
    current->next = list->pool;
    list->pool = current;
  }
}
/*----------------------------------------------------------------------------*/
void listData(struct List *list, const struct ListNode *node, void *element)
{
  memcpy(element, node->data, list->width);
}
/*----------------------------------------------------------------------------*/
struct ListNode *listErase(struct List *list, struct ListNode *node)
{
  struct ListNode *next;

  if (list->first != node)
  {
    struct ListNode *current = list->first;

    while (current->next != node)
      current = current->next;
    current->next = current->next->next;
  }
  else
    list->first = list->first->next;

  next = node->next;
  node->next = list->pool;
  list->pool = node;

  return next;
}
/*----------------------------------------------------------------------------*/
void listPush(struct List *list, const void *element)
{
  struct ListNode *node = list->pool;

  assert(node);

  list->pool = list->pool->next;

  memcpy(node->data, &element, list->width);
  node->next = list->first;
  list->first = node;
}
