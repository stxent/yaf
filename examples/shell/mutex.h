/*
 * mutex.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef MUTEX_H_
#define MUTEX_H_
/*----------------------------------------------------------------------------*/
#include <pthread.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
#define MUTEX_UNLOCKED                  PTHREAD_MUTEX_INITIALIZER
/* #define MUTEX_LOCKED                    PTHREAD_MUTEX_INITIALIZER */
/*----------------------------------------------------------------------------*/
typedef pthread_mutex_t Mutex;
/*----------------------------------------------------------------------------*/
void mutexLock(Mutex *);
bool mutexTryLock(Mutex *);
void mutexUnlock(Mutex *);
/*----------------------------------------------------------------------------*/
#endif /* MUTEX_H_ */
