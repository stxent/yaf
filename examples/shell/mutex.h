/*
 * mutex.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef MUTEX_H_
#define MUTEX_H_
/*------------------------------------------------------------------------------*/
#include <stdint.h>
/*------------------------------------------------------------------------------*/
struct Mutex
{
  uint8_t state;
};
/*------------------------------------------------------------------------------*/
void mutexLock(struct Mutex *);
uint8_t mutexTryLock(struct Mutex *);
void mutexRelease(struct Mutex *);
/*------------------------------------------------------------------------------*/
#endif /* MUTEX_H_ */
