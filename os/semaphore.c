/*
 * semaphore.c
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <os/semaphore.h>
/*----------------------------------------------------------------------------*/
enum result semInit(struct Semaphore *sem, int value)
{
  sem->handle = malloc(sizeof(sem_t));
  if (!sem->handle)
    return E_MEMORY;
  if (sem_init(sem->handle, 0, value))
    return E_ERROR;

  return E_OK;
}
/*----------------------------------------------------------------------------*/
void semDeinit(struct Semaphore *sem)
{
  sem_destroy(sem->handle);
  free(sem->handle);
}
/*----------------------------------------------------------------------------*/
void semPost(struct Semaphore *sem)
{
  sem_post(sem->handle);
}
/*----------------------------------------------------------------------------*/
bool semTryWait(struct Semaphore *sem, unsigned int interval)
{
  int res;

  if (interval)
  {
    struct timespec timestamp;

    clock_gettime(CLOCK_REALTIME, &timestamp);
    timestamp.tv_sec += interval / 1000;
    timestamp.tv_nsec += (interval % 1000) * 1000000;

    if (timestamp.tv_nsec >= 1000000000)
    {
      timestamp.tv_nsec -= 1000000000;
      ++timestamp.tv_sec;
    }

    res = sem_timedwait(sem->handle, &timestamp);
  }
  else
    res = sem_trywait(sem->handle);

  return res ? false : true;
}
/*----------------------------------------------------------------------------*/
int semValue(struct Semaphore *sem)
{
  int value = 1;

  sem_getvalue(sem->handle, &value);
  return value;
}
/*----------------------------------------------------------------------------*/
void semWait(struct Semaphore *sem)
{
  sem_wait(sem->handle);
}
