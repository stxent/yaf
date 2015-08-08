/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <bits.h>
#include "libshell/commands.hpp"

extern "C"
{
#include <realtime.h>
}
//------------------------------------------------------------------------------
#ifndef CONFIG_SHELL_BUFFER
#define CONFIG_SHELL_BUFFER 512
#endif
//------------------------------------------------------------------------------
result DataProcessing::copyContent(FsNode *sourceNode, FsNode *destinationNode,
    unsigned int blockSize, unsigned int blockCount, unsigned int seek,
    unsigned int skip) const
{
  uint64_t sourcePosition = static_cast<uint64_t>(blockSize) * skip;
  uint64_t destinationPosition = static_cast<uint64_t>(blockSize)
      * seek;
  uint32_t blocks = 0;
  char buffer[CONFIG_SHELL_BUFFER];
  result res = E_OK;

  //Copy file content
  while (!blockCount || blocks++ < blockCount)
  {
    uint32_t read, written;

    res = fsNodeRead(sourceNode, FS_NODE_DATA, sourcePosition, buffer,
        blockSize, &read);
    if (res == E_EMPTY)
    {
      res = E_OK;
      break;
    }
    if (res != E_OK)
    {
      owner.log("%s: read error at %u", name(), sourcePosition);
      break;
    }
    sourcePosition += read;

    res = fsNodeWrite(destinationNode, FS_NODE_DATA, destinationPosition,
        buffer, read, &written);
    if (res != E_OK || read != written)
    {
      owner.log("%s: write error at %u", name(), destinationPosition);
      if (res == E_OK)
        res = E_ERROR;
      break;
    }
    destinationPosition += written;
  }

  return res;
}
//------------------------------------------------------------------------------
result DataProcessing::prepareNodes(Shell::ShellContext *context,
    FsNode **destination, FsNode **source, const char *destinationPath,
    const char *sourcePath, bool overwrite)
{
  FsNode *destinationNode;
  FsNode *sourceNode;
  result res;

  Shell::joinPaths(context->pathBuffer, context->currentDir, destinationPath);

  destinationNode = followPath(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("cp: %s: entry already exists", context->pathBuffer);
    return E_EXIST;
  }

  //Find destination directory where named entry should be placed
  const char * const namePosition = Shell::extractName(destinationPath);

  if (!namePosition)
    return E_VALUE; //No entry name found

  //Remove the directory name from the path
  const uint32_t nameOffset = strlen(context->pathBuffer) -
      (strlen(destinationPath) - (namePosition - destinationPath));
  context->pathBuffer[nameOffset] = '\0';

  FsNode * const root = followPath(context->pathBuffer);

  if (root == nullptr)
  {
    owner.log("cp: %s: target directory not found", context->pathBuffer);
    return E_ENTRY;
  }

  const FsAttributeDescriptor descriptors[] = {
      //Name descriptor
      {
          namePosition,
          static_cast<uint32_t>(strlen(namePosition)) + 1,
          FS_NODE_NAME
      },
      //Payload descriptor
      {
          nullptr,
          0,
          FS_NODE_DATA
      }
  };

  res = fsNodeCreate(root, descriptors, ARRAY_SIZE(descriptors));
  fsNodeFree(root);
  if (res != E_OK)
  {
    owner.log("cp: %s: creation failed", namePosition);
    return res;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, destinationPath);
  destinationNode = followPath(context->pathBuffer);
  if (destinationNode == nullptr)
  {
    owner.log("cp: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, sourcePath);
  sourceNode = followPath(context->pathBuffer);
  if (sourceNode == nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("cp: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  *source = sourceNode;
  *destination = destinationNode;
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
    return res; //FIXME E_BUSY

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);

  FsNode * const node = followPath(context->pathBuffer);

  if (node == nullptr)
  {
    owner.log("cd: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  //Check whether the node has any descendants
  if (fsNodeLength(node, FS_NODE_DATA, nullptr) == E_OK)
  {
    owner.log("cd: %s: not a directory", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }

  //Check access rights
  access_t access;

  res = fsNodeRead(node, FS_NODE_ACCESS, 0, &access, sizeof(access), nullptr);
  if (res == E_OK)
  {
    if (!(access & FS_ACCESS_READ))
    {
      owner.log("cd: %s: access denied", context->pathBuffer);
      fsNodeFree(node);
      return E_ACCESS;
    }
  }
  else
  {
    owner.log("cd: %s: error reading attributes", context->pathBuffer);
    fsNodeFree(node);
    return E_ERROR;
  }

  fsNodeFree(node);

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
    if (*source != nullptr && *destination != nullptr)
    {
      owner.log("cp: argument processing error");
      return E_VALUE;
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
    return E_VALUE;
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
      sourcePath, true);
  if (res != E_OK)
    return res;

  res = copyContent(source, destination, CONFIG_SHELL_BUFFER, 0, 0, 0);

  fsNodeFree(destination);
  fsNodeFree(source);

  return res;
}
//------------------------------------------------------------------------------
result DirectData::processArguments(unsigned int count,
    const char * const *arguments, Arguments *output) const
{
  bool argumentError = false;
  bool help = false;

  output->in = output->out = nullptr;
  output->block = CONFIG_SHELL_BUFFER;
  output->count = 0; //Direct all blocks
  output->seek = output->skip = 0;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--bs"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->block = atoi(arguments[i]);
      continue;
    }
    if (!strcmp(arguments[i], "--count"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->count = atoi(arguments[i]);
      continue;
    }
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "--seek"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->seek = atoi(arguments[i]);
      continue;
    }
    if (!strcmp(arguments[i], "--skip"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->skip = atoi(arguments[i]);
      continue;
    }
    if (!strcmp(arguments[i], "--if"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->in = arguments[i];
      continue;
    }
    if (!strcmp(arguments[i], "--of"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      output->out = arguments[i];
      continue;
    }
  }

  if (argumentError)
  {
    owner.log("dd: argument processing error");
    return E_VALUE;
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
    return E_VALUE;
  }

  if (output->block > CONFIG_SHELL_BUFFER)
  {
    owner.log("dd: block is too large");
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

  res = prepareNodes(context, &destination, &source, parsed.out, parsed.in,
      false);
  if (res != E_OK)
    return res;

  res = copyContent(source, destination, parsed.block, parsed.count,
      parsed.seek, parsed.skip);

  fsNodeFree(destination);
  fsNodeFree(source);

  return res;
}
//------------------------------------------------------------------------------
result ExitShell::run(unsigned int, const char * const *, Shell::ShellContext *)
{
  return static_cast<result>(Shell::E_SHELL_EXIT);
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
  bool argumentError = false;
  bool help = false, showIndex = false, verbose = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "-i"))
    {
      showIndex = true;
      continue;
    }
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
    if (!strcmp(arguments[i], "--filter"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      filter = arguments[i];
      continue;
    }
    if (!strcmp(arguments[i], "--verify-count"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      verifyCount = atoi(arguments[i]);
      continue;
    }
    if (path == nullptr)
      path = arguments[i];
  }

  if (argumentError)
  {
    owner.log("ls: argument processing error");
    return E_VALUE;
  }

  if (help)
  {
    owner.log("Usage: ls [OPTION]... [DIRECTORY]...");
    owner.log("  -i                    show index of each entry");
    owner.log("  -l                    show detailed information");
    owner.log("  --help                print help message");
    owner.log("  --filter NAME         filter entries by NAME substring");
    owner.log("  --verify-count COUNT  compare entry count with COUNT");
    return E_OK;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);

  FsNode * const parentNode = followPath(context->pathBuffer);

  if (parentNode == nullptr)
  {
    owner.log("ls: %s: no such directory", context->pathBuffer);
    return E_ENTRY;
  }

  FsNode *child = reinterpret_cast<FsNode *>(fsNodeHead(parentNode));

  fsNodeFree(parentNode);
  if (child == nullptr)
  {
    owner.log("ls: %s: not a directory", context->pathBuffer);
    return E_ENTRY;
  }

  uint64_t nodeId;
  uint64_t nodeSize;
  time64_t nodeTime;
  access_t nodeAccess;
  char nodeName[CONFIG_FILENAME_LENGTH];
  bool isDirectory;

  int entries = 0;
  result res;

  do
  {
    res = fsNodeRead(child, FS_NODE_NAME, 0, nodeName, sizeof(nodeName),
        nullptr);
    if (res != E_OK)
    {
      owner.log("ls: error reading name attribute", context->pathBuffer);
      res = E_ERROR;
      break;
    }

    if (filter && !strstr(nodeName, filter))
      continue;

    res = fsNodeRead(child, FS_NODE_ACCESS, 0, &nodeAccess, sizeof(nodeAccess),
        nullptr);
    if (res != E_OK)
    {
      owner.log("ls: %s: error reading access attribute", nodeName);
      res = E_ERROR;
      break;
    }

    res = fsNodeRead(child, FS_NODE_ID, 0, &nodeId, sizeof(nodeId), nullptr);
    if (res != E_OK)
      nodeId = 0;

    res = fsNodeLength(child, FS_NODE_DATA, &nodeSize);
    if (res != E_OK)
      nodeSize = 0;
    isDirectory = res == E_INVALID;

    res = fsNodeRead(child, FS_NODE_TIME, 0, &nodeTime, sizeof(nodeTime),
        nullptr);
    if (res != E_OK)
      nodeTime = 0;

    if (verbose)
    {
      //Access
      char accessStr[4];

      accessStr[0] = isDirectory ? 'd' : '-';
      accessStr[1] = nodeAccess & FS_ACCESS_READ ? 'r' : '-';
      accessStr[2] = nodeAccess & FS_ACCESS_WRITE ? 'w' : '-';
      accessStr[3] = '\0';

      //Date and time
      char timeStr[24];
      time_t standardTime = static_cast<time_t>(nodeTime);

      strftime(timeStr, 24, "%Y-%m-%d %H:%M:%S", gmtime(&standardTime));

      if (showIndex)
      {
        //Index number of the entry
        owner.log("%12lX %s %10lu %s %s", nodeId, accessStr, nodeSize,
            timeStr, nodeName);
      }
      else
      {
        owner.log("%s %10lu %s %s", accessStr, nodeSize, timeStr, nodeName);
      }
    }
    else
    {
      owner.log(nodeName);
    }
    ++entries;
  }
  while ((res = fsNodeNext(child)) == E_OK);

  fsNodeFree(child);

  if (res != E_OK && res != E_ENTRY)
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
  {
    owner.log("mkdir: directory is not specified");
    return E_VALUE;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);

  FsNode * const destinationNode = followPath(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("mkdir: %s: entry already exists", context->pathBuffer);
    return E_EXIST;
  }

  //Find destination directory where named entry should be placed
  const char * const namePosition = Shell::extractName(target);

  if (!namePosition)
    return E_VALUE; //No entry name found

  //Remove the directory name from the path
  const uint32_t nameOffset = strlen(context->pathBuffer) -
      (strlen(target) - (namePosition - target));
  context->pathBuffer[nameOffset] = '\0';

  FsNode * const root = followPath(context->pathBuffer);

  if (root == nullptr)
  {
    owner.log("mkdir: %s: target directory not found", context->pathBuffer);
    return E_ENTRY;
  }

  const FsAttributeDescriptor descriptors[] = {
      //Name descriptor
      {
          namePosition,
          static_cast<uint32_t>(strlen(namePosition)) + 1,
          FS_NODE_NAME
      }
  };

  res = fsNodeCreate(root, descriptors, ARRAY_SIZE(descriptors));
  fsNodeFree(root);
  if (res != E_OK)
  {
    owner.log("mkdir: %s: creation failed", namePosition);
    return res;
  }

  return E_OK;
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

  FsNode * const node = followPath(context->pathBuffer);

  if (node == nullptr)
  {
    owner.log("rmdir: %s: no such entry", context->pathBuffer);
    return E_ENTRY;
  }

  //Check whether the node has any descendants
  if (fsNodeLength(node, FS_NODE_DATA, nullptr) == E_OK)
  {
    owner.log("rmdir: %s: not a directory", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }

  //Find root directory
  const char * const namePosition = Shell::extractName(target);

  if (!namePosition)
    return E_VALUE; //No entry name found

  //Remove the directory name from the path
  const uint32_t nameOffset = strlen(context->pathBuffer) -
      (strlen(target) - (namePosition - target));
  context->pathBuffer[nameOffset] = '\0';

  FsNode * const root = followPath(context->pathBuffer);

  if (root == nullptr)
  {
    owner.log("rmdir: %s: no such entry", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }

  const enum result res = fsNodeRemove(root, node);

  fsNodeFree(root);
  fsNodeFree(node);

  if (res != E_OK)
  {
    owner.log("rmdir: %s/%s: directory deletion failed", context->pathBuffer,
        context->pathBuffer + nameOffset + 1);
  }

  return res;
}
//------------------------------------------------------------------------------
//result RemoveEntry::processArguments(unsigned int count,
//    const char * const *arguments, bool *recursive, const char **targets) const
//{
//  unsigned int entries = 0;
//  bool help = false;
//
//  for (unsigned int i = 0; i < count; ++i)
//  {
//    if (!strcmp(arguments[i], "--help"))
//    {
//      help = true;
//      continue;
//    }
//    if (!strcmp(arguments[i], "-r"))
//    {
//      *recursive = true;
//      continue;
//    }
//    targets[entries++] = arguments[i];
//  }
//
//  if (help)
//  {
//    owner.log("Usage: rm [OPTION]... ENTRY");
//    owner.log("  --help  print help message");
//    owner.log("  -r      remove directories and their content");
//    return E_BUSY;
//  }
//
//  return !entries ? E_ENTRY : E_OK;
//}
////------------------------------------------------------------------------------
//result RemoveEntry::removeRecursively(FsNode *node,
//    Shell::ShellContext *context) const
//{
//  FsEntry *dir = reinterpret_cast<FsEntry *>(fsOpen(node, FS_ACCESS_READ));
//
//  if (dir == nullptr)
//  {
//    owner.log("rm: directory opening failed");
//    return E_DEVICE;
//  }
//
//  FsNode *iterator;
//  result res;
//
//  if ((iterator = reinterpret_cast<FsNode *>(fsClone(node))) == nullptr)
//  {
//    fsClose(dir);
//
//    owner.log("rm: node allocation failed");
//    return E_MEMORY;
//  }
//
//  //Previously allocated node is reused
//  while ((res = fsFetch(dir, iterator)) == E_OK)
//  {
//    if (fsEnd(dir))
//    {
//      owner.log("rm: unexpected end of directory");
//      break;
//    }
//
//    fsNodeType type;
//
//    if ((res = fsGet(node, FS_NODE_TYPE, &type)) != E_OK)
//    {
//      owner.log("rm: wrong entry");
//      goto free_node;
//    }
//
//    if (type == FS_TYPE_DIR)
//    {
//      if ((res = removeRecursively(iterator, context)) != E_OK)
//        goto free_node;
//    }
//
//    if ((res = fsTruncate(iterator)) != E_OK)
//    {
//      owner.log("rm: payload deletion failed");
//      goto free_node;
//    }
//
//    if ((res = fsUnlink(iterator)) != E_OK)
//    {
//      owner.log("rm: unlinking failed");
//      goto free_node;
//    }
//  }
//
//  res = E_OK;
//
//  free_node:
//  fsFree(iterator);
//  fsClose(dir);
//
//  return res;
//}
////------------------------------------------------------------------------------
//result RemoveEntry::run(unsigned int count, const char * const *arguments,
//    Shell::ShellContext *context)
//{
//  const char *targets[Shell::ARGUMENT_COUNT - 1] = {nullptr};
//  FsNode *node;
//  result res;
//  bool recursive = false;
//
//  if ((res = processArguments(count, arguments, &recursive, targets)) != E_OK)
//    return res;
//
//  for (unsigned int i = 0; i < Shell::ARGUMENT_COUNT - 1; ++i)
//  {
//    if (targets[i] == nullptr)
//      break;
//
//    Shell::joinPaths(context->pathBuffer, context->currentDir, targets[i]);
//    node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
//        context->pathBuffer, nullptr));
//
//    if (node == nullptr)
//    {
//      owner.log("rm: %s: no such entry", context->pathBuffer);
//      res = E_ENTRY;
//      break;
//    }
//
//    fsNodeType type;
//
//    if ((res = fsGet(node, FS_NODE_TYPE, &type)) != E_OK)
//    {
//      owner.log("rm: %s: wrong entry", context->pathBuffer);
//      fsFree(node);
//      break;
//    }
//
//    if (type == FS_TYPE_DIR)
//    {
//      if (!recursive)
//      {
//        owner.log("rm: %s: directory ignored", context->pathBuffer);
//        res = E_INVALID;
//      }
//      else if ((res = removeRecursively(node, context)) != E_OK)
//      {
//        owner.log("rm: %s: recursive deletion failed", context->pathBuffer);
//      }
//
//      if (res != E_OK)
//      {
//        fsFree(node);
//        break;
//      }
//    }
//
//    if ((res = fsTruncate(node)) != E_OK)
//    {
//      owner.log("rm: %s: payload deletion failed", context->pathBuffer);
//    }
//    else if ((res = fsUnlink(node)) != E_OK)
//    {
//      owner.log("rm: %s: unlinking failed", context->pathBuffer);
//    }
//
//    fsFree(node);
//
//    if (res != E_OK)
//      break;
//  }
//
//  return res;
//}
//------------------------------------------------------------------------------
result Synchronize::run(unsigned int, const char * const *,
    Shell::ShellContext *)
{
  result res = fsHandleSync(owner.handle());

  return res;
}
//------------------------------------------------------------------------------
result TouchEntry::processArguments(unsigned int count,
    const char * const *arguments, const char **targets) const
{
  unsigned int entries = 0;
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    targets[entries++] = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: touch ENTRY");
    owner.log("  --help  print help message");
    return E_BUSY;
  }

  return !entries ? E_ENTRY : E_OK;
}
//------------------------------------------------------------------------------
result TouchEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *targets[Shell::ARGUMENT_COUNT - 1] = {nullptr};
  result res;

  if ((res = processArguments(count, arguments, targets)) != E_OK)
    return res;

  for (unsigned int i = 0; i < Shell::ARGUMENT_COUNT - 1; ++i)
  {
    if (targets[i] == nullptr)
      break;

    Shell::joinPaths(context->pathBuffer, context->currentDir, targets[i]);

    FsNode * const destinationNode = followPath(context->pathBuffer);
    if (destinationNode != nullptr)
    {
      fsNodeFree(destinationNode);
      owner.log("touch: %s: entry already exists", context->pathBuffer);
      res = E_EXIST;
      break;
    }

    //Find destination directory where named entry should be placed
    const char * const namePosition = Shell::extractName(targets[i]);

    if (!namePosition)
    {
      //No entry name found
      res = E_VALUE;
      break;
    }

    //Remove the directory name from the path
    const uint32_t nameOffset = strlen(context->pathBuffer) -
        (strlen(targets[i]) - (namePosition - targets[i]));
    context->pathBuffer[nameOffset] = '\0';

    FsNode * const root = followPath(context->pathBuffer);

    if (root == nullptr)
    {
      owner.log("touch: %s: target directory not found", context->pathBuffer);
      res = E_ENTRY;
      break;
    }

    const FsAttributeDescriptor descriptors[] = {
        //Name descriptor
        {
            namePosition,
            static_cast<uint32_t>(strlen(namePosition)) + 1,
            FS_NODE_NAME
        },
        //Payload descriptor
        {
            nullptr,
            0,
            FS_NODE_DATA
        }
    };

    res = fsNodeCreate(root, descriptors, ARRAY_SIZE(descriptors));
    fsNodeFree(root);
    if (res != E_OK)
    {
      owner.log("touch: %s: creation failed", namePosition);
      break;
    }
  }

  return res;
}
