/*
 * libshell/file_tools.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBSHELL_FILE_TOOLS_HPP_
#define LIBSHELL_FILE_TOOLS_HPP_
//------------------------------------------------------------------------------
#include "libshell/crypto.hpp"
#include "libshell/shell.hpp"

extern "C"
{
#include <crc.h>
}
//------------------------------------------------------------------------------
class CatEntry : public Shell::ShellCommand
{
public:
  CatEntry(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "cat";
  }

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  enum : unsigned int
  {
    HEX_OUTPUT_WIDTH = 16,
    RAW_OUTPUT_WIDTH = 32
  };

  result print(FsNode *, bool) const;
  result processArguments(unsigned int, const char * const *,
      const char **, bool *) const;
};
//------------------------------------------------------------------------------
class ChecksumCrc32 : public ComputationAlgorithm
{
public:
  ChecksumCrc32();
  virtual ~ChecksumCrc32();

  static unsigned int length()
  {
    return 4;
  }

  static const char *name()
  {
    return "crc32";
  }

  virtual void finalize(char *);
  virtual void reset();
  virtual void update(const uint8_t *, uint32_t);

private:
  enum
  {
    INITIAL_CRC = 0x00000000UL
  };

  CrcEngine *engine;
  uint32_t sum;
};
//------------------------------------------------------------------------------
class EchoData : public Shell::ShellCommand
{
public:
  EchoData(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "echo";
  }

  virtual result run(unsigned int, const char * const *, Shell::ShellContext *);

private:
  result fill(FsNode *, const char *, unsigned int) const;
  result processArguments(unsigned int, const char * const *,
      const char **, const char **, unsigned int *) const;
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
#endif //LIBSHELL_FILE_TOOLS_HPP_
