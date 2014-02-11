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
//#include <openssl/md5.h>
#include "commands.hpp"
#include "shell.hpp"
//------------------------------------------------------------------------------
extern "C"
{
#include "fat32.h"
#include "mmi.h"
}
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;

  struct Interface *mmaped;
  struct FsHandle *handle;

  mmaped = (struct Interface *)init(Mmi, argv[1]);
  if (!mmaped)
  {
    printf("Error opening file\n");
    return 0;
  }

  struct MbrDescriptor mbrRecord;
  if (mmiReadTable(mmaped, 0, 0, &mbrRecord) == E_OK)
  {
    if (mmiSetPartition(mmaped, &mbrRecord) != E_OK)
      printf("Error setup partition\n");
  }
  else
    printf("No partitions found, selected raw partition at 0\n");

  struct Fat32Config fsConf;
  fsConf.interface = mmaped;
#ifdef FAT_POOLS
  fsConf.nodes = fsConf.directories = fsConf.files = 0;
#endif

  handle = (struct FsHandle *)init(FatHandle, &fsConf);
  if (!handle)
  {
    printf("Error creating FAT32 handle\n");
    return 0;
  }

  Shell shell(0, handle);
  CommandLinker<ChangeDirectory>::attach(&shell);
  CommandLinker<CopyEntry>::attach(&shell);
  CommandLinker<ExitShell>::attach(&shell);
  CommandLinker<ListCommands>::attach(&shell);
  CommandLinker<ListEntries>::attach(&shell);
  CommandLinker<MakeDirectory>::attach(&shell);
  CommandLinker<MeasureTime>::attach(&shell);
  CommandLinker<RemoveDirectory>::attach(&shell);
  CommandLinker<RemoveEntry>::attach(&shell);

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
        break;
      case E_VALUE:
        cout << "Syntax error" << endl;
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
