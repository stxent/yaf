/*
 * shell/crypto_wrapper.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_SHELL_CRYPTO_WRAPPER_HPP_
#define YAF_SHELL_CRYPTO_WRAPPER_HPP_
//------------------------------------------------------------------------------
#include <openssl/md5.h>
#include "libshell/crypto.hpp"
//------------------------------------------------------------------------------
class Md5Hash : public ComputationAlgorithm
{
public:
  static unsigned int length()
  {
    return 32;
  }

  static const char *name()
  {
    return "md5sum";
  }

  virtual void finalize(char *);
  virtual void reset();
  virtual void update(const uint8_t *, uint32_t);

private:
  MD5_CTX context;
};
//------------------------------------------------------------------------------
#endif //YAF_SHELL_CRYPTO_WRAPPER_HPP_
