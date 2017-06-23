/*
 * libshell/commands.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_COMMANDS_HPP_
#define YAF_LIBSHELL_COMMANDS_HPP_

#include "libshell/shell.hpp"

class DataProcessing: public Shell::ShellCommand
{
public:
  DataProcessing(Shell &parent) : ShellCommand{parent}
  {
  }

protected:
  Result copyContent(FsNode *, FsNode *, size_t, size_t, size_t, size_t) const;
  Result prepareNodes(Shell::ShellContext *, FsNode **, FsNode **, const char *, const char *, bool);
  Result removeNode(Shell::ShellContext *, FsNode *, char *);
};

class ChangeDirectory: public Shell::ShellCommand
{
public:
  ChangeDirectory(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "cd";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  Result processArguments(size_t, const char * const *, const char **) const;
};

class CopyEntry: public DataProcessing
{
public:
  CopyEntry(Shell &parent) : DataProcessing{parent}
  {
  }

  virtual const char *name() const override
  {
    return "cp";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  Result processArguments(size_t, const char * const *, const char **, const char **) const;
};

class DirectData: public DataProcessing
{
public:
  DirectData(Shell &parent) : DataProcessing{parent}
  {
  }

  virtual const char *name() const override
  {
    return "dd";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  struct Arguments
  {
    size_t block;
    size_t count;
    size_t seek;
    size_t skip;
    const char *in;
    const char *out;
  };

  Result processArguments(size_t, const char * const *, Arguments *) const;
};

class ExitShell: public Shell::ShellCommand
{
public:
  ExitShell(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "exit";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override
  {
    return static_cast<Result>(Shell::E_SHELL_EXIT);
  }
};

class ListCommands: public Shell::ShellCommand
{
public:
  ListCommands(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "help";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;
};

class ListEntries: public Shell::ShellCommand
{
public:
  ListEntries(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "ls";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;
};

class MakeDirectory: public Shell::ShellCommand
{
public:
  MakeDirectory(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "mkdir";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;
};

class RemoveDirectory: public Shell::ShellCommand
{
public:
  RemoveDirectory(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "rmdir";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;
};

class RemoveEntry: public Shell::ShellCommand
{
public:
  RemoveEntry(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "rm";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  Result processArguments(size_t, const char * const *, bool *, const char **) const;
};

class Synchronize: public Shell::ShellCommand
{
public:
  Synchronize(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "sync";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;
};

#endif //YAF_LIBSHELL_COMMANDS_HPP_
