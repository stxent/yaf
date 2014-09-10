/*
 * main.cpp
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>
#include "commands.hpp"
#include "crypto.hpp"
#include "shell.hpp"
#include "threading.hpp"
//------------------------------------------------------------------------------
extern "C"
{
#include <libyaf/fat32.h>
#include "mmi.h"
}
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;

  Interface *mmaped;
  FsHandle *handle;

  mmaped = reinterpret_cast<Interface *>(init(Mmi, argv[1]));
  if (!mmaped)
  {
    printf("Error opening file\n");
    return 0;
  }

  MbrDescriptor mbrRecord;
  if (mmiReadTable(mmaped, 0, 0, &mbrRecord) == E_OK)
  {
    if (mmiSetPartition(mmaped, &mbrRecord) != E_OK)
      printf("Error setup partition\n");
  }
  else
    printf("No partitions found, selected raw partition at 0\n");

  Fat32Config fsConf;
  fsConf.interface = mmaped;
#ifdef CONFIG_FAT_POOLS
  fsConf.nodes = 4;
  fsConf.directories = 2;
  fsConf.files = 2;
#endif
#ifdef CONFIG_FAT_THREADS
  fsConf.threads = 2;
#endif

  handle = reinterpret_cast<FsHandle *>(init(FatHandle, &fsConf));
  if (!handle)
  {
    printf("Error creating FAT32 handle\n");
    return 0;
  }

  Shell shell(0, handle);
  shell.append(CommandBuilder<ChangeDirectory>());
  shell.append(CommandBuilder<CopyEntry>());
  shell.append(CommandBuilder<DirectData>());
  shell.append(CommandBuilder<ExitShell>());
  shell.append(CommandBuilder<ListCommands>());
  shell.append(CommandBuilder<ListEntries>());
  shell.append(CommandBuilder<MakeDirectory>());
  shell.append(CommandBuilder<MeasureTime>());
  shell.append(CommandBuilder<RemoveDirectory>());
  shell.append(CommandBuilder<RemoveEntry>());

  shell.append(CommandBuilder<ThreadSwarm>());

#ifdef CONFIG_CRYPTO
  shell.append(CommandBuilder<ComputeHash>());
#endif

  bool terminate = false;

  while (!terminate)
  {
    cout << shell.path() << "> ";

    string command;
    getline(cin, command);

    enum result res = shell.execute(command.c_str());

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
        terminate = true;
        break;
    }
  }

  printf("Unloading\n");
  deinit(handle);
  deinit(mmaped);

  return 0;
}
