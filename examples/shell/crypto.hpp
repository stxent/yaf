/*
 * crypto.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CRYPTO_HPP_
#define CRYPTO_HPP_
//------------------------------------------------------------------------------
#include <openssl/md5.h>
#include "shell.hpp"
//------------------------------------------------------------------------------
class ComputationCommand : public Shell::ShellCommand
{
public:
  ComputationCommand(Shell &);
  virtual result run(unsigned int, const char * const *);

protected:
  enum : unsigned int
  {
    BUFFER_LENGTH = 1024
  };

  virtual void compute(const uint8_t *, uint32_t) = 0;
  virtual void finalize(const char *) = 0;
  virtual void reset() = 0;

private:
  const char * const *getNextEntry(unsigned int, const char * const *) const;
  result processArguments(unsigned int, const char * const *) const;
};
//------------------------------------------------------------------------------
class ComputeHash : public ComputationCommand
{
public:
  ComputeHash(Shell &owner) : ComputationCommand(owner) {}

  virtual const char *name() const
  {
    return "md5sum";
  }

protected:
  virtual void compute(const uint8_t *, uint32_t);
  virtual void finalize(const char *);
  virtual void reset();

private:
  MD5_CTX context;
};
//------------------------------------------------------------------------------
#endif //CRYPTO_HPP_
