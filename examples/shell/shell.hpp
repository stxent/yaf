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
class Shell;
//------------------------------------------------------------------------------
template<class A> class CommandBuilder
{
  friend class Shell;

public:
  CommandBuilder() {}

private:
  static const A *create(Shell &shell) {
    return new A(shell);
  }
};
//------------------------------------------------------------------------------
class Shell
{
  friend class ShellCommand;

public:
  enum
  {
    argumentCount = 16,
    argumentLength = 32,
    width = 80
  };

  struct ShellContext
  {
    enum
    {
      nameLength = 256
    };

    char currentDir[nameLength];
    char pathBuffer[nameLength];
  };

  class ShellCommand
  {
  public:
    virtual ~ShellCommand();
    virtual result run(unsigned int count, char *arguments[]) const = 0;

    const char *name() const {
      return buffer;
    }

  protected:
    ShellCommand(const char *, Shell &);

    ShellContext &context;
    Shell &owner;
    char *buffer;
  };

  Shell(struct Interface *console, struct FsHandle *root);
  ~Shell();

  result execute(const char *);
  void log(const char *, ...);

  static char *extractName(char *);
  static void joinPaths(char *, const char *, const char *);
  static uint64_t timestamp();

  template<class A> void append(CommandBuilder<A> builder)
  {
    registeredCommands.push_back(builder.create(*this));
  }

  const std::vector<const ShellCommand *> &commands() const {
    return registeredCommands;
  }

  FsHandle *handle() const {
    return rootHandle;
  }

  const char *path() const
  {
    return context.currentDir;
  }

private:
  FsHandle *rootHandle;
  Interface *consoleInterface;

  std::vector<const ShellCommand *> registeredCommands;

  ShellContext context;
  char *argumentPool[argumentCount];
  char logBuffer[width * 2 + 1];
  Mutex lock;
};
//------------------------------------------------------------------------------
#endif //SHELL_HPP_
