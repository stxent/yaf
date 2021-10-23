/*
 * yaf/utils.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_UTILS_H_
#define YAF_UTILS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/error.h>
#include <xcore/helpers.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct Fat32FsConfig
{
  size_t clusterSize;
  size_t tableCount;
  const char *label;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

FsCapacity fat32GetCapacity(const void *);
size_t fat32GetClusterSize(const void *);
enum Result fat32GetUsage(void *, void *, size_t, FsCapacity *);
enum Result fat32MakeFs(void *, const struct Fat32FsConfig *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_UTILS_H_ */
