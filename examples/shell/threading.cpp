/*
 * threading.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cassert>
#include <cstring>
#include "threading.hpp"
//------------------------------------------------------------------------------
void workerThreadWrapper(void *argument)
{
  reinterpret_cast<WorkerThread *>(argument)->handler();
}
//------------------------------------------------------------------------------
void workerTerminateWrapper(void *argument)
{
  reinterpret_cast<WorkerThread *>(argument)->terminate();
}
//------------------------------------------------------------------------------
WorkerThread::WorkerThread(ThreadSwarm &parent) :
    owner(parent), argumentCount(0), firstArgument(nullptr), finalize(false)
{
  result res;

  res = semInit(&semaphore, 0);
  assert(res == E_OK); //FIXME

  res = threadInit(&thread, THREAD_SIZE, THREAD_PRIORITY, workerThreadWrapper,
      this);
  assert(res == E_OK); //FIXME

  threadOnTerminateCallback(&thread, workerTerminateWrapper, this);
}
//------------------------------------------------------------------------------
WorkerThread::~WorkerThread()
{
  threadTerminate(&thread);

  threadDeinit(&thread);
  semDeinit(&semaphore);
}
//------------------------------------------------------------------------------
void WorkerThread::handler()
{
  result res;

  while (1)
  {
    semWait(&semaphore);

    if (finalize)
      break;

    if (argumentCount && firstArgument != nullptr)
    {
      for (auto entry : owner.owner.commands())
      {
        if (!strcmp(entry->name(), *firstArgument))
        {
          res = entry->run(argumentCount - 1, firstArgument + 1);
          owner.onCommandCompleted(this, res);
        }
      }
    }
  }
}
//------------------------------------------------------------------------------
void WorkerThread::process(unsigned int count, const char * const *arguments)
{
  firstArgument = arguments;
  argumentCount = count;
  semPost(&semaphore);
}
//------------------------------------------------------------------------------
void WorkerThread::start()
{
  result res;

  res = threadStart(&thread);
  assert(res == E_OK); //FIXME
}
//------------------------------------------------------------------------------
void WorkerThread::terminate()
{
  finalize = true;
  semPost(&semaphore);
}
//------------------------------------------------------------------------------
ThreadSwarm::ThreadSwarm(Shell &owner) :
     ShellCommand("swarm", owner)
{
  result res;

  res = mutexInit(&queueLock);
  assert(res == E_OK); //FIXME

  res = semInit(&queueSynchronizer, THREAD_COUNT);
  assert(res == E_OK); //FIXME

  for (unsigned int i = 0; i < THREAD_COUNT; ++i)
  {
    WorkerThread *thread = new WorkerThread(*this);
    thread->start();
    pool.push(thread);
  }
}
//------------------------------------------------------------------------------
ThreadSwarm::~ThreadSwarm()
{
  while (!pool.empty())
  {
    delete pool.front();
    pool.pop();
  }

  semDeinit(&queueSynchronizer);
  mutexDeinit(&queueLock);
}
//------------------------------------------------------------------------------
void ThreadSwarm::onCommandCompleted(WorkerThread *worker, result res)
{
  mutexLock(&queueLock);
  pool.push(worker);
  mutexUnlock(&queueLock);
  semPost(&queueSynchronizer);
}
//------------------------------------------------------------------------------
result ThreadSwarm::run(unsigned int count, const char * const *arguments)
{
  WorkerThread *thread;
  unsigned int first;
  unsigned int index = 0;

  while (index < count)
  {
    first = index;
    while (index < count && strcmp(arguments[index], "!"))
      ++index;

    if (index > first)
    {
      semWait(&queueSynchronizer);

      mutexLock(&queueLock);
      thread = pool.front();
      pool.pop();
      mutexUnlock(&queueLock);

      thread->process(index - first, arguments + first);
    }

    ++index;
  }

  //Wait for commands to complete
  for (index = 0; index < THREAD_COUNT; ++index)
    semWait(&queueSynchronizer);

  //Release acquired resources
  for (index = 0; index < THREAD_COUNT; ++index)
    semPost(&queueSynchronizer);

  return E_OK;
}
