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

  do
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
    FsNode * const node = openEntry(context->pathBuffer);
    if (node == nullptr)
    {
      owner.log("%s: %s: no such node", name(), context->pathBuffer);
      break;
    }

    res = processEntry(node, context, algorithm,
        checkValue ? expectedValue : nullptr);

    fsNodeFree(node);
  }
  while (res == E_OK);

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
result AbstractComputationCommand::processEntry(FsNode *node,
    Shell::ShellContext *context, ComputationAlgorithm *algorithm,
    const char *expectedValue) const
{
  length_t read = 0;
  length_t size;
  length_t chunk;
  uint8_t buffer[CONFIG_SHELL_BUFFER];
  char computedValue[MAX_LENGTH];
  result res = E_OK;

  algorithm->reset();

  res = fsNodeLength(node, FS_NODE_DATA, &size);
  if (res != E_OK)
    return res;

  while (read < size)
  {
    res = fsNodeRead(node, FS_NODE_DATA, read, buffer, sizeof(buffer), &chunk);

    if (res != E_OK)
    {
      owner.log("%s: %s: read error at %u", name(), context->pathBuffer, read);
      res = E_INTERFACE;
      break;
    }

    if (!chunk)
    {
      owner.log("%s: %s: invalid chunk length at %u", name(),
          context->pathBuffer, read);
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
