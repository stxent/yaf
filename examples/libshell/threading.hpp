/*
 * libshell/threading.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_THREADING_HPP_
#define YAF_LIBSHELL_THREADING_HPP_

#include <queue>
#include "libshell/shell.hpp"

extern "C"
{
#include <osw/mutex.h>
#include <osw/semaphore.h>
#include <osw/thread.h>
}

class WorkerThread;

class ThreadSwarm: public Shell::ShellCommand
{
  friend class WorkerThread;

public:
  ThreadSwarm(Shell &parent);
  virtual ~ThreadSwarm();

  virtual const char *name() const override
  {
    return "swarm";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  enum
  {
    THREAD_COUNT = 2
  };

  void onCommandCompleted(WorkerThread *, Result);

  Mutex queueLock;
  Semaphore queueSynchronizer;
  std::queue<WorkerThread *> pool;
  std::queue<Result> results;
};

extern "C" void workerThreadWrapper(void *);
extern "C" void workerTerminateWrapper(void *);

class WorkerThread
{
  friend void ::workerThreadWrapper(void *);
  friend void ::workerTerminateWrapper(void *);

public:
  WorkerThread(ThreadSwarm &parent);
  virtual ~WorkerThread();

  void process(const char * const *, size_t, Shell::ShellContext *);
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
  size_t argumentCount;
  const char * const *firstArgument;
};

#endif //YAF_LIBSHELL_THREADING_HPP_
