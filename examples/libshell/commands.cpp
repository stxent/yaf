/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include "libshell/commands.hpp"
//------------------------------------------------------------------------------
#ifndef CONFIG_SHELL_BUFFER
#define CONFIG_SHELL_BUFFER 512
#endif
//------------------------------------------------------------------------------
result DataProcessing::copyContent(FsNode *sourceNode, FsNode *destinationNode,
    unsigned int blockSize, unsigned int blockCount, unsigned int seek,
    unsigned int skip, bool overwrite) const
{
  FsEntry *sourceFile = nullptr, *destinationFile = nullptr;
  char buffer[CONFIG_SHELL_BUFFER];
  result res = E_OK;

  //Open file entries
  sourceFile = reinterpret_cast<FsEntry *>(fsOpen(sourceNode, FS_ACCESS_READ));
  if (sourceFile == nullptr)
  {
    owner.log("%s: source file opening error", name());
    return E_ENTRY;
  }

  if (overwrite)
  {
    res = fsTruncate(destinationNode);
    if (res != E_OK)
      return res;
  }

  destinationFile = reinterpret_cast<FsEntry *>(fsOpen(destinationNode,
      FS_ACCESS_WRITE));
  if (destinationFile == nullptr)
  {
    fsClose(sourceFile);
    owner.log("%s: source file opening error", name());
    return E_ENTRY;
  }

  uint32_t read = 0, written = 0, blocks = 0;

  if (!overwrite)
  {
    res = fsSeek(destinationFile, static_cast<uint64_t>(blockSize)
        * static_cast<uint64_t>(seek), FS_SEEK_SET);
    if (res != E_OK)
      return res;
  }
  if (skip)
  {
    res = fsSeek(sourceFile, static_cast<uint64_t>(blockSize)
        * static_cast<uint64_t>(skip), FS_SEEK_SET);
    if (res != E_OK)
      return res;
  }

  //Copy file content
  while (!fsEnd(sourceFile))
  {
    if (blockCount && blocks >= blockCount)
      break;
    ++blocks;

    uint32_t inCount = fsRead(sourceFile, buffer, blockSize);
    if (!inCount)
    {
      owner.log("%s: read error at %u", name(), read);
      res = E_ERROR;
      break;
    }
    read += inCount;

    const uint32_t outCount = inCount;
    inCount = fsWrite(destinationFile, buffer, outCount);
    if (inCount != outCount)
    {
      owner.log("%s: write error at %u", name(), written);
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
result DataProcessing::prepareNodes(Shell::ShellContext *context,
    FsNode **destination, FsNode **source, const char *destinationPath,
    const char *sourcePath)
{
  FsMetadata info;
  fsNodeType type;
  result res;

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
  FsNode * const location = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
  if (location == nullptr)
  {
    owner.log("%s: %s: target directory not found", name(),
        context->pathBuffer);
    return E_ENTRY;
  }
  //Check target directory entry type
  res = fsGet(location, FS_NODE_TYPE, &type);
  if (res != E_OK || type != FS_TYPE_DIR)
  {
    fsFree(location);
    owner.log("%s: %s: target directory type error", name(),
        context->pathBuffer);
    return res == E_OK ? E_ENTRY : res;
  }

  //Find source entry
  Shell::joinPaths(context->pathBuffer, context->currentDir, sourcePath);
  *source = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
  if (*source == nullptr)
  {
    fsFree(location);
    owner.log("%s: %s: no such file", name(), context->pathBuffer);
    return E_ENTRY;
  }
  //Check source entry type
  res = fsGet(*source, FS_NODE_TYPE, &type);
  if (res != E_OK || type != FS_TYPE_FILE)
  {
    fsFree(location);
    fsFree(*source);
    owner.log("%s: %s: source entry type error", name(), context->pathBuffer);
    return res == E_OK ? E_ENTRY : res;
  }

  //Check destination entry existence
  *destination = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      info.name, location));
  if (*destination)
  {
    //Entry already exists
    res = fsGet(*destination, FS_NODE_TYPE, &type);
    if (res != E_OK || type != FS_TYPE_FILE)
    {
      fsFree(*destination);
      fsFree(location);
      fsFree(*source);
      owner.log("%s: %s: destination entry type error", name(),
          destinationPath);
      return res == E_OK ? E_ENTRY : res;
    }

    fsFree(location);
  }
  else
  {
    //Clone node descriptor
    *destination = reinterpret_cast<FsNode *>(fsClone(location));
    if (*destination == nullptr)
    {
      fsFree(location);
      fsFree(*source);
      owner.log("%s: node allocation failed", name());
      return E_ERROR;
    }

    //Create new node
    res = fsMake(location, &info, *destination);
    fsFree(location);
    if (res != E_OK)
    {
      owner.log("%s: node creation failed", name());
      return res;
    }
  }

  return E_OK;
}
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
result ChangeDirectory::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *path = nullptr;
  result res;

  if ((res = processArguments(count, arguments, &path)) != E_OK)
    return res;

  FsNode *node;
  FsEntry *dir;

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
  if (node == nullptr)
  {
    owner.log("cd: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  dir = reinterpret_cast<FsEntry *>(fsOpen(node, FS_ACCESS_READ));
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
result CopyEntry::processArguments(unsigned int count,
    const char * const *arguments, const char **destination,
    const char **source) const
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
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
    owner.log("  --help  print help message");
    return E_BUSY;
  }

  if (*source == nullptr || *destination == nullptr)
  {
    owner.log("cp: not enough arguments");
    return E_ENTRY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result CopyEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *destinationPath = nullptr, *sourcePath = nullptr;
  result res;

  res = processArguments(count, arguments, &destinationPath, &sourcePath);
  if (res != E_OK)
    return res;

  FsNode *destination = nullptr, *source = nullptr;

  res = prepareNodes(context, &destination, &source, destinationPath,
      sourcePath);
  if (res != E_OK)
    return res;

  res = copyContent(source, destination, CONFIG_SHELL_BUFFER, 0, 0, 0, true);

  fsFree(destination);
  fsFree(source);

  return res;
}
//------------------------------------------------------------------------------
result DirectData::processArguments(unsigned int count,
    const char * const *arguments, Arguments *output) const
{
  bool help = false;

  output->in = output->out = nullptr;
  output->block = CONFIG_SHELL_BUFFER;
  output->count = 0; //Direct all blocks
  output->seek = output->skip = 0;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--bs"))
    {
      output->block = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--count"))
    {
      output->count = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "--seek"))
    {
      output->seek = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--skip"))
    {
      output->skip = atoi(arguments[++i]);
      continue;
    }
    if (!strcmp(arguments[i], "--if"))
    {
      output->in = arguments[++i];
      continue;
    }
    if (!strcmp(arguments[i], "--of"))
    {
      output->out = arguments[++i];
      continue;
    }
  }

  if (help)
  {
    owner.log("Usage: dd [OPTION]...");
    owner.log("  --bs BYTES     read and write up to BYTES at a time");
    owner.log("  --count COUNT  copy only COUNT input blocks");
    owner.log("  --help         print help message");
    owner.log("  --seek COUNT   skip COUNT blocks at start of output");
    owner.log("  --skip COUNT   skip COUNT blocks at start of input");
    owner.log("  --if FILE      read from FILE");
    owner.log("  --of FILE      write to FILE");
    return E_BUSY;
  }

  if (output->in == nullptr || output->out == nullptr)
  {
    owner.log("dd: not enough arguments");
    return E_ENTRY;
  }

  if (output->block > CONFIG_SHELL_BUFFER)
  {
    owner.log("dd: block size is too long");
    return E_VALUE;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result DirectData::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  Arguments parsed;
  result res;

  res = processArguments(count, arguments, &parsed);
  if (res != E_OK)
    return res;

  FsNode *destination = nullptr, *source = nullptr;

  res = prepareNodes(context, &destination, &source, parsed.out, parsed.in);
  if (res != E_OK)
    return res;

  res = copyContent(source, destination, parsed.block, parsed.count,
      parsed.seek, parsed.skip, false);

  fsFree(destination);
  fsFree(source);

  return res;
}
//------------------------------------------------------------------------------
result ExitShell::run(unsigned int, const char * const *, Shell::ShellContext *)
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
result ListCommands::run(unsigned int, const char * const *,
    Shell::ShellContext *)
{
  for (auto entry : owner.commands())
    owner.log("%s", entry->name());

  return E_OK;
}
//------------------------------------------------------------------------------
result ListEntries::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
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
    owner.log("  --filter NAME         filter entries by NAME substring");
    owner.log("  --verify-count COUNT  compare entry count with COUNT");
    return E_OK;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  FsNode *node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
  if (node == nullptr)
  {
    owner.log("ls: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  FsEntry *dir = reinterpret_cast<FsEntry *>(fsOpen(node, FS_ACCESS_READ));
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

      time64_t atime = 0;
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

        //Date and time
        char timeStr[24];
        time_t standardTime = static_cast<time_t>(atime);

        strftime(timeStr, 24, "%Y-%m-%d %H:%M:%S", gmtime(&standardTime));

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
    owner.log("ls: entry count mismatches: got %u, expected %u",
        entries, verifyCount);
    return E_ENTRY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result MakeDirectory::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
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
  FsNode *destinationNode = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
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

  FsNode *location = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
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
  destinationNode = reinterpret_cast<FsNode *>(fsClone(location));
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
result RemoveDirectory::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
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
  FsNode *destinationNode = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
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
result RemoveEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
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
    owner.log("  -r      remove directories and their content");
    return E_OK;
  }

  if (target == nullptr)
    return E_ENTRY;

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);
  FsNode *destinationNode = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
      context->pathBuffer, nullptr));
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
//------------------------------------------------------------------------------
result Synchronize::run(unsigned int, const char * const *,
    Shell::ShellContext *)
{
  result res = fsSync(owner.handle());

  return res;
}
