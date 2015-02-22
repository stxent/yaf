/*
 * crypto.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstring>
#include "libshell/crypto.hpp"
//------------------------------------------------------------------------------
#ifndef CONFIG_SHELL_BUFFER
#define CONFIG_SHELL_BUFFER 512
#endif
//------------------------------------------------------------------------------
const char * const *AbstractComputationCommand::getNextEntry(unsigned int count,
    const char * const *arguments, bool *check, char *expectedValue) const
{
  *check = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--check"))
    {
      if (++i >= count)
      {
        owner.log("%s: argument processing error", name());
        return nullptr;
      }
      if (strlen(arguments[i]) > MAX_LENGTH)
      {
        owner.log("%s: wrong argument length", name());
        return nullptr;
      }
      *check = true;
      strcpy(expectedValue, arguments[i]);
      continue;
    }

    //Skip other options
    if (*arguments[i] == '-')
      continue;

    return arguments + i;
  }

  return nullptr;
}
//------------------------------------------------------------------------------
result AbstractComputationCommand::compute(unsigned int count,
    const char * const *arguments, Shell::ShellContext *context,
    ComputationAlgorithm *algorithm) const
{
  const char * const *path = arguments;
  result res;

  if ((res = processArguments(count, arguments)) != E_OK)
    return res;

  while (1)
  {
    const unsigned int index = count
        - static_cast<unsigned int>(path - arguments);
    char expectedValue[MAX_LENGTH];
    bool checkValue;

    if ((path = getNextEntry(index, path, &checkValue,
        expectedValue)) == nullptr)
    {
      break;
    }

    const char * const fileName = *path++;

    //Find destination node
    Shell::joinPaths(context->pathBuffer, context->currentDir, fileName);

    FsNode * const node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
        context->pathBuffer, nullptr));

    if (node == nullptr)
    {
      owner.log("%s: %s: no such file", name(), context->pathBuffer);
      break;
    }

    //Check node type
    fsNodeType type;

    res = fsGet(node, FS_NODE_TYPE, &type);
    if (res != E_OK)
    {
      owner.log("%s: %s: metadata reading failed", name(), context->pathBuffer);
      fsFree(node);
      break;
    }
    if (type != FS_TYPE_FILE)
    {
      res = E_ENTRY;
      owner.log("%s: %s: wrong entry type", name(), context->pathBuffer);
      fsFree(node);
      break;
    }

    FsEntry * const entry = reinterpret_cast<FsEntry *>(fsOpen(node,
        FS_ACCESS_READ));

    fsFree(node);
    if (entry == nullptr)
    {
      owner.log("%s: %s: opening failed", name(), context->pathBuffer);
      break;
    }

    res = processEntry(entry, context, algorithm,
        checkValue ? expectedValue : nullptr);

    fsClose(entry);

    if (res != E_OK)
      break;
  }

  return res;
}
//------------------------------------------------------------------------------
result AbstractComputationCommand::processArguments(unsigned int count,
    const char * const *arguments) const
{
  bool help = false;

  for (unsigned int i = 0; i < count; ++i)
  {
    if (!strcmp(arguments[i], "--help"))
    {
      help = true;
      continue;
    }
  }

  if (help)
  {
    owner.log("Usage: %s [OPTION]... FILES", name());
    owner.log("  --check VALUE  check computation result with VALUE");
    owner.log("  --help         print help message");
    return E_BUSY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result AbstractComputationCommand::processEntry(FsEntry *entry,
    Shell::ShellContext *context, ComputationAlgorithm *algorithm,
    const char *expectedValue) const
{
  uint32_t read = 0;
  uint8_t buffer[CONFIG_SHELL_BUFFER];
  char computedValue[MAX_LENGTH];
  result res = E_OK;

  algorithm->reset();

  while (!fsEnd(entry))
  {
    const uint32_t chunk = fsRead(entry, buffer, sizeof(buffer));

    if (!chunk)
    {
      owner.log("%s: %s: read error at %u", name(), context->pathBuffer, read);
      res = E_ERROR;
      break;
    }
    read += chunk;
    algorithm->update(buffer, chunk);
  }

  algorithm->finalize(computedValue);

  if (res == E_OK)
  {
    if (expectedValue != nullptr && strcmp(computedValue, expectedValue) != 0)
    {
      owner.log("%s: %s: comparison error, expected %s", name(),
          context->pathBuffer, expectedValue);
      res = E_VALUE;
    }
    owner.log("%s\t%s", computedValue, context->pathBuffer);
  }

  return res;
}
