/*
 * file_tools.cpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <bits.h>
#include "libshell/file_tools.hpp"

extern "C"
{
#include <crc/crc32.h>
}
//------------------------------------------------------------------------------
#ifndef CONFIG_SHELL_BUFFER
#define CONFIG_SHELL_BUFFER 512
#endif
//------------------------------------------------------------------------------
ChecksumCrc32::ChecksumCrc32() :
    sum(INITIAL_CRC)
{
  engine = reinterpret_cast<CrcEngine *>(init(Crc32, nullptr));
  assert(engine != nullptr);
}
//------------------------------------------------------------------------------
ChecksumCrc32::~ChecksumCrc32()
{
  deinit(engine);
}
//------------------------------------------------------------------------------
void ChecksumCrc32::finalize(char *digest)
{
  sprintf(digest, "%08x", sum);
}
//------------------------------------------------------------------------------
void ChecksumCrc32::reset()
{
  sum = INITIAL_CRC;
}
//------------------------------------------------------------------------------
void ChecksumCrc32::update(const uint8_t *buffer, uint32_t bufferLength)
{
  sum = crcUpdate(engine, sum, buffer, bufferLength);
}
//------------------------------------------------------------------------------
result FillEntry::fill(FsNode *node, const char *pattern,
    unsigned int number) const
{
  const unsigned int patternLength = strlen(pattern);
  unsigned int iteration = 0;
  length_t nodePosition = 0;
  char buffer[CONFIG_SHELL_BUFFER];
  result res = E_OK;

  //Write file content
  while (iteration < number)
  {
    length_t length = 0;
    length_t written;

    for (; iteration < number && (length + patternLength) < sizeof(buffer);
        ++iteration)
    {
      memcpy(buffer + length, pattern, patternLength);
      length += patternLength;
    }

    res = fsNodeWrite(node, FS_NODE_DATA, nodePosition, buffer, length,
        &written);
    if (res != E_OK || written != length)
    {
      owner.log("%s: write error at %u", name(), nodePosition);
      if (res == E_OK)
        res = E_ERROR;
      break;
    }
    nodePosition += written;
  }

  return res;
}
//------------------------------------------------------------------------------
result FillEntry::processArguments(unsigned int count,
    const char * const *arguments, const char **target, const char **pattern,
    unsigned int *number) const
{
  bool argumentError = false;
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "-n"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      *number = atoi(arguments[i]);
      continue;
    }
    if (!strcmp(arguments[i], "-p"))
    {
      if (++i >= count)
      {
        argumentError = true;
        break;
      }
      *pattern = arguments[i];
      continue;
    }
    if (*target == nullptr)
    {
      *target = arguments[i];
    }
  }

  if (argumentError)
  {
    owner.log("fill: argument processing error");
    return E_VALUE;
  }

  if (help)
  {
    owner.log("Usage: fill ENTRY");
    owner.log("  --help  print help message");
    owner.log("  -n      number of iterations");
    owner.log("  -p      string pattern");
    return E_BUSY;
  }

  return *target == nullptr ? E_ENTRY : E_OK;
}
//------------------------------------------------------------------------------
result FillEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *pattern = "0";
  const char *target = nullptr;
  unsigned int number = CONFIG_SHELL_BUFFER;
  result res;

  res = processArguments(count, arguments, &target, &pattern, &number);
  if (res != E_OK)
    return res;

  const char * const entryName = Shell::extractName(target);
  if (entryName == nullptr)
  {
    owner.log("fill: %s: incorrect name", target);
    return E_VALUE;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);

  FsNode *destinationNode = openEntry(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("fill: %s: node already exists", context->pathBuffer);
    return E_EXIST;
  }

  FsNode * const root = openRoot(context->pathBuffer);
  if (root == nullptr)
  {
    owner.log("fill: %s: target directory not found", context->pathBuffer);
    return E_ENTRY;
  }

  const FsFieldDescriptor descriptors[] = {
      //Name descriptor
      {
          entryName,
          static_cast<length_t>(strlen(entryName) + 1),
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
    owner.log("fill: %s: creation failed", context->pathBuffer);
    return res;
  }

  destinationNode = openEntry(context->pathBuffer);
  if (destinationNode == nullptr)
  {
    owner.log("fill: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  res = fill(destinationNode, pattern, number);
  fsNodeFree(destinationNode);

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

    const char * const entryName = Shell::extractName(targets[i]);
    if (entryName == nullptr)
    {
      owner.log("touch: %s: incorrect name", targets[i]);
      res = E_VALUE;
      break;
    }

    Shell::joinPaths(context->pathBuffer, context->currentDir, targets[i]);

    FsNode * const destinationNode = openEntry(context->pathBuffer);
    if (destinationNode != nullptr)
    {
      fsNodeFree(destinationNode);
      owner.log("touch: %s: node already exists", context->pathBuffer);
      res = E_EXIST;
      break;
    }

    FsNode * const root = openRoot(context->pathBuffer);
    if (root == nullptr)
    {
      owner.log("touch: %s: target root node not found", context->pathBuffer);
      res = E_ENTRY;
      break;
    }

    const FsFieldDescriptor descriptors[] = {
        //Name descriptor
        {
            entryName,
            static_cast<length_t>(strlen(entryName) + 1),
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
      owner.log("touch: %s: creation failed", context->pathBuffer);
      break;
    }
  }

  return res;
}
