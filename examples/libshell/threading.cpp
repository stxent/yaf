/*
 * threading.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <cstring>
#include "libshell/threading.hpp"
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
    owner(parent),
    finalize(false),
    baseContext(nullptr),
    argumentCount(0), firstArgument(nullptr)
{
  Result res;

  res = semInit(&semaphore, 0);

  if (res == E_OK)
  {
    res = threadInit(&thread, THREAD_SIZE, THREAD_PRIORITY,
        workerThreadWrapper, this);
  }

  if (res != E_OK)
  {
    parent.owner.log("swarm: thread initialization error");
    exit(EXIT_FAILURE);
  }

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
  while (1)
  {
    semWait(&semaphore);

    if (finalize)
      break;

    if (!argumentCount || firstArgument == nullptr || baseContext == nullptr)
      continue;

    for (auto entry : owner.owner.commands())
    {
      if (!strcmp(entry->name(), *firstArgument))
      {
        memcpy(&environment, baseContext, sizeof(Shell::ShellContext));

        const Result res = entry->run(argumentCount - 1, firstArgument + 1,
            &environment);

        owner.onCommandCompleted(this, res);
        break;
      }
    }
  }
}
//------------------------------------------------------------------------------
void WorkerThread::process(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  baseContext = context;
  argumentCount = count;
  firstArgument = arguments;
  semPost(&semaphore);
}
//------------------------------------------------------------------------------
void WorkerThread::start()
{
  const Result res = threadStart(&thread);

  if (res != E_OK)
  {
    owner.owner.log("swarm: thread start error");
    exit(EXIT_FAILURE);
  }
}
//------------------------------------------------------------------------------
void WorkerThread::terminate()
{
  finalize = true;
  semPost(&semaphore);
}
//------------------------------------------------------------------------------
ThreadSwarm::ThreadSwarm(Shell &parent) :
    ShellCommand(parent)
{
  Result res;

  res = mutexInit(&queueLock);

  if (res == E_OK)
    res = semInit(&queueSynchronizer, THREAD_COUNT);

  if (res != E_OK)
  {
    owner.log("swarm: initialization error");
    exit(EXIT_FAILURE);
  }

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
void ThreadSwarm::onCommandCompleted(WorkerThread *worker, Result res)
{
  mutexLock(&queueLock);

  pool.push(worker);
  results.push(res);

  mutexUnlock(&queueLock);
  semPost(&queueSynchronizer);
}
//------------------------------------------------------------------------------
Result ThreadSwarm::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
  }

  if (help)
  {
    owner.log("Usage: swarm COMMAND ! COMMAND...");
    owner.log("  --help  print help message");
    return E_OK;
  }

  unsigned int index = 0;
  Result res = E_OK;

  while (index < count)
  {
    const unsigned int first = index;

    while (index < count && strcmp(arguments[index], "!"))
      ++index;

    if (index > first)
    {
      semWait(&queueSynchronizer);
      mutexLock(&queueLock);

      while (!results.empty())
      {
        res = results.front();
        results.pop();
        if (res != E_OK)
          break;
      }

      if (res != E_OK)
      {
        semPost(&queueSynchronizer);
        break;
      }

      WorkerThread * const thread = pool.front();

      pool.pop();
      mutexUnlock(&queueLock);

      thread->process(index - first, arguments + first, context);
    }

    ++index;
  }

  //Wait for commands to complete
  for (index = 0; index < THREAD_COUNT; ++index)
    semWait(&queueSynchronizer);

  //Release acquired resources
  for (index = 0; index < THREAD_COUNT; ++index)
    semPost(&queueSynchronizer);

  //Clear result queue
  while (!results.empty())
  {
    if (res == E_OK)
      res = results.front();
    results.pop();
  }

  return res;
}
