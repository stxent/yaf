/*
 * yaf/utils.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
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
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

enum Result fat32MakeFs(struct Interface *, const struct Fat32FsConfig *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_UTILS_H_ */
