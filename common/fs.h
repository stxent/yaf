/*
 * fs.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef FS_H_
#define FS_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
#include "error.h"
#include "entity.h"
#include "interface.h"
/*----------------------------------------------------------------------------*/
typedef uint32_t bsize_t; /* Block size for memory operations */
typedef int64_t asize_t; /* Underlying device address space, signed value */
/*----------------------------------------------------------------------------*/
enum fsMode
{
    FS_NONE = 0,
    FS_READ,
    FS_WRITE,
    FS_APPEND
};
/*----------------------------------------------------------------------------*/
enum fsSeekOrigin
{
    FS_SEEK_SET = 0,
    FS_SEEK_CUR,
    FS_SEEK_END
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* TODO rewrite */
enum fsEntryType
{
    FS_TYPE_NONE = 0, /* Unknown type */
    FS_TYPE_FIFO, /* Named pipe */
    FS_TYPE_CHAR, /* Character device */
    FS_TYPE_DIR, /* Directory */
    FS_TYPE_REG, /* Regular file */
    FS_TYPE_LNK, /* Symbolic link */
    FS_TYPE_SOCK /* Socket */
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
  uint64_t atime;
  uint32_t size;
#ifdef DEBUG
  uint16_t access;
  uint32_t cluster;
  uint32_t pcluster;
  uint16_t pindex;
#endif
  enum fsEntryType type;
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
struct FsFileClass
{
  CLASS_GENERATOR(FsFile)

  /* Virtual methods */
  void (*close)(struct FsFile *);
  bool (*eof)(struct FsFile *);
  asize_t (*tell)(struct FsFile *);
  enum result (*seek)(struct FsFile *, asize_t, enum fsSeekOrigin);
  enum result (*read)(struct FsFile *, uint8_t *, bsize_t, bsize_t *);
  enum result (*write)(struct FsFile *, const uint8_t *, bsize_t, bsize_t *);
};
/*----------------------------------------------------------------------------*/
struct FsFile
{
  struct Entity parent;

  struct FsHandle *descriptor;
  enum fsMode mode; /* Access mode: read, write or append */
};
/*----------------------------------------------------------------------------*/
struct FsDirClass
{
  CLASS_GENERATOR(FsDir)

  /* Virtual methods */
  void (*close)(struct FsDir *);
  /* bool (*eof)(struct FsFile *); TODO */
  enum result (*read)(struct FsDir *, char *);
  /* enum result (*rewind)(struct FsDir *); TODO */
  /* enum result (*seek)(struct FsDir *, uint16_t); TODO */
  /* uint16_t (*tell)(struct FsDir *); TODO */
};
/*----------------------------------------------------------------------------*/
struct FsDir
{
  struct Entity parent;

  struct FsHandle *descriptor;
  /* uint16_t position; TODO */
};
/*----------------------------------------------------------------------------*/
struct FsHandleClass
{
  CLASS_GENERATOR(FsHandle)

  /* Pointers to subclasses */
  const void *File;
  const void *Dir;

  /* Virtual methods */
  enum result (*open)(struct FsHandle *, struct FsFile *, const char *,
      enum fsMode);
  enum result (*openDir)(struct FsHandle *, struct FsDir *, const char *);
  enum result (*remove)(struct FsHandle *, const char *);
  enum result (*move)(struct FsHandle *, const char *, const char *);
  enum result (*makeDir)(struct FsHandle *, const char *);
  enum result (*stat)(struct FsHandle *, const char *, struct FsStat *);
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
/* Address space defined by asize_t, block size defined by bsize_t */
enum result fsBlockRead(struct Interface *, asize_t, uint8_t *, bsize_t);
enum result fsBlockWrite(struct Interface *, asize_t, const uint8_t *, bsize_t);
/*----------------------------------------------------------------------------*/
/* Filesystem handle functions */
enum result fsMount(struct FsHandle *, struct Interface *);
void fsUmount(struct FsHandle *);
struct FsFile *fsOpen(struct FsHandle *, const char *, enum fsMode);
enum result fsRemove(struct FsHandle *, const char *);
enum result fsMove(struct FsHandle *, const char *, const char *);
struct FsDir *fsOpenDir(struct FsHandle *, const char *);
enum result fsMakeDir(struct FsHandle *, const char *);
enum result fsStat(struct FsHandle *, const char *, struct FsStat *);
/*----------------------------------------------------------------------------*/
/* File functions */
void fsClose(struct FsFile *);
bool fsEof(struct FsFile *);
asize_t fsTell(struct FsFile *);
enum result fsSeek(struct FsFile *, asize_t, enum fsSeekOrigin);
enum result fsRead(struct FsFile *, uint8_t *, bsize_t, bsize_t *);
enum result fsWrite(struct FsFile *, const uint8_t *, bsize_t, bsize_t *);
/*----------------------------------------------------------------------------*/
/* Directory functions */
void fsCloseDir(struct FsDir *);
/* bool (*fsEofDir)(struct FsFile *); TODO */
enum result fsReadDir(struct FsDir *, char *);
/* enum result fsRewindDir(struct FsDir *); TODO */
/* enum result fsSeekDir(struct FsDir *, uint16_t); TODO */
/* uint16_t fsTellDir(struct FsDir *); TODO */
/*----------------------------------------------------------------------------*/
#endif /* FS_H_ */
