/*
 * fat32.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef FAT32_H_
#define FAT32_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
#include "fs.h"
/*----------------------------------------------------------------------------*/
extern const void *FatHandle;
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(struct FsHandle *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* FAT32_H_ */
