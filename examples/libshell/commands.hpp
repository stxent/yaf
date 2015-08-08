/*
 * libshell/commands.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBSHELL_COMMANDS_HPP_
#define LIBSHELL_COMMANDS_HPP_
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
  result copyContent(FsNode *, FsNode *, unsigned int, unsigned int,
      unsigned int, unsigned int) const;
  result prepareNodes(Shell::ShellContext *, FsNode **, FsNode **, const char *,
      const char *, bool);
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  result processArguments(unsigned int, const char * const *,
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  result processArguments(unsigned int, const char * const *, const char **,
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

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

  result processArguments(unsigned int, const char * const *,
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class RemoveEntry : public Shell::ShellCommand
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

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);
};
//------------------------------------------------------------------------------
class TouchEntry : public Shell::ShellCommand
{
public:
  TouchEntry(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "touch";
  }

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  result processArguments(unsigned int, const char * const *,
      const char **) const;
};
//------------------------------------------------------------------------------
#endif //LIBSHELL_COMMANDS_HPP_
