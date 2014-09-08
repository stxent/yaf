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
class ChangeDirectory : public Shell::ShellCommand
{
public:
  ChangeDirectory(Shell &owner) : ShellCommand(owner) {}

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
class CopyEntry : public Shell::ShellCommand
{
public:
  CopyEntry(Shell &owner) : ShellCommand(owner) {}

  virtual const char *name() const
  {
    return "cp";
  }

  virtual result run(unsigned int, const char * const *);

private:
  enum : unsigned int
  {
    BUFFER_LENGTH = 1024
  };

  result copyContent(FsNode *, FsNode *, unsigned int) const;
  result processArguments(unsigned int, const char * const *, const char **,
      const char **, unsigned int *) const;
};
//------------------------------------------------------------------------------
class ExitShell : public Shell::ShellCommand
{
public:
  ExitShell(Shell &owner) : ShellCommand(owner) {}

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
  ListCommands(Shell &owner) : ShellCommand(owner) {}

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
  ListEntries(Shell &owner) : ShellCommand(owner) {}

  virtual const char *name() const
  {
    return "ls";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class MakeDirectory : public Shell::ShellCommand
{
public:
  MakeDirectory(Shell &owner) : ShellCommand(owner) {}

  virtual const char *name() const
  {
    return "mkdir";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class MeasureTime : public Shell::ShellCommand
{
public:
  MeasureTime(Shell &owner) : ShellCommand(owner) {}

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
  RemoveDirectory(Shell &owner) : ShellCommand(owner) {}

  virtual const char *name() const
  {
    return "rmdir";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class RemoveEntry : public Shell::ShellCommand
{
public:
  RemoveEntry(Shell &owner) : ShellCommand(owner) {}

  virtual const char *name() const
  {
    return "rm";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
#endif //COMMANDS_HPP_
