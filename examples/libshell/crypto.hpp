/*
 * libshell/crypto.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_LIBSHELL_CRYPTO_HPP_
#define YAF_LIBSHELL_CRYPTO_HPP_

#include <cassert>
#include "libshell/shell.hpp"

class ComputationAlgorithm
{
public:
  virtual ~ComputationAlgorithm() = default;
  virtual void finalize(char *) = 0;
  virtual void reset() = 0;
  virtual void update(const void *, size_t) = 0;
};

class AbstractComputationCommand: public Shell::ShellCommand
{
public:
  AbstractComputationCommand(Shell &parent) : ShellCommand{parent}
  {
  }

protected:
  enum
  {
    MAX_LENGTH = 64
  };

  Result compute(const char * const *, size_t, Shell::ShellContext *, ComputationAlgorithm *) const;

private:
  const char * const *getNextEntry(size_t, const char * const *, bool *, char *) const;
  Result processArguments(size_t, const char * const *) const;
  Result processEntry(FsNode *, Shell::ShellContext *, ComputationAlgorithm *, const char *) const;
};

template<class T> class ComputationCommand: public AbstractComputationCommand
{
public:
  ComputationCommand(Shell &parent) : AbstractComputationCommand{parent}
  {
    static_assert(T::length() <= MAX_LENGTH, "Incorrect length");
  }

  virtual const char *name() const override
  {
    return T::name();
  }

  virtual Result run(const char * const *arguments, size_t count, Shell::ShellContext *context) override
  {
    T algorithm;

    return compute(arguments, count, context, &algorithm);
  }
};

#endif //YAF_LIBSHELL_CRYPTO_HPP_
