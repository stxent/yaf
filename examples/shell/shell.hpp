/*
 * shell.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef SHELL_HPP_
#define SHELL_HPP_
//------------------------------------------------------------------------------
#include <cstdarg>
#include <vector>
//------------------------------------------------------------------------------
extern "C"
{
#include <fs.h>
#include <os/mutex.h>
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
  static A *create(Shell &shell)
  {
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
    ARGUMENT_COUNT = 16,
    ARGUMENT_LENGTH = 32
  };

  enum
  {
    LOG_LENGTH = 80
  };

  struct ShellContext
  {
    enum
    {
      NAME_LENGTH = 256
    };

    char currentDir[NAME_LENGTH];
    char pathBuffer[NAME_LENGTH];
  };

  class ShellCommand
  {
  public:
    virtual ~ShellCommand() {}
    virtual const char *name() const = 0;
    virtual result run(unsigned int count, const char * const *arguments) = 0;

    void link(ShellContext *commandContext)
    {
      context = commandContext;
    }

  protected:
    ShellCommand(Shell &);
    ShellCommand(const ShellCommand &other);

    Shell &owner;
    ShellContext *context;
  };

  Shell(struct Interface *console, struct FsHandle *root);
  ~Shell();

  result execute(const char *);
  void log(const char *, ...);

  static const char *extractName(const char *);
  static void joinPaths(char *, const char *, const char *);
  static uint64_t timestamp();

  template<class A> void append(CommandBuilder<A> builder)
  {
    A *command = builder.create(*this);

    command->link(&context);
    registeredCommands.push_back(command);
  }

  const std::vector<ShellCommand *> &commands() const
  {
    return registeredCommands;
  }

  FsHandle *handle() const
  {
    return rootHandle;
  }

  const char *path() const
  {
    return context.currentDir;
  }

private:
  FsHandle *rootHandle;
  Interface *consoleInterface;

  std::vector<ShellCommand *> registeredCommands;

  ShellContext context;
  char *argumentPool[ARGUMENT_COUNT];

  char logBuffer[LOG_LENGTH * 2 + 1];
  Mutex logMutex;
};
//------------------------------------------------------------------------------
#endif //SHELL_HPP_
