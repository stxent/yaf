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
//#define FS_WRITE_BUFFERED
#define FS_WRITE_ENABLED
#define FS_RTC_ENABLED
/*----------------------------------------------------------------------------*/
/* Cluster size may be 1, 2, 4, 8, 16, 32, 64, 128 sectors                    */
/* Sector size may be 512, 1024, 2048, 4096 bytes, default is 512             */
/*----------------------------------------------------------------------------*/
#define SECTOR_POW      9 /* Sector size in power of 2 */
#define SECTOR_SIZE     (1 << SECTOR_POW) /* Sector size in bytes */
#define FS_BUFFER       (SECTOR_SIZE * 1) /* TODO add buffering */
/*----------------------------------------------------------------------------*/
void fat32Init(struct FsHandle *);
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
uint32_t countFree(struct FsHandle *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* FAT32_H_ */
