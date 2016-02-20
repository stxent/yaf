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
result CatEntry::print(FsNode *node, bool hex, bool quiet) const
{
  //TODO Style for lambdas
  auto hexify = [](unsigned char value) {
    return value < 10 ? '0' + value : 'A' + (value - 10);
  };

  length_t length;
  result res;

  if ((res = fsNodeLength(node, FS_NODE_DATA, &length)) != E_OK)
  {
    owner.log("cat: error reading data length");
    return res;
  }

  uint8_t buffer[CONFIG_SHELL_BUFFER];
  length_t position = 0;

  //Read file content
  while (position < length)
  {
    const length_t chunkLength = length - position > sizeof(buffer) ?
        sizeof(buffer) : length - position;
    length_t read;

    res = fsNodeRead(node, FS_NODE_DATA, position, buffer, chunkLength,
        &read);
    if (res != E_OK || read != chunkLength)
    {
      owner.log("cat: read error at %u", position);
      if (res == E_OK)
        res = E_ERROR;
      break;
    }
    position += read;

    if (quiet)
      continue;

    if (hex)
    {
      const unsigned int rowNumber =
          (read + HEX_OUTPUT_WIDTH - 1) / HEX_OUTPUT_WIDTH;

      for (unsigned int row = 0; row < rowNumber; ++row)
      {
        const unsigned int offset = row * HEX_OUTPUT_WIDTH;
        const unsigned int currentRowWidth = read - offset > HEX_OUTPUT_WIDTH ?
            HEX_OUTPUT_WIDTH : static_cast<unsigned int>(read - offset);
        char output[HEX_OUTPUT_WIDTH * 3];
        char *outputPosition = output;

        for (unsigned int i = 0; i < currentRowWidth; ++i)
        {
          *outputPosition++ = hexify(buffer[offset + i] >> 4);
          *outputPosition++ = hexify(buffer[offset + i] & 0x0F);
          *outputPosition++ = i == currentRowWidth - 1 ? '\0' : ' ';
        }
        owner.log(output);
      }
    }
    else
    {
      const unsigned int rowNumber =
          (read + RAW_OUTPUT_WIDTH - 1) / RAW_OUTPUT_WIDTH;

      for (unsigned int row = 0; row < rowNumber; ++row)
      {
        const unsigned int offset = row * RAW_OUTPUT_WIDTH;
        const unsigned int currentRowWidth = read - offset > RAW_OUTPUT_WIDTH ?
            RAW_OUTPUT_WIDTH : static_cast<unsigned int>(read - offset);
        char output[RAW_OUTPUT_WIDTH + 1];

        memcpy(output, buffer + offset, currentRowWidth);
        output[currentRowWidth] = '\0';
        owner.log(output);
      }
    }
  }

  return res;
}
//------------------------------------------------------------------------------
result CatEntry::processArguments(unsigned int count,
    const char * const *arguments, const char **target, bool *hex, bool *quiet,
    const char **output) const
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
    if (!strcmp(arguments[i], "--hex"))
    {
      *hex = true;
      continue;
    }
    if (!strcmp(arguments[i], "-o"))
    {
      if (++i >= count)
      {
        *output = arguments[i];
        break;
      }
      continue;
    }
    if (!strcmp(arguments[i], "-q"))
    {
      *quiet = true;
      continue;
    }
    if (*target == nullptr)
    {
      *target = arguments[i];
    }
  }

  if (help)
  {
    owner.log("Usage: cat ENTRY...");
    owner.log("  --help  print help message");
    owner.log("  --hex   print in hexadecimal format");
    owner.log("  -o      output file");
    owner.log("  -q      quiet mode");
    return E_BUSY;
  }

  return *target == nullptr ? E_ENTRY : E_OK;
}
//------------------------------------------------------------------------------
result CatEntry::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *output = nullptr;
  const char *target = nullptr;
  bool hex = false;
  bool quiet = false;
  result res;

  res = processArguments(count, arguments, &target, &hex, &quiet, &output);
  if (res != E_OK)
    return res;

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);
  FsNode * const node = openNode(context->pathBuffer);
  if (node == nullptr)
  {
    owner.log("cat: %s: node not found", context->pathBuffer);
    return E_ENTRY;
  }

  res = print(node, hex, quiet);
  fsNodeFree(node);

  return res;
}
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
  sprintf(digest, "%08x", static_cast<unsigned int>(sum));
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
result EchoData::fill(FsNode *node, const char *pattern,
    unsigned int number) const
{
  const unsigned int patternLength = strlen(pattern);
  char buffer[CONFIG_SHELL_BUFFER];
  unsigned int iteration = 0;
  length_t nodePosition = 0;
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
      owner.log("echo: write error at %u", nodePosition);
      if (res == E_OK)
        res = E_ERROR;
      break;
    }
    nodePosition += written;
  }

  return res;
}
//------------------------------------------------------------------------------
result EchoData::processArguments(unsigned int count,
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
    owner.log("echo: argument processing error");
    return E_VALUE;
  }

  if (help)
  {
    owner.log("Usage: echo [OPTION]... ENTRY");
    owner.log("  --help  print help message");
    owner.log("  -n      number of iterations");
    owner.log("  -p      string pattern");
    return E_BUSY;
  }

  return *target == nullptr ? E_ENTRY : E_OK;
}
//------------------------------------------------------------------------------
result EchoData::run(unsigned int count, const char * const *arguments,
    Shell::ShellContext *context)
{
  const char *pattern = "0";
  const char *target = nullptr;
  unsigned int number = CONFIG_SHELL_BUFFER;
  result res;

  res = processArguments(count, arguments, &target, &pattern, &number);
  if (res != E_OK)
    return res;

  const char * const nodeName = Shell::extractName(target);
  if (nodeName == nullptr)
  {
    owner.log("echo: %s: incorrect name", target);
    return E_VALUE;
  }

  Shell::joinPaths(context->pathBuffer, context->currentDir, target);

  FsNode *destinationNode = openNode(context->pathBuffer);
  if (destinationNode != nullptr)
  {
    fsNodeFree(destinationNode);
    owner.log("echo: %s: node already exists", context->pathBuffer);
    return E_EXIST;
  }

  FsNode * const root = openBaseNode(context->pathBuffer);
  if (root == nullptr)
  {
    owner.log("echo: %s: root node not found", context->pathBuffer);
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
    owner.log("echo: %s: creation failed", context->pathBuffer);
    return res;
  }

  destinationNode = openNode(context->pathBuffer);
  if (destinationNode == nullptr)
  {
    owner.log("echo: %s: node not found", context->pathBuffer);
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
    owner.log("Usage: touch ENTRY...");
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

    const char * const nodeName = Shell::extractName(targets[i]);
    if (nodeName == nullptr)
    {
      owner.log("touch: %s: incorrect name", targets[i]);
      res = E_VALUE;
      break;
    }

    Shell::joinPaths(context->pathBuffer, context->currentDir, targets[i]);

    FsNode * const destinationNode = openNode(context->pathBuffer);
    if (destinationNode != nullptr)
    {
      fsNodeFree(destinationNode);
      owner.log("touch: %s: node already exists", context->pathBuffer);
      res = E_EXIST;
      break;
    }

    FsNode * const root = openBaseNode(context->pathBuffer);
    if (root == nullptr)
    {
      owner.log("touch: %s: root node not found", context->pathBuffer);
      res = E_ENTRY;
      break;
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
      owner.log("touch: %s: creation failed", context->pathBuffer);
      break;
    }
  }

  return res;
}
