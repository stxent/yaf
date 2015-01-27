/*
 * shell/crypto_wrapper.hpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef SHELL_CRYPTO_WRAPPER_HPP_
#define SHELL_CRYPTO_WRAPPER_HPP_
//------------------------------------------------------------------------------
#include <openssl/md5.h>
#include "libshell/crypto.hpp"
//------------------------------------------------------------------------------
class Md5Hash : public ComputationAlgorithm
{
public:
  static const char *name()
  {
    return "md5sum";
  }

  virtual void finalize(char *digest, uint32_t length);
  virtual void reset();
  virtual void update(const uint8_t *buffer, uint32_t length);

private:
  MD5_CTX context;
};
//------------------------------------------------------------------------------
#endif //SHELL_CRYPTO_WRAPPER_HPP_
