/*
 * libshell/file_tools.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_FILE_TOOLS_HPP_
#define YAF_LIBSHELL_FILE_TOOLS_HPP_

#include "libshell/crypto.hpp"
#include "libshell/shell.hpp"

class CatEntry: public Shell::ShellCommand
{
public:
  CatEntry(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "cat";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  enum: size_t
  {
    HEX_OUTPUT_WIDTH = 16,
    RAW_OUTPUT_WIDTH = 32
  };

  Result print(FsNode *, bool, bool) const;
  Result processArguments(size_t, const char * const *, const char **, bool *, bool *, const char **) const;
};

class ChecksumCrc32: public ComputationAlgorithm
{
public:
  ChecksumCrc32() : sum{INITIAL_CRC}
  {
  }

  static constexpr size_t length()
  {
    return sizeof(uint32_t);
  }

  static const char *name()
  {
    return "crc32";
  }

  virtual void finalize(char *) override;
  virtual void reset() override;
  virtual void update(const void *, size_t) override;

private:
  enum
  {
    INITIAL_CRC = 0x00000000UL
  };

  uint32_t sum;
};

class EchoData: public Shell::ShellCommand
{
public:
  EchoData(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "echo";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  Result fill(FsNode *, const char *, size_t) const;
  Result processArguments(size_t, const char * const *, const char **, const char **, size_t *) const;
};

class TouchEntry: public Shell::ShellCommand
{
public:
  TouchEntry(Shell &parent) : ShellCommand{parent}
  {
  }

  virtual const char *name() const override
  {
    return "touch";
  }

  virtual Result run(const char * const *, size_t, Shell::ShellContext *) override;

private:
  Result processArguments(size_t, const char * const *, const char **) const;
};

#endif //YAF_LIBSHELL_FILE_TOOLS_HPP_
