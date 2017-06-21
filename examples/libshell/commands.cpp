/*
 * commands.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include "libshell/commands.hpp"

extern "C"
{
#include <xcore/bits.h>
#include <xcore/realtime.h>
}
//------------------------------------------------------------------------------
#ifndef CONFIG_SHELL_BUFFER
#define CONFIG_SHELL_BUFFER 512
#endif
//------------------------------------------------------------------------------
Result DataProcessing::copyContent(FsNode *sourceNode, FsNode *destinationNode,
    unsigned int blockSize, unsigned int blockCount, unsigned int seek,
    unsigned int skip) const
{
  length_t sourcePosition = static_cast<length_t>(blockSize) * skip;
  length_t destinationPosition = static_cast<length_t>(blockSize)
      * seek;
  uint32_t blocks = 0;
  char buffer[CONFIG_SHELL_BUFFER];
  Result res = E_OK;

  //Copy file content
  while (!blockCount || blocks++ < blockCount)
  {
    length_t read, written;

    res = fsNodeRead(sourceNode, FS_NODE_DATA, sourcePosition, buffer,
        static_cast<length_t>(blockSize), &read);
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
Result DataProcessing::prepareNodes(Shell::ShellContext *context,
    FsNode **destination, FsNode **source, const char *destinationPath,
    const char *sourcePath, bool overwrite)
{
  Result res;

  const char * const nodeName = Shell::extractName(destinationPath);
  if (nodeName == nullptr)
  {
    owner.log("%s: %s: incorrect name", name(), destinationPath);
    return E_VALUE;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, sourcePath);
  FsNode * const sourceNode = openNode(context->pathBuffer);
  if (sourceNode == nullptr)
  {
    owner.log("%s: %s: node not found", name(), context->pathBuffer);
    return E_ENTRY;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, destinationPath);

  FsNode *destinationNode = openNode(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    if (overwrite)
    {
      res = removeNode(context, destinationNode, context->pathBuffer);
      if (res != E_OK)
      {
        owner.log("%s: %s: overwrite failed", name(), context->pathBuffer);
      }
    }
    else
    {
      owner.log("%s: %s: node already exists", name(), context->pathBuffer);
      res = E_EXIST;
    }
    fsNodeFree(destinationNode);

    if (res != E_OK)
    {
      fsNodeFree(sourceNode);
      return res;
    }
  }

  FsNode * const root = openBaseNode(context->pathBuffer);
  if (root == nullptr)
  {
    fsNodeFree(sourceNode);
    owner.log("%s: %s: target root node not found", name(),
        context->pathBuffer);
    return E_ENTRY;
  }

  const FsFieldDescriptor descriptors[] = {
      //Name descriptor
      {
          nodeName,
          static_cast<length_t>(strlen(nodeName) + 1),
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
    fsNodeFree(sourceNode);
    owner.log("%s: %s: creation failed", name(), context->pathBuffer);
    return res;
  }

  destinationNode = openNode(context->pathBuffer);
  if (destinationNode == nullptr)
  {
    fsNodeFree(sourceNode);
    owner.log("%s: %s: node not found", name(), context->pathBuffer);
    return E_ENTRY;
  }

  *source = sourceNode;
  *destination = destinationNode;
  return E_OK;
}
//------------------------------------------------------------------------------
Result ChangeDirectory::processArguments(unsigned int count,
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
  {
    owner.log("cd: not enough arguments");
    return E_ENTRY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
Result DataProcessing::removeNode(Shell::ShellContext *context, FsNode *node,
    char *path)
{
  FsNode *root;
  Result res;

  if ((res = fsNodeLength(node, FS_NODE_DATA, nullptr)) != E_OK)
  {
    owner.log("%s: %s: data-less node ignored", name(), context->pathBuffer);
    return res;
  }

  if ((root = openBaseNode(path)) == nullptr)
  {
    owner.log("%s: %s: root node not found", name(), context->pathBuffer);
    return E_ENTRY;
  }

  if ((res = fsNodeRemove(root, node)) != E_OK)
  {
    owner.log("%s: %s: deletion failed", name(), context->pathBuffer);
  }

  fsNodeFree(root);
  return res;
}
//------------------------------------------------------------------------------
Result ChangeDirectory::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *path = nullptr;
  Result res;

  if ((res = processArguments(count, arguments, &path)) != E_OK)
    return res; //FIXME E_BUSY

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  FsNode * const node = openNode(context->pathBuffer);
  if (node == nullptr)
  {
    owner.log("cd: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  //Check whether the node has any descendants
  FsNode * const descendant = reinterpret_cast<FsNode *>(fsNodeHead(node));
  if (descendant == nullptr)
  {
    owner.log("cd: %s: node is empty", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }
  fsNodeFree(descendant);

  //Check access rights
  access_t access;
  res = fsNodeRead(node, FS_NODE_ACCESS, 0, &access, sizeof(access), nullptr);
  fsNodeFree(node);

  if (res == E_OK)
  {
    if (!(access & FS_ACCESS_READ))
    {
      owner.log("cd: %s: access denied", context->pathBuffer);
      return E_ACCESS;
    }
  }
  else
  {
    owner.log("cd: %s: error reading attributes", context->pathBuffer);
    return E_ERROR;
  }

  strcpy(context->currentDir, context->pathBuffer);
  return E_OK;
}
//------------------------------------------------------------------------------
Result CopyEntry::processArguments(unsigned int count,
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
Result CopyEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *destinationPath = nullptr, *sourcePath = nullptr;
  Result res;

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
Result DirectData::processArguments(unsigned int count,
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
Result DirectData::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  Arguments parsed;
  Result res;

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
Result ExitShell::run(unsigned int, const char * const *, Shell::ShellContext *)
{
  return static_cast<Result>(Shell::E_SHELL_EXIT);
}
//------------------------------------------------------------------------------
Result ListCommands::run(unsigned int, const char * const *,
    Shell::ShellContext *)
{
  for (auto entry : owner.commands())
    owner.log("%s", entry->name());

  return E_OK;
}
//------------------------------------------------------------------------------
Result ListEntries::run(unsigned int count, const char * const *arguments,
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
    owner.log("  -i                    show index of each node");
    owner.log("  -l                    show detailed information");
    owner.log("  --help                print help message");
    owner.log("  --filter NAME         filter entries by NAME substring");
    owner.log("  --verify-count COUNT  compare node count with COUNT");
    return E_OK;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, path);
  FsNode * const root = openNode(context->pathBuffer);
  if (root == nullptr)
  {
    owner.log("ls: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  FsNode * const child = reinterpret_cast<FsNode *>(fsNodeHead(root));
  fsNodeFree(root);
  if (child == nullptr)
  {
    owner.log("ls: %s: node is empty", context->pathBuffer);
    return E_ENTRY;
  }

  int entries = 0;
  Result res;

  do
  {
    char nodeName[CONFIG_FILENAME_LENGTH];
    uint64_t nodeId = 0;
    time64_t nodeTime = 0;
    length_t nodeSize = 0;
    access_t nodeAccess;
    bool isDirectory = false;

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

    fsNodeRead(child, FS_NODE_ID, 0, &nodeId, sizeof(nodeId), nullptr);
    fsNodeLength(child, FS_NODE_DATA, &nodeSize);
    fsNodeRead(child, FS_NODE_TIME, 0, &nodeTime, sizeof(nodeTime), nullptr);

    FsNode * const descendant = reinterpret_cast<FsNode *>(fsNodeHead(child));
    if (descendant)
    {
      isDirectory = true;
      fsNodeFree(descendant);
    }

    if (verbose)
    {
      //Access
      char printableNodeAccess[4];

      printableNodeAccess[0] = isDirectory ? 'd' : '-';
      printableNodeAccess[1] = nodeAccess & FS_ACCESS_READ ? 'r' : '-';
      printableNodeAccess[2] = nodeAccess & FS_ACCESS_WRITE ? 'w' : '-';
      printableNodeAccess[3] = '\0';

      //Date and time
      const time_t standardTime = static_cast<time_t>(nodeTime);
      char printableNodeTime[24];

      strftime(printableNodeTime, 24, "%Y-%m-%d %H:%M:%S",
          gmtime(&standardTime));

      //Convert size to printable format
      const unsigned long printableNodeSize =
          static_cast<unsigned long>(nodeSize);

      if (showIndex)
      {
        const unsigned long printableClusterPart =
            static_cast<unsigned long>(nodeId >> 16);
        const unsigned long printableIndexPart =
            static_cast<unsigned long>(nodeId & 0xFFFF);

        owner.log("%8lX%04lX %s %10lu %s %s", printableClusterPart,
            printableIndexPart, printableNodeAccess, printableNodeSize,
            printableNodeTime, nodeName);
      }
      else
      {
        owner.log("%s %10lu %s %s", printableNodeAccess, printableNodeSize,
            printableNodeTime, nodeName);
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
    owner.log("ls: node numbers mismatch: got %u, expected %u",
        entries, verifyCount);
    return E_ENTRY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
Result MakeDirectory::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *target = nullptr;
  bool help = false;
  Result res;

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
    owner.log("mkdir: target is not specified");
    return E_VALUE;
  }

  const char * const nodeName = Shell::extractName(target);
  if (nodeName == nullptr)
  {
    owner.log("mkdir: %s: incorrect name", target);
    return E_VALUE;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);

  FsNode * const destinationNode = openNode(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("mkdir: %s: node already exists", context->pathBuffer);
    return E_EXIST;
  }

  FsNode * const root = openBaseNode(context->pathBuffer);
  if (root == nullptr)
  {
    owner.log("mkdir: %s: root node not found", context->pathBuffer);
    return E_ENTRY;
  }

  const FsFieldDescriptor descriptors[] = {
      //Name descriptor
      {
          nodeName,
          static_cast<length_t>(strlen(nodeName) + 1),
          FS_NODE_NAME
      }
  };

  res = fsNodeCreate(root, descriptors, ARRAY_SIZE(descriptors));
  fsNodeFree(root);
  if (res != E_OK)
  {
    owner.log("mkdir: %s: creation failed", context->pathBuffer);
    return res;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
Result RemoveDirectory::run(unsigned int count, const char * const *arguments,
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

  FsNode * const node = openNode(context->pathBuffer);
  if (node == nullptr)
  {
    owner.log("rmdir: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  //Check whether the node contains data or other entries
  if (fsNodeLength(node, FS_NODE_DATA, nullptr) == E_OK)
  {
    owner.log("rmdir: %s: node contains data", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }

  FsNode * const root = openBaseNode(context->pathBuffer);
  if (root == nullptr)
  {
    owner.log("rmdir: %s: root node not found", context->pathBuffer);
    fsNodeFree(node);
    return E_ENTRY;
  }

  const Result res = fsNodeRemove(root, node);

  fsNodeFree(root);
  fsNodeFree(node);

  if (res != E_OK)
  {
    owner.log("rmdir: deletion failed");
  }

  return res;
}
//------------------------------------------------------------------------------
Result RemoveEntry::processArguments(unsigned int count,
    const char * const *arguments, bool *recursive, const char **targets) const
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
    if (!strcmp(arguments[i], "-r"))
    {
      *recursive = true;
      continue;
    }
    targets[entries++] = arguments[i];
  }

  if (help)
  {
    owner.log("Usage: rm [OPTION]... ENTRY");
    owner.log("  --help  print help message");
//    owner.log("  -r      remove directories and their content");//TODO
    return E_BUSY;
  }

  return !entries ? E_ENTRY : E_OK;
}
//------------------------------------------------------------------------------
Result RemoveEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *targets[Shell::ARGUMENT_COUNT - 1] = {nullptr};
  bool recursive = false;
  Result res;

  if ((res = processArguments(count, arguments, &recursive, targets)) != E_OK)
    return res;

  for (unsigned int i = 0; i < Shell::ARGUMENT_COUNT - 1; ++i)
  {
    if (targets[i] == nullptr)
      break;

    Shell::joinPaths(context->pathBuffer, context->currentDir, targets[i]);

    FsNode * const node = openNode(context->pathBuffer);
    if (node == nullptr)
    {
      owner.log("rm: %s: node not found", context->pathBuffer);
      res = E_ENTRY;
      break;
    }

    if ((res = fsNodeLength(node, FS_NODE_DATA, nullptr)) != E_OK)
    {
      fsNodeFree(node);
      owner.log("rm: %s: data-less node ignored", context->pathBuffer);
      break;
    }

    FsNode * const root = openBaseNode(context->pathBuffer);
    if (root == nullptr)
    {
      fsNodeFree(node);
      owner.log("rm: %s: root node not found", context->pathBuffer);
      res = E_ENTRY;
      break;
    }

    if ((res = fsNodeRemove(root, node)) != E_OK)
    {
      owner.log("rm: %s: deletion failed", context->pathBuffer);
    }

    fsNodeFree(root);
    fsNodeFree(node);

    if (res != E_OK)
      break;
  }

  return res;
}
//------------------------------------------------------------------------------
Result Synchronize::run(unsigned int, const char * const *,
    Shell::ShellContext *)
{
  Result res = fsHandleSync(owner.handle());

  return res;
}
