/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <ctime>
#include "commands.hpp"
//------------------------------------------------------------------------------
result ChangeDirectory::run(unsigned int count, char *arguments[]) const
{
  const char *path = 0;
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    path = arguments[i];
  }

  if (help)
  {
    owner->log("Usage: cd DIRECTORY");
    owner->log("  --help  print help message");
    return E_OK;
  }

  if (!path)
    return E_ENTRY;

  Shell::joinPaths(owner->pathBuffer, owner->currentDir, path);

  struct FsNode *node = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0);
  if (!node)
  {
    owner->log("cd: %s: no such directory", owner->pathBuffer);
    return E_ENTRY;
  }

  struct FsEntry *dir = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);
  fsFree(node);

  if (!dir)
  {
    owner->log("cd: %s: access denied", owner->pathBuffer);
    return E_ACCESS;
  }

  fsClose(dir);
  strcpy(owner->currentDir, owner->pathBuffer);

  return E_OK;
}
//------------------------------------------------------------------------------
result CopyEntry::run(unsigned int count, char *arguments[]) const
{
  const char *source = 0, *destination = 0;
  unsigned int chunkSize = 512;
  bool help = false;
  result res;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--chunk-size") && i < count - 1)
    {
      chunkSize = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!source)
    {
      source = arguments[i];
      continue;
    }
    if (!destination)
    {
      destination = arguments[i];
      continue;
    }
  }

  if (help)
  {
    owner->log("Usage: cp SOURCE DESTINATION");
    owner->log("  --help  print help message");
    return E_OK;
  }

  if (!source || !destination)
    return E_ENTRY;

  struct FsNode *sourceNode = 0, *destinationNode = 0, *destinationRoot = 0;
  struct FsMetadata info;

  //Find destination directory or check for target file existence
  Shell::joinPaths(owner->pathBuffer, owner->currentDir, destination);
  destinationRoot = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0);
  if (destinationRoot)
  {
    fsFree(destinationRoot);
    owner->log("cp: %s: entry already exists", owner->pathBuffer);
    return E_ENTRY;
  }

  //Find source entry
  Shell::joinPaths(owner->pathBuffer, owner->currentDir, source);
  sourceNode = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0);
  if (!sourceNode)
  {
    owner->log("cp: %s: no such file or directory", owner->pathBuffer);
    return E_ENTRY;
  }
  if ((res = fsGet(sourceNode, FS_NODE_METADATA, &info)) != E_OK
      || info.type != FS_TYPE_FILE)
  {
    fsFree(sourceNode);
    if (res == E_OK)
    {
      owner->log("cp: %s: entry is not a file", owner->pathBuffer);
      return E_ENTRY;
    }
    else
      return res;
  }

  //Find destination directory where named entry should be placed
  Shell::joinPaths(owner->pathBuffer, owner->currentDir, destination);
  char *namePosition = Shell::extractName(owner->pathBuffer);
  if (!namePosition)
    return E_VALUE; //No entry name found

  strcpy(info.name, namePosition);
  *namePosition = '\0';

  destinationRoot = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0);
  if (!destinationRoot)
  {
    fsFree(sourceNode);
    owner->log("cp: %s: target directory not found", owner->pathBuffer);
    return E_ENTRY;
  }

  destinationNode = (struct FsNode *)fsClone(destinationRoot);
  if (!destinationNode)
  {
    fsFree(destinationRoot);
    fsFree(sourceNode);
    owner->log("cp: node allocation failed");
    return E_MEMORY;
  }

  if (fsMake(destinationRoot, &info, destinationNode) != E_OK)
  {
    fsFree(destinationNode);
    fsFree(destinationRoot);
    fsFree(sourceNode);
    owner->log("cp: node creation failed");
    return E_ERROR;
  }
  fsFree(destinationRoot);

  struct FsEntry *sourceFile = 0, *destinationFile = 0;

  sourceFile = (struct FsEntry *)fsOpen(sourceNode, FS_ACCESS_READ);
  fsFree(sourceNode);
  destinationFile = (struct FsEntry *)fsOpen(destinationNode, FS_ACCESS_WRITE);
  fsFree(destinationNode);

  if (!sourceFile || !destinationFile)
  {
    if (sourceFile)
      fsClose(sourceFile);
    owner->log("cp: opening failed", source);
    return E_ERROR;
  }

  char *buffer = new char[chunkSize];
  uint32_t read = 0, written = 0;
  res = E_OK;

  while (!fsEnd(sourceFile))
  {
    uint32_t inCount, outCount;

    inCount = fsRead(sourceFile, buffer, chunkSize);
    if (!inCount)
    {
      owner->log("cp: read error at %u", read);
      res = E_ERROR;
      break;
    }
    read += inCount;

    outCount = inCount;
    inCount = fsWrite(destinationFile, buffer, outCount);
    if (inCount != outCount)
    {
      owner->log("cp: write error at %u", written);
      res = E_ERROR;
      break;
    }
    written += inCount;
  }

  fsClose(sourceFile);
  fsClose(destinationFile);

  return E_OK;
}
//------------------------------------------------------------------------------
result ExitShell::run(unsigned int count, char *arguments[]) const
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
result ListCommands::run(unsigned int count, char *arguments[]) const
{
  for (auto iter = owner->commands.begin(); iter != owner->commands.end();
      ++iter)
  {
    owner->log("%s", (*iter)->name);
  }
  return E_OK;
}
//------------------------------------------------------------------------------
result ListEntries::run(unsigned int count, char *arguments[]) const
{
  const char *filter = 0, *path = 0;
  int verifyCount = -1;
  bool help = false, verbose = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "-l"))
    {
      verbose = true;
      continue;
    }
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "--filter") && i < count - 1)
    {
      filter = arguments[++i];
      continue;
    }
    if (!strcmp(arguments[i], "--verify-count") && i < count - 1)
    {
      verifyCount = atoi(arguments[++i]);
      continue;
    }
    if (!path)
      path = arguments[i];
  }

  if (help)
  {
    owner->log("Usage: ls [OPTION]... [DIRECTORY]...");
    owner->log("  -l                    show detailed information");
    owner->log("  --help                print help message");
    owner->log("  --filter=NAME         filter entries by NAME substring");
    owner->log("  --verify-count=COUNT  compare entry count with COUNT");
    return E_OK;
  }

  Shell::joinPaths(owner->pathBuffer, owner->currentDir, path);

  struct FsNode *node = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0);
  if (!node)
  {
    owner->log("ls: %s: no such directory", owner->pathBuffer);
    return E_ENTRY;
  }

  struct FsEntry *dir = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);
  if (!dir)
  {
    owner->log("ls: %s: open directory failed", owner->pathBuffer);
    return E_DEVICE;
  }

  struct FsMetadata info;
  int entries = 0;
  result res;

  //Previously allocated node is reused
  while ((res = fsFetch(dir, node)) == E_OK)
  {
    if (!fsEnd(dir))
    {
      if ((res = fsGet(node, FS_NODE_METADATA, &info)) != E_OK)
        break;

      if (filter && !strstr(info.name, filter))
        continue;

      int64_t atime = 0;
      uint64_t size = 0;
      access_t access = 0;

      if ((res = fsGet(node, FS_NODE_SIZE, &size)) != E_OK)
        break;

      if ((res = fsGet(node, FS_NODE_TIME, &atime)) != E_OK)
        break;

      if ((res = fsGet(node, FS_NODE_ACCESS, &access)) != E_OK)
        break;

      if (verbose)
      {
        //Access
        char accessStr[4];
        accessStr[0] = (info.type == FS_TYPE_DIR) ? 'd' : '-';
        accessStr[1] = access & FS_ACCESS_READ ? 'r' : '-';
        accessStr[2] = access & FS_ACCESS_WRITE ? 'w' : '-';
        accessStr[3] = '\0';

        char timeStr[24];
        strftime(timeStr, 24, "%Y-%m-%d %H:%M:%S", gmtime((time_t *)&atime));

        owner->log("%s %10lu %s %s", accessStr, size, timeStr, info.name);
      }
      else
      {
        owner->log(info.name);
      }

      ++entries;
    }
  }

  fsClose(dir);
  fsFree(node);

  if (res != E_ENTRY)
    return res;

  if (verifyCount != -1 && entries != verifyCount)
  {
    owner->log("ls: entry count mismatch: got %u, expected %u", entries,
        verifyCount);
    return E_ERROR;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result MakeDirectory::run(unsigned int count, char *arguments[]) const
{
  const char *target = 0;
  bool help = false;
  result res;

  //TODO Add option -m
  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!target)
      target = arguments[i];
  }

  if (help)
  {
    owner->log("Usage: mkdir [OPTION]... DIRECTORY");
    owner->log("  --help  print help message");
    return E_OK;
  }

  if (!target)
    return E_ENTRY;

  struct FsNode *destinationNode = 0, *destinationRoot = 0;

  //Check for target entry existence
  Shell::joinPaths(owner->pathBuffer, owner->currentDir, target);
  if ((destinationNode = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0)))
  {
    fsFree(destinationNode);
    owner->log("mkdir: %s: entry already exists", owner->pathBuffer);
    return E_ENTRY;
  }

  struct FsMetadata info;
  info.type = FS_TYPE_DIR;

  //Find destination directory where named entry should be placed
  char *namePosition = Shell::extractName(owner->pathBuffer);
  if (!namePosition)
    return E_VALUE; //No entry name found

  strcpy(info.name, namePosition);
  *namePosition = '\0';

  if (!(destinationRoot = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0)))
  {
    owner->log("mkdir: %s: target directory not found", owner->pathBuffer);
    return E_ENTRY;
  }
  else
  {
    enum fsNodeType type;
    if ((res = fsGet(destinationRoot, FS_NODE_TYPE, &type)) != E_OK
        || type != FS_TYPE_DIR)
    {
      fsFree(destinationRoot);
      owner->log("mkdir: %s: wrong destination type", owner->pathBuffer);
      return res == E_OK ? E_ENTRY : res;
    }
  }

  //Clone node to allocate node from the same handle
  if (!(destinationNode = (struct FsNode *)fsClone(destinationRoot)))
  {
    fsFree(destinationRoot);
    owner->log("mkdir: node allocation failed");
    return E_MEMORY;
  }

  res = fsMake(destinationRoot, &info, destinationNode);
  fsFree(destinationRoot);
  fsFree(destinationNode);

  if (res != E_OK)
    owner->log("mkdir: node creation failed");

  return res;
}
//------------------------------------------------------------------------------
result MeasureTime::run(unsigned int count, char *arguments[]) const
{
  uint64_t start, delta;
  result res = E_VALUE;

  for (auto iter = owner->commands.begin(); iter != owner->commands.end();
      ++iter)
  {
    if (!strcmp((*iter)->name, arguments[0]))
    {
      start = Shell::timestamp();
      res = (*iter)->run(count - 1, arguments + 1);
      delta = Shell::timestamp() - start;

      owner->log("Time passed: %lu ns", delta);
    }
  }
  return res;
}
//------------------------------------------------------------------------------
result RemoveDirectory::run(unsigned int count, char *arguments[]) const
{
  const char *target = 0;
  bool help = false;
  result res;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!target)
      target = arguments[i];
  }

  if (help)
  {
    owner->log("Usage: rmdir DIRECTORY");
    owner->log("  --help  print help message");
    return E_OK;
  }

  if (!target)
    return E_ENTRY;

  struct FsNode *destinationNode = 0;

  Shell::joinPaths(owner->pathBuffer, owner->currentDir, target);
  if (!(destinationNode = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0)))
  {
    owner->log("rmdir: %s: no such entry", owner->pathBuffer);
    return E_ENTRY;
  }

  enum fsNodeType type;
  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || type != FS_TYPE_DIR)
  {
    owner->log("rmdir: %s: wrong entry type", owner->pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner->log("rmdir: %s: directory not empty", owner->pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner->log("rmdir: %s: unlinking failed", owner->pathBuffer);
    goto free_node;
  }

free_node:
  fsFree(destinationNode);
  return res;
}
//------------------------------------------------------------------------------
result RemoveEntry::run(unsigned int count, char *arguments[]) const
{
  const char *target = 0;
  bool help = false, recursive = false;
  result res;

  //TODO Implement recursive remove
  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "-r"))
    {
      recursive = true;
      continue;
    }
    if (!target)
      target = arguments[i];
  }

  if (help)
  {
    owner->log("Usage: rm [OPTION]... ENTRY");
    owner->log("  --help  print help message");
    owner->log("  -t      remove directories and their content");
    return E_OK;
  }

  if (!target)
    return E_ENTRY;

  struct FsNode *destinationNode = 0;

  Shell::joinPaths(owner->pathBuffer, owner->currentDir, target);
  if (!(destinationNode = (struct FsNode *)fsFollow(owner->rootHandle,
      owner->pathBuffer, 0)))
  {
    owner->log("rm: %s: no such entry", owner->pathBuffer);
    return E_ENTRY;
  }

  enum fsNodeType type;
  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || (type == FS_TYPE_DIR && !recursive))
  {
    owner->log("rm: %s: wrong entry type", owner->pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner->log("rm: %s: payload removing failed", owner->pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner->log("rm: %s: unlinking failed", owner->pathBuffer);
    goto free_node;
  }

free_node:
  fsFree(destinationNode);
  return res;
}
