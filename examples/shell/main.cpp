/*
 * main.cpp
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cassert>
#include <iostream>
#include "libshell/commands.hpp"
#include "libshell/shell.hpp"
#include "libshell/threading.hpp"
#include "shell/crypto_wrapper.hpp"
#include "shell/time_wrapper.hpp"

extern "C"
{
#include <libyaf/fat32.h>
#include <os/mutex.h>
#include "shell/mmi.h"
}
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
class ConsoleShell : public Shell
{
public:
  ConsoleShell(struct Interface *console, struct FsHandle *root) :
      Shell(console, root)
  {
    result res;

    res = mutexInit(&logMutex);
    assert(res == E_OK);

    //Log function should be called after mutex initialization
    log("Shell opened, size %u", static_cast<unsigned int>(sizeof(Shell)));
  }

  virtual ~ConsoleShell()
  {
    mutexDeinit(&logMutex);
  }

  virtual void log(const char *format, ...)
  {
    va_list arguments;

    mutexLock(&logMutex);
    va_start(arguments, format);
    vsnprintf(logBuffer, LOG_LENGTH - 1, format, arguments);
    va_end(arguments);

    strcat(logBuffer, "\n");
    printf(logBuffer);
    mutexUnlock(&logMutex);
  }

private:
  enum
  {
    LOG_LENGTH = 80
  };

  char logBuffer[LOG_LENGTH * 2 + 1];
  Mutex logMutex;
};
//------------------------------------------------------------------------------
class Application
{
public:
  Application(const char *);
  ~Application();

  int run();

private:
  enum
  {
    INIT_FAILED = -1,
    RUNTIME_ERROR = -2
  };

  Interface *fsInterface;
  FsHandle *fsHandle;
  Shell *appShell;

  Interface *initInterface(const char *);
  FsHandle *initHandle(Interface *);
  Shell *initShell(FsHandle *);
};
//------------------------------------------------------------------------------
Application::Application(const char *file)
{
  fsInterface = initInterface(file);
  fsHandle = initHandle(fsInterface);
  appShell = initShell(fsHandle);
}
//------------------------------------------------------------------------------
Application::~Application()
{
  delete appShell;
  deinit(fsHandle);
  deinit(fsInterface);
}
//------------------------------------------------------------------------------
int Application::run()
{
  int exitFlag = 0;
  bool terminate = false;

  while (!terminate)
  {
    cout << appShell->path() << "> ";

    string command;
    getline(cin, command);

    const result res = appShell->execute(command.c_str());

    switch (res)
    {
      case E_OK:
        break;

      case E_ACCESS:
      case E_BUSY:
      case E_ENTRY:
      case E_VALUE:
        break;

      default:
        exitFlag = RUNTIME_ERROR;
        terminate = true;
        break;
    }
  }

  return exitFlag;
}
//------------------------------------------------------------------------------
Interface *Application::initInterface(const char *file)
{
  Interface *interface;

  interface = reinterpret_cast<Interface *>(init(Mmi, file));
  if (interface == nullptr)
  {
    printf("Error opening file\n");
    exit(INIT_FAILED);
  }

  MbrDescriptor mbr;

  if (mmiReadTable(interface, 0, 0, &mbr) == E_OK)
  {
    if (mmiSetPartition(interface, &mbr) != E_OK)
    {
      printf("Error during partition setup\n");
      exit(INIT_FAILED);
    }
  }
  else
  {
    printf("No partitions found, selected raw partition at 0\n");
  }

  return interface;
}
//------------------------------------------------------------------------------
FsHandle *Application::initHandle(Interface *interface)
{
  FsHandle *handle;
  Fat32Config fsConf;

  fsConf.interface = interface;
  //Unused when pools are disabled
  fsConf.nodes = 4;
  fsConf.directories = 2;
  fsConf.files = 2;
  //Unused when threading is disabled
  fsConf.threads = 2;

#ifdef CONFIG_FAT_TIME
  fsConf.timer = UnixTimeProvider::instance()->rtc();
#endif

  handle = reinterpret_cast<FsHandle *>(init(FatHandle, &fsConf));

  if (!handle)
  {
    printf("Error creating FAT32 handle\n");
    exit(INIT_FAILED);
  }

  return handle;
}
//------------------------------------------------------------------------------
Shell *Application::initShell(FsHandle *handle)
{
  Shell *shell = new ConsoleShell(0, handle);

  shell->append(CommandBuilder<ChangeDirectory>());
  shell->append(CommandBuilder<CopyEntry>());
  shell->append(CommandBuilder<DirectData>());
  shell->append(CommandBuilder<ExitShell>());
  shell->append(CommandBuilder<ListCommands>());
  shell->append(CommandBuilder<ListEntries>());
  shell->append(CommandBuilder<MakeDirectory>());
  shell->append(CommandBuilder<RemoveDirectory>());
  shell->append(CommandBuilder<RemoveEntry>());
  shell->append(CommandBuilder<Synchronize>());

  shell->append(CommandBuilder<ComputationCommand<Md5Hash>>());

#ifdef CONFIG_FAT_THREADS
  shell->append(CommandBuilder<ThreadSwarm>());
#endif
#ifdef CONFIG_FAT_TIME
  shell->append(CommandBuilder<CurrentDate<UnixTimeProvider>>());
  shell->append(CommandBuilder<MeasureTime<UnixTimeProvider>>());
#endif

  return shell;
}
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;

  Application application(argv[1]);

  return application.run();
}
