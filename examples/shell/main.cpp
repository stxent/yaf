#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sstream>
//---------------------------------------------------------------------------
extern "C"
{
#include "fat.h"
#include "io.h"
#include "rtc.h"
}
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
extern long readExcess, readCount, writeCount;
extern char devicePath[255];
//---------------------------------------------------------------------------
void parsePath(char *dest, const char *folder, const char *path)
{
  size_t length;
  if (path[0] != '/')
  {
    strcpy(dest, folder);
    length = strlen(folder);
    if (length > 1)
    {
      dest[length] = '/';
      dest[length + 1] = '\0';
    }
    strcpy(dest + strlen(dest), path);
  }
  else
    strcpy(dest, path);
  length = strlen(dest);
  if ((length > 1) && (dest[length] == '/'))
    dest[length] = '\0';
}
//---------------------------------------------------------------------------
int util_cd(struct FsHandle *handler, char *location)
{
  struct FsDir dir;
  if (fsOpenDir(handler, &dir, location) == FS_OK)
    return FS_OK;
#ifdef DEBUG
  cout << "cd: " << location << ": No such directory" << endl;
#endif
  return FS_ERROR;
}
//---------------------------------------------------------------------------
int util_ls(char **args, int count, struct FsHandle *handler, const char *location)
{
  struct FsDir dir;
  bool details = false;
  if ((count >= 2) && !strcmp(args[1], "-l"))
    details = true;
  if (fsOpenDir(handler, &dir, location) == FS_OK)
  {
#ifdef DEBUG
    struct FsFile file;
    struct FsDir tempdir;
    char strbuf[40];
#endif
    char fname[13];
    char path[256];
    int listPos = 0;
    if (details)
      cout << "PARENT.INDEX CLUSTER ACCESS     SIZE     DATE                NAME" << endl;
    while (fsReadDir(&dir, fname) == FS_OK)
    {
      listPos++;
      if (details)
      {
        parsePath(path, location, fname);
        FsStat stat;
        if (fsStat(handler, path, &stat) == FS_OK)
        {
#ifdef DEBUG
          sprintf(strbuf, "%d.%d", stat.pcluster, stat.pindex);
          cout.width(12);
          cout << strbuf << ' ';
          cout.width(7);
          cout << stat.cluster << ' ';
#endif
          //Access
          if (stat.type == FS_TYPE_DIR)
            strbuf[0] = 'd';
          else
            strbuf[0] = '-';
          strcpy(strbuf + 1, "rwxrwxrwx");
          for (int ai = 8; ai >= 0; ai--)
            if (!(stat.access & (1 << ai)))
              strbuf[9 - ai] = '-';

          cout << strbuf << ' ';

          cout.width(8);
          cout << left << stat.size << ' ';

          strftime(strbuf, sizeof(strbuf), "%Y-%m-%d %H:%M:%S", gmtime((time_t *)&stat.atime));
          cout << strbuf << ' ';
          cout.width(11);
          cout << fname << right << endl;
        }
      }
      else
      {
        cout.width(20);
        cout << left << fname;
        if (!(listPos % 5))
          cout << right << endl;
      }
    }
    if (!details && (listPos % 5))
      cout << endl;
    return FS_OK;
  }
#ifdef DEBUG
  cout << "ls: " << location << ": No such directory" << endl;
#endif
  return FS_ERROR;
}
//---------------------------------------------------------------------------
int util_stat(struct FsHandle *handler)
{
#ifdef DEBUG
  cout << "Read excess:     " << readExcess << endl;
#endif
  cout << "Sectors read:    " << readCount << endl;
  cout << "Sectors written: " << writeCount << endl;
  return FS_OK;
}
//---------------------------------------------------------------------------
int util_info(struct FsHandle *handler)
{
#if defined (FS_WRITE_ENABLED) && defined (DEBUG)
  uint32_t sz;
  sz = countFree(handler);
#endif
  cout << "Device:             " << devicePath << endl;
  cout << "Sectors in cluster: " << (int)(1 << handler->clusterSize) << endl;
  cout << "FAT sector:         " << handler->fatSector << endl;
  cout << "Data sector:        " << handler->dataSector << endl;
  cout << "Root cluster:       " << handler->rootCluster << endl;
#ifdef FS_WRITE_ENABLED
  cout << "FAT records count:  " << (int)handler->fatCount << endl;
  cout << "Sectors in FAT:     " << handler->sectorsPerFAT << endl;
  cout << "Info sector:        " << handler->infoSector << endl;
  cout << "Data clusters:      " << handler->clusterCount << endl;
  cout << "Last allocated:     " << handler->lastAllocated << endl;
#ifdef DEBUG
  cout << "Free clusters:      " << sz << endl;
#endif
#endif
  cout << "Size of fsHandle:   " << sizeof(FsHandle) << endl;
  cout << "Size of fsFile:     " << sizeof(FsFile) << endl;
  cout << "Size of fsDir:      " << sizeof(FsDir) << endl;
//   cout << "Size of fsEntry:    " << sizeof(FsEntry) << endl;
  return FS_OK;
}
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;
  char internalBuf[FS_BUFFER];
  FsDevice dev;
  FsHandle handler;

  fsSetIO(&handler, sRead, sWrite);
  sOpen(&dev, (uint8_t *)internalBuf, argv[1]);
  if (!sReadTable(&dev, 0, 0))
  {
    //0x0B: 32-bit FAT
    //0x0C: 32-bit FAT, using INT 13 Extensions.
    //0x1B: Hidden 32-bit FAT
    //0x1C: Hidden 32-bit FAT, using INT 13 Extensions
    if (dev.type != 0x0B)
      cout << "Wrong partition descriptor, expected: 0x0B, got: " << hex << (int)dev.type << dec << endl;
    else
      cout << "Selected partition: offset = " << dev.offset << ", size = " << dev.size << ", type = " << hex << (int)dev.type << dec << endl;
  }
  else
    cout << "No partitions found, selected raw partition at 0" << endl;

  if (fsLoad(&handler, &dev) != FS_OK)
  {
    cout << "ERROR!" << endl;
    return 0;
  }

  const int argCount = 8;
  const int argLength = 80;
  char **args = new char *[argCount];
  for (int i = 0; i < argCount; i++)
    args[i] = new char[argLength];
  char location[256];
  location[0] = '/';
  memset(location + 1, '\0', sizeof(location) - 1);
  stringstream parseBuffer;
  bool terminate = false;
  while (!terminate)
  {
    int count;
    char buf[256], cbuf[256];
    cout << location << "> ";
    cin.getline(cbuf, sizeof(cbuf));
    parseBuffer.clear();
    parseBuffer << cbuf;
    if (!parseBuffer.eof())
    {
      for (count = 0; !parseBuffer.eof() && (count < argCount); count++)
        parseBuffer >> args[count];
      if (!strcmp(args[0], "exit"))
        terminate = true;
      if (!strcmp(args[0], "stat"))
        util_stat(&handler);
      if (!strcmp(args[0], "info"))
        util_info(&handler);
      if (!strcmp(args[0], "cd") && (count >= 2))
      {
        parsePath(buf, location, args[1]);
        if (util_cd(&handler, buf) == FS_OK)
          strcpy(location, buf);
      }
      if (!strcmp(args[0], "ls"))
      {
        util_ls(args, count, &handler, location);
      }
    }
  }

  cout << "Unloading" << endl;
  fsUnload(&handler);
  return 0;

  for (int i = 0; i < argCount; i++)
    delete[] args[i];
  delete[] args;
}
