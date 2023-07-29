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
  /** Mandatory: cluster size in bytes. */
  size_t cluster;
  /** Optional: number of reserved sectors. */
  size_t reserved;
  /** Mandatory: number of FAT tables. */
  size_t tables;
  /** Optional: volume label. */
  const char *label;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

FsCapacity fat32GetCapacity(const void *);
size_t fat32GetClusterSize(const void *);
enum Result fat32GetUsage(void *, void *, size_t, FsCapacity *);
enum Result fat32MakeFs(void *, const struct Fat32FsConfig *, void *, size_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_UTILS_H_ */
