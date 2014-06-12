/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <ctime>
#include "commands.hpp"
//------------------------------------------------------------------------------
result ChangeDirectory::processArguments(unsigned int count,
    const char * const *arguments, const char **path) const
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    *path = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: cd DIRECTORY");
    owner.log("  --help  print help message");
    return E_BUSY;
  }

  if (*path == nullptr)
    return E_ENTRY;

  return E_OK;
}
//------------------------------------------------------------------------------
result ChangeDirectory::run(unsigned int count,
    const char * const *arguments) const
{
  const char *path = nullptr;
  result res;

  if ((res = processArguments(count, arguments, &path)) != E_OK)
    return res;

  FsNode *node;
  FsEntry *dir;

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  node = (FsNode *)fsFollow(owner.handle(), context->pathBuffer, nullptr);
  if (node == nullptr)
  {
    owner.log("cd: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  dir = (FsEntry *)fsOpen(node, FS_ACCESS_READ);
  fsFree(node);

  if (dir == nullptr)
  {
    owner.log("cd: %s: access denied", context->pathBuffer);
    return E_ACCESS;
  }

  fsClose(dir);
  strcpy(context->currentDir, context->pathBuffer);

  return E_OK;
}
//------------------------------------------------------------------------------
result CopyEntry::copyContent(FsNode *sourceNode, FsNode *destinationNode,
    unsigned int chunkSize) const
{
  FsEntry *sourceFile = nullptr, *destinationFile = nullptr;
  char buffer[bufferLength];

  //Open file entries
  sourceFile = (FsEntry *)fsOpen(sourceNode, FS_ACCESS_READ);
  if (sourceFile == nullptr)
  {
    owner.log("cp: source file opening error");
    return E_ENTRY;
  }

  destinationFile = (FsEntry *)fsOpen(destinationNode, FS_ACCESS_WRITE);
  if (destinationFile == nullptr)
  {
    fsClose(sourceFile);
    owner.log("cp: source file opening error");
    return E_ENTRY;
  }

  uint32_t read = 0, written = 0;
  result res = E_OK;

  //Copy file content
  while (!fsEnd(sourceFile))
  {
    uint32_t inCount, outCount;

    inCount = fsRead(sourceFile, buffer, chunkSize);
    if (!inCount)
    {
      owner.log("cp: read error at %u", read);
      res = E_ERROR;
      break;
    }
    read += inCount;

    outCount = inCount;
    inCount = fsWrite(destinationFile, buffer, outCount);
    if (inCount != outCount)
    {
      owner.log("cp: write error at %u", written);
      res = E_ERROR;
      break;
    }
    written += inCount;
  }

  fsClose(sourceFile);
  fsClose(destinationFile);

  return res;
}
//------------------------------------------------------------------------------
result CopyEntry::processArguments(unsigned int count,
    const char * const *arguments, const char **source,
    const char **destination, unsigned int *chunkSize) const
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--chunk-size") && i < count - 1)
    {
      *chunkSize = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (*source == nullptr)
    {
      *source = arguments[i];
      continue;
    }
    if (*destination == nullptr)
    {
      *destination = arguments[i];
      continue;
    }
  }

  if (help)
  {
    owner.log("Usage: cp SOURCE DESTINATION");
    owner.log("  --chunk-size  set chunk size, up to %u bytes", bufferLength);
    owner.log("  --help        print help message");
    return E_BUSY;
  }

  if (!*chunkSize || *chunkSize > bufferLength)
  {
    owner.log("cp: wrong chunk size, got %u, allowed up to %u", chunkSize,
        bufferLength);
    return E_VALUE;
  }

  if (*source == nullptr || *destination == nullptr)
  {
    owner.log("cp: not enough arguments");
    return E_ENTRY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result CopyEntry::run(unsigned int count, const char * const *arguments) const
{
  const char *sourcePath = nullptr, *destinationPath = nullptr;
  unsigned int chunkSize = bufferLength;
  result res;

  res = processArguments(count, arguments, &sourcePath, &destinationPath,
      &chunkSize);
  if (res != E_OK)
    return res;

  FsNode *destination = nullptr, *location = nullptr, *source = nullptr;
  FsMetadata info;
  fsNodeType type;

  Shell::joinPaths(context->pathBuffer, context->currentDir, destinationPath);
  const char *namePosition = Shell::extractName(context->pathBuffer);
  if (namePosition == nullptr)
    return E_VALUE; //Node name not found

  //Fill target entry metadata
  info.type = FS_TYPE_FILE;
  strcpy(info.name, namePosition);

  //Remove the file name from the destination path
  context->pathBuffer[namePosition - context->pathBuffer] = '\0';

  //Find destination directory where entry should be placed
  location = (FsNode *)fsFollow(owner.handle(), context->pathBuffer, nullptr);
  if (location == nullptr)
  {
    fsFree(source);
    owner.log("cp: %s: target directory not found", context->pathBuffer);
    return E_ENTRY;
  }
  //Check target directory entry type
  res = fsGet(location, FS_NODE_TYPE, &type);
  if (res != E_OK || type != FS_TYPE_DIR)
  {
    fsFree(location);
    owner.log("cp: %s: target directory type error", context->pathBuffer);
    return res == E_OK ? E_ENTRY : res;
  }

  //Find source entry
  Shell::joinPaths(context->pathBuffer, context->currentDir, sourcePath);
  source = (FsNode *)fsFollow(owner.handle(), context->pathBuffer, nullptr);
  if (source == nullptr)
  {
    owner.log("cp: %s: no such file", context->pathBuffer);
    return E_ENTRY;
  }
  //Check source entry type
  res = fsGet(source, FS_NODE_TYPE, &type);
  if (res != E_OK || type != FS_TYPE_FILE)
  {
    fsFree(location);
    fsFree(source);
    owner.log("cp: %s: source entry type error", context->pathBuffer);
    return res == E_OK ? E_ENTRY : res;
  }

  //Clone node descriptor
  destination = (FsNode *)fsClone(location);
  if (destination == nullptr)
  {
    fsFree(location);
    fsFree(source);
    owner.log("cp: node allocation failed");
    return E_ERROR;
  }

  //Create new node
  res = fsMake(location, &info, destination);
  fsFree(location);

  if (res != E_OK)
    owner.log("cp: node creation failed");
  else
    res = copyContent(source, destination, chunkSize);

  fsFree(destination);
  fsFree(source);

  return res;
}
//------------------------------------------------------------------------------
result ExitShell::run(unsigned int, const char * const *) const
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
result ListCommands::run(unsigned int, const char * const *) const
{
  for(auto entry : owner.commands())
    owner.log("%s", entry->name());

  return E_OK;
}
//------------------------------------------------------------------------------
result ListEntries::run(unsigned int count, const char * const *arguments) const
{
  const char *filter = nullptr, *path = nullptr;
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
    if (path == nullptr)
      path = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: ls [OPTION]... [DIRECTORY]...");
    owner.log("  -l                    show detailed information");
    owner.log("  --help                print help message");
    owner.log("  --filter=NAME         filter entries by NAME substring");
    owner.log("  --verify-count=COUNT  compare entry count with COUNT");
    return E_OK;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  FsNode *node = (FsNode *)fsFollow(owner.handle(), context->pathBuffer,
      nullptr);
  if (node == nullptr)
  {
    owner.log("ls: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  FsEntry *dir = (FsEntry *)fsOpen(node, FS_ACCESS_READ);
  if (dir == nullptr)
  {
    owner.log("ls: %s: open directory failed", context->pathBuffer);
    return E_DEVICE;
  }

  FsMetadata info;
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

      //Try to get update time
      fsGet(node, FS_NODE_TIME, &atime);

      if ((res = fsGet(node, FS_NODE_SIZE, &size)) != E_OK)
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

        owner.log("%s %10lu %s %s", accessStr, size, timeStr, info.name);
      }
      else
      {
        owner.log(info.name);
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
    owner.log("ls: entry count mismatch: got %u, expected %u", entries,
        verifyCount);
    return E_ERROR;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result MakeDirectory::run(unsigned int count,
    const char * const *arguments) const
{
  const char *target = nullptr;
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
    if (target == nullptr)
      target = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: mkdir [OPTION]... DIRECTORY");
    owner.log("  --help  print help message");
    return E_OK;
  }

  if (target == nullptr)
    return E_ENTRY;

  //Check for target entry existence
  Shell::joinPaths(context->pathBuffer, context->currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context->pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    fsFree(destinationNode);
    owner.log("mkdir: %s: entry already exists", context->pathBuffer);
    return E_ENTRY;
  }

  FsMetadata info;
  info.type = FS_TYPE_DIR;

  //Find destination directory where named entry should be placed
  const char *namePosition = Shell::extractName(context->pathBuffer);
  if (!namePosition)
    return E_VALUE; //No entry name found

  info.type = FS_TYPE_DIR;
  strcpy(info.name, namePosition);

  //Remove the directory name from the path
  context->pathBuffer[namePosition - context->pathBuffer] = '\0';

  FsNode *location = (FsNode *)fsFollow(owner.handle(), context->pathBuffer,
      nullptr);
  if (location == nullptr)
  {
    owner.log("mkdir: %s: target directory not found", context->pathBuffer);
    return E_ENTRY;
  }
  else
  {
    fsNodeType type;
    if ((res = fsGet(location, FS_NODE_TYPE, &type)) != E_OK
        || type != FS_TYPE_DIR)
    {
      fsFree(location);
      owner.log("mkdir: %s: wrong destination type", context->pathBuffer);
      return res == E_OK ? E_ENTRY : res;
    }
  }

  //Clone node to allocate node from the same handle
  destinationNode = (FsNode *)fsClone(location);
  if (destinationNode == nullptr)
  {
    fsFree(location);
    owner.log("mkdir: node allocation failed");
    return E_MEMORY;
  }

  res = fsMake(location, &info, destinationNode);
  fsFree(location);
  fsFree(destinationNode);

  if (res != E_OK)
    owner.log("mkdir: node creation failed");

  return res;
}
//------------------------------------------------------------------------------
result MeasureTime::run(unsigned int count, const char * const *arguments) const
{
  uint64_t start, delta;
  result res = E_VALUE;

  for(auto entry : owner.commands())
  {
    if (!strcmp(entry->name(), arguments[0]))
    {
      start = Shell::timestamp();
      res = entry->run(count - 1, arguments + 1);
      delta = Shell::timestamp() - start;

      owner.log("Time passed: %lu ns", delta);
    }
  }
  return res;
}
//------------------------------------------------------------------------------
result RemoveDirectory::run(unsigned int count,
    const char * const *arguments) const
{
  const char *target = nullptr;
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (target == nullptr)
      target = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: rmdir DIRECTORY");
    owner.log("  --help  print help message");
    return E_OK;
  }

  if (target == nullptr)
    return E_ENTRY;

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context->pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    owner.log("rmdir: %s: no such entry", context->pathBuffer);
    return E_ENTRY;
  }

  fsNodeType type;
  result res;

  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || type != FS_TYPE_DIR)
  {
    owner.log("rmdir: %s: wrong entry type", context->pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner.log("rmdir: %s: directory not empty", context->pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner.log("rmdir: %s: unlinking failed", context->pathBuffer);
    goto free_node;
  }

free_node:
  fsFree(destinationNode);
  return res;
}
//------------------------------------------------------------------------------
result RemoveEntry::run(unsigned int count, const char * const *arguments) const
{
  const char *target = nullptr;
  bool help = false, recursive = false;

  //TODO Implement recursive remove and multiple entries
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
    if (target == nullptr)
      target = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: rm [OPTION]... ENTRY");
    owner.log("  --help  print help message");
    owner.log("  -t      remove directories and their content");
    return E_OK;
  }

  if (target == nullptr)
    return E_ENTRY;

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context->pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    owner.log("rm: %s: no such entry", context->pathBuffer);
    return E_ENTRY;
  }

  fsNodeType type;
  result res;

  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || (type == FS_TYPE_DIR && !recursive))
  {
    owner.log("rm: %s: wrong entry type", context->pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner.log("rm: %s: payload removing failed", context->pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner.log("rm: %s: unlinking failed", context->pathBuffer);
    goto free_node;
  }

free_node:
  fsFree(destinationNode);
  return res;
}
