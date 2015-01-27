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
result AbstractComputationCommand::compute(unsigned int count,
    const char * const *arguments, Shell::ShellContext *context,
    ComputationAlgorithm *algorithm) const
{
  const char * const *path = arguments;
  unsigned int position;
  uint8_t buffer[CONFIG_SHELL_BUFFER];
  result res;

  if ((res = processArguments(count, arguments)) != E_OK)
    return res;

  FsNode *node;
  FsEntry *entry;
  fsNodeType type;

  while (1)
  {
    position = count - static_cast<unsigned int>(path - arguments);
    if ((path = getNextEntry(position, path)) == nullptr)
      break;

    const char *fileName = *path++;

    //Find destination node
    Shell::joinPaths(context->pathBuffer, context->currentDir, fileName);
    node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
        context->pathBuffer, nullptr));
    if (node == nullptr)
    {
      owner.log("%s: %s: no such file", name(), context->pathBuffer);
      continue;
    }
    //Check node type
    res = fsGet(node, FS_NODE_TYPE, &type);
    if (res != E_OK || type != FS_TYPE_FILE)
    {
      fsFree(node);
      owner.log("%s: %s: wrong entry type", name(), context->pathBuffer);
      continue;
    }

    entry = reinterpret_cast<FsEntry *>(fsOpen(node, FS_ACCESS_READ));
    fsFree(node);
    if (entry == nullptr)
    {
      owner.log("%s: %s: opening failed", name(), context->pathBuffer);
      continue;
    }

    uint32_t chunk, read = 0;
    res = E_OK;

    algorithm->reset();
    while (!fsEnd(entry))
    {
      chunk = fsRead(entry, buffer, CONFIG_SHELL_BUFFER);
      if (!chunk)
      {
        owner.log("%s: read error at %u", name(), read);
        res = E_ERROR;
        break;
      }
      read += chunk;
      algorithm->update(buffer, chunk);
    }
    algorithm->finalize(reinterpret_cast<char *>(buffer), CONFIG_SHELL_BUFFER);
    if (res == E_OK)
      owner.log("%s\t%s", buffer, context->pathBuffer);

    fsClose(entry);
  }

  return E_OK;
}
