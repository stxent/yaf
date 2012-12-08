/*
 * mutex.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "mutex.h"
/*----------------------------------------------------------------------------*/
void mutexLock(Mutex *m)
{
  pthread_mutex_lock(m);
}
/*----------------------------------------------------------------------------*/
bool mutexTryLock(Mutex *m)
{
  if (pthread_mutex_trylock(m))
    return false;
  else
    return true;
}
/*----------------------------------------------------------------------------*/
void mutexUnlock(Mutex *m)
{
  pthread_mutex_unlock(m);
}
