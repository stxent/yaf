/*
 * fs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

/**
 * @file
 * Abstract filesystem interface for embedded system applications.
 */

#ifndef FS_H_
#define FS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#include "entity.h"
#include "error.h"
#include "interface.h"
/*----------------------------------------------------------------------------*/
/* Type which represents address space size */
typedef int64_t asize_t;
/*----------------------------------------------------------------------------*/
enum fsMode
{
    FS_NONE = 0x00,
    /** Open file for reading. */
    FS_READ = 0x01,
    /** Truncate to zero length or create file for writing. */
    FS_WRITE = 0x02,
    /** Append: open file for writing at end-of-file. */
    FS_APPEND = 0x04,
    /** Open file for update: reading and writing. */
    FS_UPDATE = 0x08
};
/*----------------------------------------------------------------------------*/
enum fsSeekOrigin
{
    /** Beginning of file. */
    FS_SEEK_SET = 0,
    /** Current position of the file pointer. */
    FS_SEEK_CUR,
    /** End of file. */
    FS_SEEK_END
};
/*----------------------------------------------------------------------------*/
enum fsEntryType
{
    /** Unknown type. */
    FS_TYPE_NONE = 0,
    /** Directory entry. */
    FS_TYPE_DIR,
    /** Regular file. */
    FS_TYPE_FILE,
    /** Symbolic link. */
    FS_TYPE_LINK
};
/*----------------------------------------------------------------------------*/
struct FsFile;
struct FsDir;
struct FsHandle;
// struct FsStat;
/*----------------------------------------------------------------------------*/
/* TODO rewrite */
struct FsStat
{
  enum fsEntryType type;
  asize_t size;
  uint64_t atime;
#ifdef DEBUG
  uint16_t access;
  uint32_t cluster;
  uint32_t pcluster;
  uint16_t pindex;
#endif
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
struct FsFileClass
{
  CLASS_GENERATOR

  /* Virtual functions */
  void (*close)(void *);
  bool (*eof)(void *);
  enum result (*flush)(void *);
  uint32_t (*read)(void *, uint8_t *, uint32_t);
  enum result (*seek)(void *, asize_t, enum fsSeekOrigin);
  asize_t (*tell)(void *);
  uint32_t (*write)(void *, const uint8_t *, uint32_t);
};
/*----------------------------------------------------------------------------*/
struct FsFile
{
  struct Entity parent;

  struct FsHandle *descriptor;
};
/*----------------------------------------------------------------------------*/
struct FsDirClass
{
  CLASS_GENERATOR

  /* Virtual functions */
  void (*close)(void *);
  enum result (*read)(void *, char *);
};
/*----------------------------------------------------------------------------*/
struct FsDir
{
  struct Entity parent;

  struct FsHandle *descriptor;
};
/*----------------------------------------------------------------------------*/
struct FsHandleClass
{
  CLASS_GENERATOR

  /* Virtual functions */
  enum result (*move)(void *, const char *, const char *);
  enum result (*stat)(void *, struct FsStat *, const char *);
  void *(*open)(void *, const char *, enum fsMode);
  enum result (*remove)(void *, const char *);
  void *(*openDir)(void *, const char *);
  enum result (*makeDir)(void *, const char *);
  enum result (*removeDir)(void *, const char *);
};
/*----------------------------------------------------------------------------*/
struct FsHandle
{
  struct Entity parent;

  struct Interface *dev;
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* Block access functions */
/* Address space defined by asize_t, block size defined by uint32_t */
enum result fsBlockRead(void *, asize_t, uint8_t *, uint32_t);
enum result fsBlockWrite(void *, asize_t, const uint8_t *, uint32_t);
/*----------------------------------------------------------------------------*/
/*------------------Filesystem handle functions-------------------------------*/
/* Common functions */
enum result fsMove(void *, const char *, const char *);
enum result fsStat(void *, struct FsStat *, const char *);
/* File functions */
void *fsOpen(void *, const char *, enum fsMode);
enum result fsRemove(void *, const char *);
/* Directory functions */
void *fsOpenDir(void *, const char *);
enum result fsMakeDir(void *, const char *);
enum result fsRemoveDir(void *, const char *);
/*----------------------------------------------------------------------------*/
/*------------------File functions--------------------------------------------*/
void fsClose(void *);
bool fsEof(void *);
enum result fsFlush(void *);
uint32_t fsRead(void *, uint8_t *, uint32_t);
enum result fsSeek(void *, asize_t, enum fsSeekOrigin);
asize_t fsTell(void *);
uint32_t fsWrite(void *, const uint8_t *, uint32_t);
/*----------------------------------------------------------------------------*/
/*------------------Directory functions---------------------------------------*/
void fsCloseDir(void *);
enum result fsReadDir(void *, char *);
/*----------------------------------------------------------------------------*/
#endif /* FS_H_ */
