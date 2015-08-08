/*
 * shell.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "libshell/shell.hpp"
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
Shell::ShellCommand::ShellCommand(Shell &shell) :
    owner(shell)
{

}
//------------------------------------------------------------------------------
FsNode *Shell::ShellCommand::followPath(const char *path) const
{
  FsNode *node = nullptr;

  while (path && *path)
    path = followNextPart(&node, path);

  return path != nullptr ? node : nullptr;
}
//------------------------------------------------------------------------------
const char *Shell::ShellCommand::followNextPart(FsNode **node,
    const char *path) const
{
  char currentName[CONFIG_FILENAME_LENGTH];
  char nextPart[CONFIG_FILENAME_LENGTH];

  path = getChunk(nextPart, path);

  if (!strlen(nextPart))
  {
    path = nullptr;
  }
  else if (!strcmp(nextPart, ".") || !strcmp(nextPart, ".."))
  {
    //Path contains forbidden directories
    path = nullptr;
  }
  else
  {
    if (*node == nullptr)
    {
      if (nextPart[0] == '/')
      {
        *node = reinterpret_cast<FsNode *>(fsHandleRoot(owner.handle()));

        if (*node == nullptr)
          return nullptr;
      }
      else
        return nullptr;
    }
    else
    {
      FsNode *child = reinterpret_cast<FsNode *>(fsNodeHead(*node));
      enum result res;

      fsNodeFree(*node);
      if (child == nullptr)
        return nullptr;

      do
      {
        res = fsNodeRead(child, FS_NODE_NAME, 0, currentName,
            sizeof(currentName), nullptr);

        if (res != E_OK)
        {
          fsNodeFree(child);
          return nullptr;
        }

        if (!strcmp(nextPart, currentName))
          break;
      }
      while ((res = fsNodeNext(child)) == E_OK);

      //Check whether the node is found
      if (res != E_OK)
      {
        fsNodeFree(child);
        path = nullptr;
      }
      else
      {
        *node = child;
      }
    }
  }

  return path;
}
//------------------------------------------------------------------------------
//Output buffer length should be greater or equal to maximum name length
const char *Shell::ShellCommand::getChunk(char *dst, const char *src)
{
  uint16_t counter = 0;

  if (!*src)
    return src;

  if (*src == '/')
  {
    *dst++ = '/';
    *dst = '\0';
    return src + 1;
  }

  while (*src && counter++ < CONFIG_FILENAME_LENGTH - 1)
  {
    if (*src == '/')
    {
      ++src;
      break;
    }
    *dst++ = *src++;
  }
  *dst = '\0';

  return src;
}
//------------------------------------------------------------------------------
const char *Shell::extractName(const char *path)
{
  int length, pos;

  for (length = 0, pos = strlen(path) - 1; pos >= 0; --pos, ++length)
  {
    if (path[pos] == '/')
      return length ? path + pos + 1 : nullptr;
  }

  return length ? path : nullptr;
}
//------------------------------------------------------------------------------
void Shell::joinPaths(char *buffer, const char *directory, const char *path)
{
  if (!path || !strlen(path))
  {
    strcpy(buffer, directory);
  }
  else if (path[0] != '/')
  {
    unsigned int directoryLength = strlen(directory);

    strcpy(buffer, directory);
    if (directoryLength > 1 && directory[directoryLength - 1] != '/')
      strcat(buffer, "/");
    strcat(buffer, path);
  }
  else
    strcpy(buffer, path);
}
//------------------------------------------------------------------------------
Shell::Shell(Interface *console, FsHandle *root) :
    rootHandle(root), consoleInterface(console)
{
  strcpy(context.currentDir, "/");
  strcpy(context.pathBuffer, "");

  for (unsigned int pos = 0; pos < ARGUMENT_COUNT; ++pos)
    argumentPool[pos] = new char[ARGUMENT_LENGTH];

  const result res = mutexInit(&logMutex);

  if (res != E_OK)
    exit(EXIT_FAILURE);

  //Log function should be called after mutex initialization
  log("Shell opened, size %u", static_cast<unsigned int>(sizeof(Shell)));
}
//------------------------------------------------------------------------------
Shell::~Shell()
{
  for (auto entry : registeredCommands)
    delete entry;

  for (unsigned int pos = 0; pos < ARGUMENT_COUNT; ++pos)
    delete[] argumentPool[pos];

  mutexDeinit(&logMutex);
}
//------------------------------------------------------------------------------
result Shell::execute(const char *input)
{
  unsigned int line = 0, start = 0;
  bool braces = false, spaces = true;

  for (unsigned int pos = 0, length = strlen(input); pos < length; ++pos)
  {
    if (!braces && (input[pos] == ' ' || input[pos] == '\t'))
    {
      if (!spaces)
      {
        spaces = true;
        memcpy(argumentPool[line], input + start, pos - start);
        *(argumentPool[line] + pos - start) = '\0';
        ++line;
        continue;
      }
      continue;
    }

    if (!spaces && pos - start >= ARGUMENT_LENGTH)
    {
      log("Parser error: argument length is greater than allowed");
      return E_VALUE;
    }

    if (line >= ARGUMENT_COUNT)
    {
      log("Parser error: too many arguments");
      return E_VALUE;
    }

    if (input[pos] == '"')
    {
      if (!braces)
      {
        braces = true;
        spaces = false;
        start = pos + 1;
        continue;
      }
      else
      {
        braces = false;
        spaces = true;
        memcpy(argumentPool[line], input + start, pos - start);
        *(argumentPool[line] + pos - start) = '\0';
        ++line;
        continue;
      }
    }

    if (spaces)
    {
      spaces = false;
      start = pos;
    }

    if (pos == length - 1)
    {
      memcpy(argumentPool[line], input + start, length - start);
      *(argumentPool[line] + length - start) = '\0';
      ++line;
    }
  }

  if (line)
  {
    for (auto entry : registeredCommands)
    {
      if (!strcmp(entry->name(), argumentPool[0]))
      {
        result res = entry->run(line - 1, argumentPool + 1, &context);
        return res;
      }
    }
  }

  return E_OK;
}
//------------------------------------------------------------------------------
void Shell::log(const char *format, ...)
{
  int length;
  va_list arguments;

  mutexLock(&logMutex);
  va_start(arguments, format);
  //Version with sized buffer does not work on some platforms
  length = vsprintf(logBuffer, format, arguments);
  va_end(arguments);

  assert(length >= 0 && length < static_cast<int>(sizeof(logBuffer)) - 1);

  if (length)
  {
    memcpy(logBuffer + length, "\n", 2);
    ifWrite(consoleInterface, reinterpret_cast<const uint8_t *>(logBuffer),
        length + 2);
  }
  mutexUnlock(&logMutex);
}
