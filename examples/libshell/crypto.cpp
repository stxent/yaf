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
    const char * const *arguments) const
{
  for (unsigned int i = 0; i < count; ++i)
  {
    //Skip options
    if (!strncmp(arguments[i], "--", 2))
    {
      ++i;
      continue;
    }
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
  unsigned int index;
  result res;

  if ((res = processArguments(count, arguments)) != E_OK)
    return res;

  while (1)
  {
    index = count - static_cast<unsigned int>(path - arguments);
    if ((path = getNextEntry(index, path)) == nullptr)
      break;

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

    res = processEntry(entry, context, algorithm);
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
    owner.log("Usage: %s FILES", name());
    owner.log("  --help  print help message");
    return E_BUSY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result AbstractComputationCommand::processEntry(FsEntry *entry,
    Shell::ShellContext *context, ComputationAlgorithm *algorithm) const
{
  uint32_t read = 0;
  uint8_t buffer[CONFIG_SHELL_BUFFER];
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

  algorithm->finalize(reinterpret_cast<char *>(buffer), sizeof(buffer));
  fsClose(entry);

  if (res == E_OK)
    owner.log("%s\t%s", buffer, context->pathBuffer);

  return res;
}
