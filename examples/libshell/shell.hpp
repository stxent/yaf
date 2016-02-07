/*
 * libshell/shell.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBSHELL_SHELL_HPP_
#define LIBSHELL_SHELL_HPP_
//------------------------------------------------------------------------------
#include <cstdarg>
#include <vector>

extern "C"
{
#include <fs.h>
#include <interface.h>
#include <libosw/mutex.h>
}
//------------------------------------------------------------------------------
class Shell;
//------------------------------------------------------------------------------
template<class T> class CommandBuilder
{
  friend class Shell;

public:
  CommandBuilder()
  {
  }

private:
  static T *create(Shell &shell)
  {
    return new T(shell);
  }
};
//------------------------------------------------------------------------------
class Shell
{
  friend class ShellCommand;

public:
  enum
  {
    E_SHELL_EXIT = E_RESULT_END
  };

  enum
  {
    ARGUMENT_COUNT = 16,
    ARGUMENT_LENGTH = 64
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
    virtual ~ShellCommand()
    {
    }

    /**
     * Get name of the command.
     * @return Pointer to a statically allocated string with a read-only access.
     */
    virtual const char *name() const = 0;

    /**
     * Run command with specified arguments.
     * @param count Argument count.
     * @param arguments Array of the arguments.
     * @param environment Pointer to a context object.
     * @return @b E_OK on success, @b E_ERROR on unrecoverable error.
     */
    virtual result run(unsigned int count, const char * const *arguments,
        ShellContext *environment) = 0;

  protected:
    ShellCommand(Shell &);

    //Helper functions
    FsNode *openBaseNode(const char *) const;
    FsNode *openNode(const char *) const;

    Shell &owner;

  private:
    static const char *getChunk(char *dst, const char *src);
    FsNode *followPath(const char *, bool) const;
    const char *followNextPart(FsNode **, const char *, bool) const;
  };

  Shell(struct Interface *console, struct FsHandle *root);
  ~Shell();

  result execute(const char *);
  void log(const char *, ...);

  static const char *extractName(const char *);
  static void joinPaths(char *, const char *, const char *);
  static bool stripName(char *);

  template<class T> void append(CommandBuilder<T> builder)
  {
    T *command = builder.create(*this);

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

  char logBuffer[LOG_LENGTH * 3 + 1];
  Mutex logMutex;
};
//------------------------------------------------------------------------------
#endif //LIBSHELL_SHELL_HPP_
