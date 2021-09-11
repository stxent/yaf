/*
 * yaf/utils.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_UTILS_H_
#define YAF_UTILS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/interface.h>
/*----------------------------------------------------------------------------*/
struct Fat32FsConfig
{
  size_t clusterSize;
  size_t tableCount;
  const char *label;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

enum Result fat32MakeFs(void *, const struct Fat32FsConfig *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_UTILS_H_ */
