/*
 * shell.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef SHELL_HPP_
#define SHELL_HPP_
//------------------------------------------------------------------------------
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <vector>
//------------------------------------------------------------------------------
extern "C" {
#include <fs.h>
#include <mutex.h>
}
//------------------------------------------------------------------------------
class Shell
{
public:
  class ShellCommand
  {
  public:
    virtual ~ShellCommand() {
      delete[] name;
    }

    virtual result run(unsigned int count, char *arguments[]) const = 0;

    char *name;
    Shell *owner;

  protected:
    ShellCommand(Shell *shell, const char *alias) : owner(shell) {
      name = new char[strlen(alias) + 1];
      strcpy(name, alias);
    }
  };

  enum
  {
    argumentCount = 16,
    argumentLength = 32,
    nameLength = 256,
    width = 80
  };

  Shell(struct Interface *console, struct FsHandle *root);
  ~Shell();

  result execute(const char *input);
  void log(const char *format, ...);
  const char *path();

  static char *extractName(char *path);
  static void joinPaths(char *buffer, const char *directory, const char *path);
  static uint64_t timestamp();

  FsHandle *rootHandle;
  char currentDir[nameLength], pathBuffer[nameLength];
  std::vector<const ShellCommand *> commands;

protected:
  Interface *consoleInterface;

  char *argumentPool[argumentCount];
  char logBuffer[width * 2 + 1];
  Mutex lock;
};
//------------------------------------------------------------------------------
template <class A> class CommandLinker
{
public:
  static void attach(Shell *shell) {
    shell->commands.push_back(new A(shell));
  }

private:
  CommandLinker() {}
};
//------------------------------------------------------------------------------
#endif //SHELL_HPP_