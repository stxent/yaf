/*
 * crypto.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstring>
#include "crypto.hpp"
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const char * const *ComputationCommand::getNextEntry(unsigned int count,
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
result ComputationCommand::processArguments(unsigned int count,
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
    owner.log("Usage: md5sum FILES");
    owner.log("  --help  print help message");
    return E_BUSY;
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result ComputationCommand::run(unsigned int count,
    const char * const *arguments)
{
  const char * const *path = arguments;
  const char *name;
  uint8_t buffer[BUFFER_LENGTH];
  result res;

  if ((res = processArguments(count, arguments)) != E_OK)
    return res;

  FsNode *node;
  FsEntry *entry;
  fsNodeType type;

  //TODO Return from function on some error types
  while ((path = getNextEntry(count
      - static_cast<unsigned int>(path - arguments), path)) != nullptr)
  {
    name = *path;
    ++path;

    //Find destination node
    Shell::joinPaths(context->pathBuffer, context->currentDir, name);
    node = reinterpret_cast<FsNode *>(fsFollow(owner.handle(),
        context->pathBuffer, nullptr));
    if (node == nullptr)
    {
      owner.log("md5sum: %s: no such file", context->pathBuffer);
      continue;
    }
    //Check node type
    res = fsGet(node, FS_NODE_TYPE, &type);
    if (res != E_OK || type != FS_TYPE_FILE)
    {
      fsFree(node);
      owner.log("md5sum: %s: wrong entry type", context->pathBuffer);
      continue;
    }

    entry = reinterpret_cast<FsEntry *>(fsOpen(node, FS_ACCESS_READ));
    fsFree(node);
    if (entry == nullptr)
    {
      owner.log("md5sum: %s: opening failed", context->pathBuffer);
      continue;
    }

    uint32_t chunk, read = 0;
    result res = E_OK;

    reset();
    while (!fsEnd(entry))
    {
      chunk = fsRead(entry, buffer, BUFFER_LENGTH);
      if (!chunk)
      {
        owner.log("md5sum: read error at %u", read);
        res = E_ERROR;
        break;
      }
      read += chunk;
      compute(buffer, chunk);
    }
    finalize(res == E_OK ? context->pathBuffer : nullptr);

    fsClose(entry);
  }

  return E_OK;
}
//------------------------------------------------------------------------------
result ComputeHash::isolate(Shell::ShellContext *environment,
    unsigned int count, const char * const *arguments)
{
  ComputeHash instance(owner);

  instance.link(environment);
  return instance.run(count, arguments);
}
//------------------------------------------------------------------------------
void ComputeHash::compute(const uint8_t *buffer, uint32_t length)
{
  MD5_Update(&context, buffer, length);
}
//------------------------------------------------------------------------------
void ComputeHash::finalize(const char *name)
{
  unsigned char result[16];

  MD5_Final(result, &context);

  if (name != nullptr)
  {
    char digest[33];
    auto hexify = [](unsigned char value) { return value < 10 ? '0' + value
        : 'a' + (value - 10); };

    for (unsigned int pos = 0; pos < 16; pos++)
    {
      digest[pos * 2 + 0] = hexify(result[pos] >> 4);
      digest[pos * 2 + 1] = hexify(result[pos] & 0x0F);
    }
    digest[32] = '\0';

    owner.log("%s\t%s", digest, name);
  }
}
//------------------------------------------------------------------------------
void ComputeHash::reset()
{
  MD5_Init(&context);
}
