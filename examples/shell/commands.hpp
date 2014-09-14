/*
 * commands.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef COMMANDS_HPP_
#define COMMANDS_HPP_
//------------------------------------------------------------------------------
#include "shell.hpp"
//------------------------------------------------------------------------------
class DataProcessing : public Shell::ShellCommand
{
public:
  DataProcessing(Shell &parent) : ShellCommand(parent) {}

protected:
  result copyContent(FsNode *, FsNode *, unsigned int, unsigned int,
      unsigned int, unsigned int, bool) const;
  result prepareNodes(FsNode **, FsNode **, const char *, const char *);
};
//------------------------------------------------------------------------------
class ChangeDirectory : public Shell::ShellCommand
{
public:
  ChangeDirectory(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "cd";
  }

  virtual result run(unsigned int, const char * const *);

private:
  result processArguments(unsigned int, const char * const *,
      const char **) const;
};
//------------------------------------------------------------------------------
class CopyEntry : public DataProcessing
{
public:
  CopyEntry(Shell &parent) : DataProcessing(parent) {}

  virtual const char *name() const
  {
    return "cp";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);

private:
  result processArguments(unsigned int, const char * const *, const char **,
      const char **) const;
};
//------------------------------------------------------------------------------
class DirectData : public DataProcessing
{
public:
  DirectData(Shell &parent) : DataProcessing(parent) {}

  virtual const char *name() const
  {
    return "dd";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);

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
  ExitShell(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "exit";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class ListCommands : public Shell::ShellCommand
{
public:
  ListCommands(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "help";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class ListEntries : public Shell::ShellCommand
{
public:
  ListEntries(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "ls";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class MakeDirectory : public Shell::ShellCommand
{
public:
  MakeDirectory(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "mkdir";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class MeasureTime : public Shell::ShellCommand
{
public:
  MeasureTime(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "time";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class RemoveDirectory : public Shell::ShellCommand
{
public:
  RemoveDirectory(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "rmdir";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class RemoveEntry : public Shell::ShellCommand
{
public:
  RemoveEntry(Shell &parent) : ShellCommand(parent) {}

  virtual const char *name() const
  {
    return "rm";
  }

  virtual result isolate(Shell::ShellContext *, unsigned int,
      const char * const *);
  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
#endif //COMMANDS_HPP_
