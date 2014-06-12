/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <ctime>
#include "commands.hpp"
//------------------------------------------------------------------------------
bool ChangeDirectory::processArguments(unsigned int count,
    const char * const *arguments, const char **path, result *res) const
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
    *res = E_OK;
    return false;
  }

  if (*path == nullptr)
  {
    *res = E_ENTRY;
    return false;
  }

  *res = E_OK;
  return true;
}
//------------------------------------------------------------------------------
result ChangeDirectory::run(unsigned int count,
    const char * const *arguments) const
{
  const char *path = nullptr;
  result res;

  if (!processArguments(count, arguments, &path, &res))
    return res;

  FsNode *node;
  FsEntry *dir;

  Shell::joinPaths(context.pathBuffer, context.currentDir, path);
  node = (FsNode *)fsFollow(owner.handle(), context.pathBuffer, nullptr);
  if (node == nullptr)
  {
    owner.log("cd: %s: no such directory", context.pathBuffer);
    return E_ENTRY;
  }

  dir = (FsEntry *)fsOpen(node, FS_ACCESS_READ);
  fsFree(node);

  if (dir == nullptr)
  {
    owner.log("cd: %s: access denied", context.pathBuffer);
    return E_ACCESS;
  }

  fsClose(dir);
  strcpy(context.currentDir, context.pathBuffer);

  return E_OK;
}
//------------------------------------------------------------------------------
result CopyEntry::run(unsigned int count, const char * const *arguments) const
{
  const char *sourcePath = nullptr, *targetPath = nullptr;
  char buffer[bufferLength];
  unsigned int chunkSize = bufferLength;
  bool help = false;

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
    if (sourcePath == nullptr)
    {
      sourcePath = arguments[i];
      continue;
    }
    if (targetPath == nullptr)
    {
      targetPath = arguments[i];
      continue;
    }
  }

  if (help)
  {
    owner.log("Usage: cp SOURCE DESTINATION");
    owner.log("  --chunk-size  set chunk size, up to %u bytes", bufferLength);
    owner.log("  --help        print help message");
    return E_OK;
  }

  if (!chunkSize || chunkSize > bufferLength)
  {
    owner.log("cp: wrong chunk size, got %u, allowed up to %u", chunkSize,
        bufferLength);
    return E_VALUE;
  }

  if (sourcePath == nullptr || targetPath == nullptr)
    return E_ENTRY;

  FsNode *location = nullptr, *source = nullptr, *target = nullptr;
  FsMetadata info;
  result res;

  //Find destination directory
  Shell::joinPaths(context.pathBuffer, context.currentDir, targetPath);
  location = (FsNode *)fsFollow(owner.handle(), context.pathBuffer, nullptr);
  if (location != nullptr)
  {
    fsFree(location);
    owner.log("cp: %s: entry already exists", context.pathBuffer);
    return E_ENTRY;
  }

  //Find source entry
  Shell::joinPaths(context.pathBuffer, context.currentDir, sourcePath);
  source = (FsNode *)fsFollow(owner.handle(), context.pathBuffer, nullptr);
  if (source == nullptr)
  {
    owner.log("cp: %s: no such file or directory", context.pathBuffer);
    return E_ENTRY;
  }
  if ((res = fsGet(source, FS_NODE_METADATA, &info)) != E_OK
      || info.type != FS_TYPE_FILE)
  {
    fsFree(source);
    if (res == E_OK)
    {
      owner.log("cp: %s: entry is not a file", context.pathBuffer);
      return E_ENTRY;
    }
    else
      return res;
  }

  //Find destination directory where entry should be placed
  Shell::joinPaths(context.pathBuffer, context.currentDir, targetPath);
  char *namePosition = Shell::extractName(context.pathBuffer);
  if (namePosition == nullptr)
    return E_VALUE; //Node name not found

  strcpy(info.name, namePosition);
  *namePosition = '\0';

  location = (FsNode *)fsFollow(owner.handle(), context.pathBuffer, nullptr);
  if (location == nullptr)
  {
    fsFree(source);
    owner.log("cp: %s: target directory not found", context.pathBuffer);
    return E_ENTRY;
  }

  //Clone node descriptor
  target = (FsNode *)fsClone(location);
  if (target == nullptr)
  {
    fsFree(location);
    fsFree(source);
    owner.log("cp: node allocation failed");
    return E_MEMORY;
  }

  //Create new node
  if (fsMake(location, &info, target) != E_OK)
  {
    fsFree(target);
    fsFree(location);
    fsFree(source);
    owner.log("cp: node creation failed");
    return E_ERROR;
  }
  fsFree(location);

  FsEntry *sourceFile = nullptr, *targetFile = nullptr;

  //Open file entries
  sourceFile = (FsEntry *)fsOpen(source, FS_ACCESS_READ);
  fsFree(source);
  targetFile = (FsEntry *)fsOpen(target, FS_ACCESS_WRITE);
  fsFree(target);

  if (sourceFile == nullptr || targetFile == nullptr)
  {
    if (sourceFile)
      fsClose(sourceFile);
    owner.log("cp: opening failed", sourcePath);
    return E_ERROR;
  }

  uint32_t read = 0, written = 0;
  res = E_OK;

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
    inCount = fsWrite(targetFile, buffer, outCount);
    if (inCount != outCount)
    {
      owner.log("cp: write error at %u", written);
      res = E_ERROR;
      break;
    }
    written += inCount;
  }

  fsClose(sourceFile);
  fsClose(targetFile);

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

  Shell::joinPaths(context.pathBuffer, context.currentDir, path);
  FsNode *node = (FsNode *)fsFollow(owner.handle(), context.pathBuffer,
      nullptr);
  if (node == nullptr)
  {
    owner.log("ls: %s: no such directory", context.pathBuffer);
    return E_ENTRY;
  }

  FsEntry *dir = (FsEntry *)fsOpen(node, FS_ACCESS_READ);
  if (dir == nullptr)
  {
    owner.log("ls: %s: open directory failed", context.pathBuffer);
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
  Shell::joinPaths(context.pathBuffer, context.currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context.pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    fsFree(destinationNode);
    owner.log("mkdir: %s: entry already exists", context.pathBuffer);
    return E_ENTRY;
  }

  FsMetadata info;
  info.type = FS_TYPE_DIR;

  //Find destination directory where named entry should be placed
  char *namePosition = Shell::extractName(context.pathBuffer);
  if (!namePosition)
    return E_VALUE; //No entry name found

  strcpy(info.name, namePosition);
  *namePosition = '\0';

  FsNode *location = (FsNode *)fsFollow(owner.handle(), context.pathBuffer,
      nullptr);
  if (location == nullptr)
  {
    owner.log("mkdir: %s: target directory not found", context.pathBuffer);
    return E_ENTRY;
  }
  else
  {
    fsNodeType type;
    if ((res = fsGet(location, FS_NODE_TYPE, &type)) != E_OK
        || type != FS_TYPE_DIR)
    {
      fsFree(location);
      owner.log("mkdir: %s: wrong destination type", context.pathBuffer);
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

  Shell::joinPaths(context.pathBuffer, context.currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context.pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    owner.log("rmdir: %s: no such entry", context.pathBuffer);
    return E_ENTRY;
  }

  fsNodeType type;
  result res;

  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || type != FS_TYPE_DIR)
  {
    owner.log("rmdir: %s: wrong entry type", context.pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner.log("rmdir: %s: directory not empty", context.pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner.log("rmdir: %s: unlinking failed", context.pathBuffer);
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

  Shell::joinPaths(context.pathBuffer, context.currentDir, target);
  FsNode *destinationNode = (FsNode *)fsFollow(owner.handle(),
      context.pathBuffer, nullptr);
  if (destinationNode == nullptr)
  {
    owner.log("rm: %s: no such entry", context.pathBuffer);
    return E_ENTRY;
  }

  fsNodeType type;
  result res;

  if ((res = fsGet(destinationNode, FS_NODE_TYPE, &type)) != E_OK
      || (type == FS_TYPE_DIR && !recursive))
  {
    owner.log("rm: %s: wrong entry type", context.pathBuffer);
    goto free_node;
  }

  if ((res = fsTruncate(destinationNode)) != E_OK)
  {
    owner.log("rm: %s: payload removing failed", context.pathBuffer);
    goto free_node;
  }

  if ((res = fsUnlink(destinationNode)) != E_OK)
  {
    owner.log("rm: %s: unlinking failed", context.pathBuffer);
    goto free_node;
  }

free_node:
  fsFree(destinationNode);
  return res;
}
