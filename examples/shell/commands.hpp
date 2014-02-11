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
  ChangeDirectory(Shell *owner) : ShellCommand(owner, "cd") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
////TODO Extend with different hash and checksum types
//class ComputeHash : public Shell::ShellCommand
//{
//public:
//  ComputeHash(Shell *owner) : ShellCommand(owner, "md5sum") {}
//  virtual result run(unsigned int count, char *arguments[]) const;
//};
//------------------------------------------------------------------------------
class CopyEntry : public Shell::ShellCommand
{
public:
  CopyEntry(Shell *owner) : ShellCommand(owner, "cp") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class ExitShell : public Shell::ShellCommand
{
public:
  ExitShell(Shell *owner) : ShellCommand(owner, "exit") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class ListCommands : public Shell::ShellCommand
{
public:
  ListCommands(Shell *owner) : ShellCommand(owner, "help") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class ListEntries : public Shell::ShellCommand
{
public:
  ListEntries(Shell *owner) : ShellCommand(owner, "ls") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class MakeDirectory : public Shell::ShellCommand
{
public:
  MakeDirectory(Shell *owner) : ShellCommand(owner, "mkdir") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class MeasureTime : public Shell::ShellCommand
{
public:
  MeasureTime(Shell *owner) : ShellCommand(owner, "time") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class RemoveDirectory : public Shell::ShellCommand
{
public:
  RemoveDirectory(Shell *owner) : ShellCommand(owner, "rmdir") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
class RemoveEntry : public Shell::ShellCommand
{
public:
  RemoveEntry(Shell *owner) : ShellCommand(owner, "rm") {}
  virtual result run(unsigned int count, char *arguments[]) const;
};
//------------------------------------------------------------------------------
#endif //COMMANDS_HPP_
