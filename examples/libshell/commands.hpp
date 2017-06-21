/*
 * libshell/commands.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_COMMANDS_HPP_
#define YAF_LIBSHELL_COMMANDS_HPP_
//------------------------------------------------------------------------------
#include "libshell/shell.hpp"
//------------------------------------------------------------------------------
class DataProcessing : public Shell::ShellCommand
{
public:
  DataProcessing(Shell &parent) :
      ShellCommand(parent)
  {
  }

protected:
  Result copyContent(FsNode *, FsNode *, unsigned int, unsigned int,
      unsigned int, unsigned int) const;
  Result prepareNodes(Shell::ShellContext *, FsNode **, FsNode **, const char *,
      const char *, bool);
  Result removeNode(Shell::ShellContext *, FsNode *, char *);
};
//------------------------------------------------------------------------------
class ChangeDirectory : public Shell::ShellCommand
{
public:
  ChangeDirectory(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "cd";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  Result processArguments(unsigned int, const char * const *,
      const char **) const;
};
//------------------------------------------------------------------------------
class CopyEntry : public DataProcessing
{
public:
  CopyEntry(Shell &parent) :
      DataProcessing(parent)
  {
  }

  virtual const char *name() const
  {
    return "cp";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  Result processArguments(unsigned int, const char * const *, const char **,
      const char **) const;
};
//------------------------------------------------------------------------------
class DirectData : public DataProcessing
{
public:
  DirectData(Shell &parent) :
      DataProcessing(parent)
  {
  }

  virtual const char *name() const
  {
    return "dd";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  struct Arguments
  {
    uint32_t block;
    uint32_t count;
    uint32_t seek;
    uint32_t skip;
    const char *in;
    const char *out;
  };

  Result processArguments(unsigned int, const char * const *,
      Arguments *) const;
};
//------------------------------------------------------------------------------
class ExitShell : public Shell::ShellCommand
{
public:
  ExitShell(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "exit";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class ListCommands : public Shell::ShellCommand
{
public:
  ListCommands(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "help";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class ListEntries : public Shell::ShellCommand
{
public:
  ListEntries(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "ls";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class MakeDirectory : public Shell::ShellCommand
{
public:
  MakeDirectory(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "mkdir";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class RemoveDirectory : public Shell::ShellCommand
{
public:
  RemoveDirectory(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "rmdir";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class RemoveEntry : public Shell::ShellCommand
{
public:
  RemoveEntry(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "rm";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  Result processArguments(unsigned int, const char * const *, bool *,
      const char **) const;
};
//------------------------------------------------------------------------------
class Synchronize : public Shell::ShellCommand
{
public:
  Synchronize(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "sync";
  }

  virtual Result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
#endif //YAF_LIBSHELL_COMMANDS_HPP_
