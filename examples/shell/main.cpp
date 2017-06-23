/*
 * main.cpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "libshell/commands.hpp"
#include "libshell/file_tools.hpp"
#include "libshell/shell.hpp"
#include "libshell/threading.hpp"
#include "shell/crypto_wrapper.hpp"
#include "shell/time_wrapper.hpp"

extern "C"
{
#include <yaf/fat32.h>
#include <vfs/vfs.h>
#include <vfs/vfs_handle_node.h>
#include "shell/console.h"
#include "shell/mmi.h"
}

class Application
{
public:
  Application(const char *, const char *);
  ~Application();

  int run();

private:
  enum
  {
    BUFFER_SIZE = 512
  };

  const char *scriptFile;
  Interface *consoleInterface;
  Interface *fsInterface;
  FsHandle *fsHandle;
  Shell *appShell;

  std::map<std::string, Result> resultBeautifier;

  Interface *initConsole();
  Interface *initInterface(const char *);
  FsHandle *initHandle(Interface *);
  Shell *initShell(Interface *, FsHandle *);

  std::string nameByValue(Result) const;
  int runShell();
  int runScript(const char *);
};

Application::Application(const char *file, const char *script) :
    scriptFile(script)
{
  consoleInterface = initConsole();
  fsInterface = initInterface(file);
  fsHandle = initHandle(fsInterface);
  appShell = initShell(consoleInterface, fsHandle);

  resultBeautifier.insert(std::pair<std::string, Result>("E_OK", E_OK));
  resultBeautifier.insert(std::pair<std::string, Result>("E_ERROR", E_ERROR));
  resultBeautifier.insert(std::pair<std::string, Result>("E_MEMORY", E_MEMORY));
  resultBeautifier.insert(std::pair<std::string, Result>("E_ACCESS", E_ACCESS));
  resultBeautifier.insert(std::pair<std::string, Result>("E_ADDRESS", E_ADDRESS));
  resultBeautifier.insert(std::pair<std::string, Result>("E_BUSY", E_BUSY));
  resultBeautifier.insert(std::pair<std::string, Result>("E_DEVICE", E_DEVICE));
  resultBeautifier.insert(std::pair<std::string, Result>("E_IDLE", E_IDLE));
  resultBeautifier.insert(std::pair<std::string, Result>("E_INTERFACE", E_INTERFACE));
  resultBeautifier.insert(std::pair<std::string, Result>("E_INVALID", E_INVALID));
  resultBeautifier.insert(std::pair<std::string, Result>("E_TIMEOUT", E_TIMEOUT));
  resultBeautifier.insert(std::pair<std::string, Result>("E_VALUE", E_VALUE));
  resultBeautifier.insert(std::pair<std::string, Result>("E_ENTRY", E_ENTRY));
  resultBeautifier.insert(std::pair<std::string, Result>("E_EXIST", E_EXIST));
  resultBeautifier.insert(std::pair<std::string, Result>("E_EMPTY", E_EMPTY));
  resultBeautifier.insert(std::pair<std::string, Result>("E_FULL", E_FULL));
}

Application::~Application()
{
  delete appShell;
  deinit(fsHandle);
  deinit(fsInterface);
  deinit(consoleInterface);
}

int Application::run()
{
  if (scriptFile != nullptr)
  {
    return runScript(scriptFile);
  }
  else
  {
    return runShell();
  }
}

std::string Application::nameByValue(Result res) const
{
  for (auto entry : resultBeautifier)
  {
    if (entry.second == res)
      return entry.first;
  }

  return "";
}

int Application::runShell()
{
  std::string command;
  int exitFlag = 0;
  bool terminate = false;

  while (!terminate)
  {
    std::cout << appShell->path() << "> ";
    getline(std::cin, command);

    const Result res = appShell->execute(command.c_str());

    if (res == static_cast<Result>(Shell::E_SHELL_EXIT))
    {
      exitFlag = EXIT_SUCCESS;
      terminate = true;
      break;
    }

    switch (res)
    {
      case E_OK:
        break;

      case E_ACCESS:
      case E_BUSY:
      case E_ENTRY:
      case E_EXIST:
      case E_INVALID:
      case E_VALUE:
        std::cout << "Error: " << nameByValue(res) << std::endl;
        break;

      default:
        std::cout << "Fatal error: " << nameByValue(res) << std::endl;
        exitFlag = EXIT_FAILURE;
        terminate = true;
        break;
    }
  }

  return exitFlag;
}

int Application::runScript(const char *script)
{
  std::ifstream source(script);

  if (!source.is_open())
    return EXIT_FAILURE;

  std::string command;
  int exitFlag = 0;

  while (!source.eof())
  {
    getline(source, command);

    const std::string::size_type delimiter = command.find(':');

    if (delimiter != std::string::npos && delimiter + 1 < command.length())
    {
      const std::string resultString = command.substr(0, delimiter);
      auto expectedResult = resultBeautifier.find(resultString);

      if (expectedResult == resultBeautifier.end())
      {
        std::cout << "Undefined result value" << std::endl;
        exitFlag = EXIT_FAILURE;
        break;
      }

      const Result commandResult = appShell->execute(command.c_str() + delimiter + 1);

      if (commandResult != expectedResult->second)
      {
        std::cout << "Expected result: " << resultString << ", got: " << nameByValue(commandResult) << std::endl;
        exitFlag = EXIT_FAILURE;
        break;
      }
      else
      {
        std::cout << "Result: " << nameByValue(commandResult) << std::endl;
      }
    }
  }

  source.close();
  return exitFlag;
}

Interface *Application::initConsole()
{
  Interface * const interface = static_cast<Interface *>(init(Console, nullptr));

  if (interface == nullptr)
  {
    std::cout << "Error creating console interface" << std::endl;
    exit(EXIT_FAILURE);
  }

  return interface;
}

Interface *Application::initInterface(const char *file)
{
  Interface * const interface = static_cast<Interface *>(init(Mmi, file));

  if (interface == nullptr)
  {
    std::cout << "Error opening file" << std::endl;
    exit(EXIT_FAILURE);
  }

  MbrDescriptor mbr;

  if (mmiReadTable(interface, 0, 0, &mbr) == E_OK)
  {
    if (mmiSetPartition(interface, &mbr) != E_OK)
    {
      std::cout << "Error during partition setup" << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    std::cout << "No partitions found, selected raw partition at 0" << std::endl;
  }

  return interface;
}

FsHandle *Application::initHandle(Interface *interface)
{
  FsHandle *handle;
  Fat32Config fsConf;

  fsConf.interface = interface;
  //Unused when pools are disabled
  fsConf.nodes = 4;
  //Unused when threading is disabled
  fsConf.threads = 2;

#ifdef CONFIG_FAT_TIME
  fsConf.clock = UnixTimeProvider::instance()->rtc();
#endif

  FsHandle *fhandle = static_cast<FsHandle *>(init(FatHandle, &fsConf));
  handle = static_cast<FsHandle *>(init(VfsHandle, nullptr));

  const VfsHandleNodeConfig x3 = {fhandle, "media"};
  struct VfsNode *y3 = static_cast<struct VfsNode *>(init(VfsHandleNode, &x3));

  const FsFieldDescriptor d3[] = {
      {
          &y3,
          sizeof(y3),
          static_cast<FsFieldType>(VFS_NODE_OBJECT)
      }
  };

  FsNode *hhhjh = static_cast<FsNode *>(fsHandleRoot(handle));
  fsNodeCreate(hhhjh, d3, 1);
  fsNodeFree(hhhjh);

  if (!handle)
  {
    std::cout << "Error creating FAT32 handle" << std::endl;
    exit(EXIT_FAILURE);
  }

  return handle;
}

Shell *Application::initShell(Interface *console, FsHandle *handle)
{
  Shell * const shell = new Shell(console, handle);

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

  shell->append(CommandBuilder<CatEntry>());
  shell->append(CommandBuilder<ComputationCommand<ChecksumCrc32>>());
  shell->append(CommandBuilder<EchoData>());
  shell->append(CommandBuilder<TouchEntry>());

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

int main(int argc, char *argv[])
{
  const char *image = nullptr;
  const char *script = nullptr;
  bool help = false;

  for (int i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
    {
      help = true;
      continue;
    }
    if (!strcmp(argv[i], "--script") || !strcmp(argv[i], "-s"))
    {
      if (i + 1 >= argc)
      {
        std::cout << "Argument error" << std::endl;
        break;
      }

      script = argv[i + 1];
      ++i;
      continue;
    }
    if (image == nullptr)
    {
      image = argv[i];
      continue;
    }
  }

  if (image == nullptr)
    help = true;

  if (help)
  {
    std::cout << "Usage: shell [OPTION]... FILE" << std::endl;
    std::cout << "  -f, --format       create file system on partition" << std::endl;
    std::cout << "  -h, --help         print help message" << std::endl;
    std::cout << "  -s, --script FILE  use script from FILE" << std::endl;
    exit(0);
  }

  Application application(image, script);

  return application.run();
}
