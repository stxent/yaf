/*
 * entity.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef ENTITY_H_
#define ENTITY_H_
/*----------------------------------------------------------------------------*/
#define CLASS_GENERATOR(name) \
  unsigned int size;\
  enum result (*init)(const struct name *, const void *);\
  void (*deinit)(struct name *);
/*----------------------------------------------------------------------------*/
enum result
{
  E_OK = 0,
  E_ERROR,
  E_IO,
  E_MEM
};
/*----------------------------------------------------------------------------*/
// typedef unsigned int entitySize;
// /*----------------------------------------------------------------------------*/
// /* Class descriptor */
// struct EntityType
// {
//   entitySize size;
//   /* Create object, arguments: pointer to object, constructor parameters */
//   int (*init)(void *, const void *);
//   /* Delete object, arguments: pointer to object */
//   void (*deinit)(void *);
// };
/*----------------------------------------------------------------------------*/
void *init(const void *, const void *);
void deinit(void *);
/*----------------------------------------------------------------------------*/
#endif /* ENTITY_H_ */
