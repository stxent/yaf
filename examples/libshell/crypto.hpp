/*
 * libshell/crypto.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_CRYPTO_HPP_
#define YAF_LIBSHELL_CRYPTO_HPP_
//------------------------------------------------------------------------------
#include <cassert>
#include "libshell/shell.hpp"
//------------------------------------------------------------------------------
class ComputationAlgorithm
{
public:
  virtual ~ComputationAlgorithm()
  {
  }

  virtual void finalize(char *) = 0;
  virtual void reset() = 0;
  virtual void update(const uint8_t *, uint32_t) = 0;
};
//------------------------------------------------------------------------------
class AbstractComputationCommand : public Shell::ShellCommand
{
public:
  AbstractComputationCommand(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual ~AbstractComputationCommand()
  {
  }

protected:
  result compute(unsigned int, const char * const *,
      Shell::ShellContext *, ComputationAlgorithm *) const;

private:
  enum
  {
    MAX_LENGTH = 64
  };

  const char * const *getNextEntry(unsigned int, const char * const *, bool *,
      char *) const;
  result processArguments(unsigned int, const char * const *) const;
  result processEntry(FsNode *, Shell::ShellContext *,
      ComputationAlgorithm *, const char *) const;
};
//------------------------------------------------------------------------------
template<class T> class ComputationCommand : public AbstractComputationCommand
{
public:
  ComputationCommand(Shell &parent) :
      AbstractComputationCommand(parent)
  {
    assert(T::length() <= MAX_LENGTH);
  }

  virtual ~ComputationCommand()
  {
  }

  virtual const char *name() const
  {
    return T::name();
  }

  virtual result run(unsigned int count, const char * const *arguments,
      Shell::ShellContext *context)
  {
    T algorithm;

    return compute(count, arguments, context, &algorithm);
  }
};
//------------------------------------------------------------------------------
#endif //YAF_LIBSHELL_CRYPTO_HPP_
