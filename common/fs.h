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
#include "bdev.h"
/*----------------------------------------------------------------------------*/
enum fsMode
{
    FS_NONE = 0,
    FS_READ,
    FS_WRITE,
    FS_APPEND
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
enum fsResult
{
    FS_OK = 0,
    FS_ERROR,
    FS_DEVICE_ERROR,
    FS_WRITE_ERROR,
    FS_READ_ERROR,
    FS_EOF,
    FS_NOT_FOUND
} __attribute__((packed));
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
} __attribute__((packed));
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
struct FsFile
{
  struct FsHandle *descriptor;
  enum fsMode mode; /* Access mode: read, write or append */
  uint32_t size; /* File size */
  uint32_t position; /* Position in file */
  /* Filesystem-specific data */
  void *data;

  void (*close)(struct FsFile *);
  bool (*eof)(struct FsFile *);
  /* enum fsResult (*tell)(struct FsFile *, uint32_t); TODO */
  enum fsResult (*seek)(struct FsFile *, uint32_t);
  enum fsResult (*read)(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
  enum fsResult (*write)(struct FsFile *, const uint8_t *, uint16_t,
      uint16_t *);
};
/*----------------------------------------------------------------------------*/
struct FsDir
{
  struct FsHandle *descriptor;
  /* Filesystem-specific data */
  void *data;

  void (*close)(struct FsDir *);
  /* bool (*eof)(struct FsFile *); TODO */
  enum fsResult (*read)(struct FsDir *, char *);
  /* enum fsResult (*rewind)(struct FsDir *); TODO */
  /* enum fsResult (*seek)(struct FsDir *, uint16_t); TODO */
  /* uint16_t (*tell)(struct FsDir *); TODO */
};
/*----------------------------------------------------------------------------*/
struct FsHandle
{
  struct BlockDevice *dev;
  /* Filesystem-specific data */
  void *data;

  void (*umount)(struct FsHandle *);
  enum fsResult (*stat)(struct FsHandle *, const char *, struct FsStat *);
  enum fsResult (*open)(struct FsHandle *, struct FsFile *, const char *,
      enum fsMode);
  enum fsResult (*remove)(struct FsHandle *, const char *);
  enum fsResult (*move)(struct FsHandle *, const char *, const char *);
  enum fsResult (*openDir)(struct FsHandle *, struct FsDir *, const char *);
  enum fsResult (*makeDir)(struct FsHandle *, const char *);
};
/*------------------------------------------------------------------------------*/
/* Filesystem handle functions */
enum fsResult fsMount(struct FsHandle *, struct BlockDevice *);
void fsUmount(struct FsHandle *);
enum fsResult fsStat(struct FsHandle *, const char *, struct FsStat *);
enum fsResult fsOpen(struct FsHandle *, struct FsFile *, const char *,
    enum fsMode);
enum fsResult fsRemove(struct FsHandle *, const char *);
enum fsResult fsMove(struct FsHandle *, const char *, const char *);
enum fsResult fsOpenDir(struct FsHandle *, struct FsDir *, const char *);
enum fsResult fsMakeDir(struct FsHandle *, const char *);
/*------------------------------------------------------------------------------*/
/* File functions */
void fsClose(struct FsFile *);
bool fsEof(struct FsFile *);
/* enum fsResult fsTell(struct FsFile *, uint32_t); TODO */
enum fsResult fsSeek(struct FsFile *, uint32_t);
enum fsResult fsRead(struct FsFile *, uint8_t *, uint16_t, uint16_t *);
enum fsResult fsWrite(struct FsFile *, const uint8_t *, uint16_t, uint16_t *);
/*------------------------------------------------------------------------------*/
/* Directory functions */
void fsCloseDir(struct FsDir *);
/* bool (*fsEofDir)(struct FsFile *); TODO */
enum fsResult fsReadDir(struct FsDir *, char *);
/* enum fsResult fsRewindDir(struct FsDir *); TODO */
/* enum fsResult fsSeekDir(struct FsDir *, uint16_t); TODO */
/* uint16_t fsTellDir(struct FsDir *); TODO */
/*------------------------------------------------------------------------------*/
#endif /* FS_H_ */
