/*
 * crypto_wrapper.cpp
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cassert>
#include "shell/crypto_wrapper.hpp"

void Md5Hash::finalize(char *digest)
{
  uint8_t result[16];

  MD5_Final(result, &context);

  //TODO Style for lambdas
  auto hexify = [](uint8_t value) {
    return value < 10 ? '0' + value : 'a' + (value - 10);
  };

  for (size_t pos = 0; pos < 16; pos++)
  {
    digest[pos * 2 + 0] = hexify(result[pos] >> 4);
    digest[pos * 2 + 1] = hexify(result[pos] & 0x0F);
  }
  digest[32] = '\0';
}

void Md5Hash::reset()
{
  MD5_Init(&context);
}

void Md5Hash::update(const void *buffer, size_t bufferLength)
{
  MD5_Update(&context, buffer, bufferLength);
}
