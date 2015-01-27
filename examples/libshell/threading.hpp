/*
 * libshell/threading.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBSHELL_THREADING_HPP_
#define LIBSHELL_THREADING_HPP_
//------------------------------------------------------------------------------
#include <queue>
#include "libshell/shell.hpp"

extern "C"
{
#include <os/mutex.h>
#include <os/semaphore.h>
#include <os/thread.h>
}
//------------------------------------------------------------------------------
class WorkerThread;
//------------------------------------------------------------------------------
class ThreadSwarm : public Shell::ShellCommand
{
  friend class WorkerThread;

public:
  ThreadSwarm(Shell &owner);
  ~ThreadSwarm();

  virtual const char *name() const
  {
    return "swarm";
  }

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  enum
  {
    THREAD_COUNT = 2
  };

  void onCommandCompleted(WorkerThread *, result);

  Mutex queueLock;
  Semaphore queueSynchronizer;
  std::queue<WorkerThread *> pool;
};
//------------------------------------------------------------------------------
extern "C" void workerThreadWrapper(void *);
extern "C" void workerTerminateWrapper(void *);
//------------------------------------------------------------------------------
class WorkerThread
{
  friend void ::workerThreadWrapper(void *);
  friend void ::workerTerminateWrapper(void *);

public:
  WorkerThread(ThreadSwarm &parent);
  ~WorkerThread();

  void process(unsigned int, const char * const *, Shell::ShellContext *);
  void start();

private:
  enum
  {
    THREAD_PRIORITY = 0,
    THREAD_SIZE = 1024
  };

  void handler();
  void terminate();

  ThreadSwarm &owner;

  Semaphore semaphore;
  Thread thread;
  bool finalize;

  Shell::ShellContext environment;
  Shell::ShellContext *baseContext;
  unsigned int argumentCount;
  const char * const *firstArgument;
};
//------------------------------------------------------------------------------
#endif //LIBSHELL_THREADING_HPP_
