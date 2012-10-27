/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "fs.h"
/*------------------------------------------------------------------------------*/
enum fsResult fsMount(struct FsHandle *sys, struct BlockDevice *dev)
{
  return sys->mount(sys, dev);
}
/*------------------------------------------------------------------------------*/
void fsUmount(struct FsHandle *sys)
{
  sys->umount(sys);
}
/*------------------------------------------------------------------------------*/
enum fsResult fsStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  if (sys->stat)
    return sys->stat(sys, path, result);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsOpen(struct FsHandle *sys, struct FsFile *file, const char *path,
    enum fsMode mode)
{
  if (sys->open)
    return sys->open(sys, file, path, mode);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsRemove(struct FsHandle *sys, const char *path)
{
  if (sys->remove)
    return sys->remove(sys, path);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsMove(struct FsHandle *sys, const char *src, const char *dest)
{
  if (sys->move)
    return sys->move(sys, src, dest);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsOpenDir(struct FsHandle *sys, struct FsDir *dir, const char *path)
{
  if (sys->openDir)
    return sys->openDir(sys, dir, path);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsMakeDir(struct FsHandle *sys, const char *path)
{
  if (sys->makeDir)
    return sys->makeDir(sys, path);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
void fsClose(struct FsFile *file)
{
  if (file->close)
    file->close(file);
}
/*------------------------------------------------------------------------------*/
bool fsEof(struct FsFile *file)
{
  if (file->eof)
    return file->eof(file);
  else
    return true; /* No EOF detection function, return EOF */
}
/*------------------------------------------------------------------------------*/
/* enum fsResult fsTell(struct FsFile *, uint32_t); TODO */
/*------------------------------------------------------------------------------*/
enum fsResult fsSeek(struct FsFile *file, uint32_t pos)
{
  if (file->seek)
    return file->seek(file, pos);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsRead(struct FsFile *file, uint8_t *buf, uint16_t len,
    uint16_t *result)
{
  if (file->read)
    return file->read(file, buf, len, result);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
enum fsResult fsWrite(struct FsFile *file, const uint8_t *buf, uint16_t len,
    uint16_t *result)
{
  if (file->write)
    return file->write(file, buf, len, result);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
void fsCloseDir(struct FsDir *dir)
{
  if (dir->close)
    dir->close(dir);
}
/*------------------------------------------------------------------------------*/
/* bool (*fsEofDir)(struct FsFile *); TODO */
/*------------------------------------------------------------------------------*/
enum fsResult fsReadDir(struct FsDir *dir, char *buf)
{
  if (dir->read)
    return dir->read(dir, buf);
  else
    return FS_ERROR;
}
/*------------------------------------------------------------------------------*/
/* enum fsResult fsRewindDir(struct FsDir *); TODO */
/*------------------------------------------------------------------------------*/
/* enum fsResult fsSeekDir(struct FsDir *, uint16_t); TODO */
/*------------------------------------------------------------------------------*/
/* uint16_t fsTellDir(struct FsDir *); TODO */
