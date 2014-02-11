/*
 * owner.cpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

/* TODO Fix defines */
#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <cstdio> //FIXME
#include <cstdlib>
#include <ctime>
#include "shell.hpp"
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
char *Shell::extractName(char *path)
{
  int length, pos;

  for (length = 0, pos = strlen(path) - 1; pos >= 0; --pos, ++length)
  {
    if (path[pos] == '/')
      return length ? path + pos + 1 : 0;
  }

  return length ? path : 0;
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
    strcpy(buffer, directory);
    if (strlen(directory) > 1 && directory[strlen(directory) - 1] != '/')
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
  strcpy(currentDir, "/");
  for (unsigned int pos = 0; pos < argumentCount; ++pos)
    argumentPool[pos] = new char[argumentLength];

  mutexInit(&lock);

  log("Shell opened, size %u", (unsigned int)sizeof(Shell));
}
//------------------------------------------------------------------------------
Shell::~Shell()
{
  for (auto iter = commands.begin(); iter != commands.end(); ++iter)
    delete *iter;
  for (unsigned int pos = 0; pos < argumentCount; ++pos)
    delete[] argumentPool[pos];
  mutexDeinit(&lock);

  log("Shell closed");
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

    if (!spaces && pos - start >= argumentLength)
    {
      log("Parser error: argument length is greater than allowed");
      return E_VALUE;
    }

    if (line >= argumentCount)
    {
      log("Parser error: too many arguments");
      return E_VALUE;
    }

    if (input[pos] == '"')
    {
      if (!braces)
      {
        braces = true;
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
    for (auto iter = commands.begin(); iter != commands.end(); ++iter)
    {
      if (!strcmp((*iter)->name, argumentPool[0]))
      {
        mutexLock(&lock);
        result res = (*iter)->run(line - 1, argumentPool + 1);
        mutexUnlock(&lock);
        return res;
      }
    }
  }

  return E_OK;
}
//------------------------------------------------------------------------------
void Shell::log(const char *format, ...)
{
  va_list arguments;

  va_start(arguments, format);
  vsprintf(logBuffer, format, arguments);
  va_end(arguments);

  strcat(logBuffer, "\n");
  printf(logBuffer);
}
//------------------------------------------------------------------------------
const char *Shell::path()
{
  return currentDir;
}
//------------------------------------------------------------------------------
uint64_t Shell::timestamp()
{
  struct timespec rawtime;
  uint64_t result;

  clock_gettime(CLOCK_REALTIME, &rawtime);
  result = rawtime.tv_sec * 1e6 + rawtime.tv_nsec / 1e3;

  return result;
}
