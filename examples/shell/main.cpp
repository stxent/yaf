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
#include "libyaf/fat32.h"
#include "shell/console.h"
#include "shell/mmi.h"
}
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
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

  map<string, result> resultBeautifier;

  Interface *initConsole();
  Interface *initInterface(const char *);
  FsHandle *initHandle(Interface *);
  Shell *initShell(Interface *, FsHandle *);

  string nameByValue(result) const;
  int runShell();
  int runScript(const char *);
};
//------------------------------------------------------------------------------
Application::Application(const char *file, const char *script) :
    scriptFile(script)
{
  consoleInterface = initConsole();
  fsInterface = initInterface(file);
  fsHandle = initHandle(fsInterface);
  appShell = initShell(consoleInterface, fsHandle);

  resultBeautifier.insert(pair<string, result>("E_OK", E_OK));
  resultBeautifier.insert(pair<string, result>("E_ERROR", E_ERROR));
  resultBeautifier.insert(pair<string, result>("E_MEMORY", E_MEMORY));
  resultBeautifier.insert(pair<string, result>("E_ACCESS", E_ACCESS));
  resultBeautifier.insert(pair<string, result>("E_ADDRESS", E_ADDRESS));
  resultBeautifier.insert(pair<string, result>("E_BUSY", E_BUSY));
  resultBeautifier.insert(pair<string, result>("E_DEVICE", E_DEVICE));
  resultBeautifier.insert(pair<string, result>("E_IDLE", E_IDLE));
  resultBeautifier.insert(pair<string, result>("E_INTERFACE", E_INTERFACE));
  resultBeautifier.insert(pair<string, result>("E_INVALID", E_INVALID));
  resultBeautifier.insert(pair<string, result>("E_TIMEOUT", E_TIMEOUT));
  resultBeautifier.insert(pair<string, result>("E_VALUE", E_VALUE));
  resultBeautifier.insert(pair<string, result>("E_ENTRY", E_ENTRY));
  resultBeautifier.insert(pair<string, result>("E_EXIST", E_EXIST));
  resultBeautifier.insert(pair<string, result>("E_EMPTY", E_EMPTY));
  resultBeautifier.insert(pair<string, result>("E_FULL", E_FULL));
}
//------------------------------------------------------------------------------
Application::~Application()
{
  delete appShell;
  deinit(fsHandle);
  deinit(fsInterface);
  deinit(consoleInterface);
}
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
string Application::nameByValue(result res) const
{
  for (auto entry : resultBeautifier)
  {
    if (entry.second == res)
      return entry.first;
  }

  return "";
}
//------------------------------------------------------------------------------
int Application::runShell()
{
  string command;
  int exitFlag = 0;
  bool terminate = false;

  while (!terminate)
  {
    cout << appShell->path() << "> ";
    getline(cin, command);

    const result res = appShell->execute(command.c_str());

    if (res == static_cast<result>(Shell::E_SHELL_EXIT))
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
        cout << "Error: " << nameByValue(res) << endl;
        break;

      default:
        cout << "Fatal error: " << nameByValue(res) << endl;
        exitFlag = EXIT_FAILURE;
        terminate = true;
        break;
    }
  }

  return exitFlag;
}
//------------------------------------------------------------------------------
int Application::runScript(const char *script)
{
  ifstream source(script);

  if (!source.is_open())
    return EXIT_FAILURE;

  string command;
  int exitFlag = 0;

  while (!source.eof())
  {
    getline(source, command);

    const string::size_type delimiter = command.find(':');

    if (delimiter != string::npos && delimiter + 1 < command.length())
    {
      const string resultString = command.substr(0, delimiter);
      auto expectedResult = resultBeautifier.find(resultString);

      if (expectedResult == resultBeautifier.end())
      {
        cout << "Undefined result value" << endl;
        exitFlag = EXIT_FAILURE;
        break;
      }

      const result commandResult =
          appShell->execute(command.c_str() + delimiter + 1);

      if (commandResult != expectedResult->second)
      {
        cout << "Expected result: " << resultString << ", got: "
            << nameByValue(commandResult) << endl;
        exitFlag = EXIT_FAILURE;
        break;
      }
      else
      {
        cout << "Result: " << nameByValue(commandResult) << endl;
      }
    }
  }

  source.close();
  return exitFlag;
}
//------------------------------------------------------------------------------
Interface *Application::initConsole()
{
  Interface * const interface = reinterpret_cast<Interface *>(init(Console, 0));

  if (interface == nullptr)
  {
    cout << "Error creating console interface" << endl;
    exit(EXIT_FAILURE);
  }

  return interface;
}
//------------------------------------------------------------------------------
Interface *Application::initInterface(const char *file)
{
  Interface * const interface = reinterpret_cast<Interface *>(init(Mmi, file));

  if (interface == nullptr)
  {
    cout << "Error opening file" << endl;
    exit(EXIT_FAILURE);
  }

  MbrDescriptor mbr;

  if (mmiReadTable(interface, 0, 0, &mbr) == E_OK)
  {
    if (mmiSetPartition(interface, &mbr) != E_OK)
    {
      cout << "Error during partition setup" << endl;
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    cout << "No partitions found, selected raw partition at 0" << endl;
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
  //Unused when threading is disabled
  fsConf.threads = 2;

#ifdef CONFIG_FAT_TIME
  fsConf.clock = UnixTimeProvider::instance()->rtc();
#endif

  handle = reinterpret_cast<FsHandle *>(init(FatHandle, &fsConf));

  if (!handle)
  {
    cout << "Error creating FAT32 handle" << endl;
    exit(EXIT_FAILURE);
  }

  return handle;
}
//------------------------------------------------------------------------------
Shell *Application::initShell(Interface *console, FsHandle *handle)
{
  Shell *shell = new Shell(console, handle);

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
//------------------------------------------------------------------------------
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
        cout << "Argument error" << endl;
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
    cout << "Usage: shell [OPTION]... FILE" << endl;
    cout << "  -f, --format       create file system on partition" << endl;
    cout << "  -h, --help         print help message" << endl;
    cout << "  -s, --script FILE  use script from FILE" << endl;
    exit(0);
  }

  Application application(image, script);

  return application.run();
}
