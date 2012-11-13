/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "fs.h"
/*----------------------------------------------------------------------------*/
enum fsResult fsMount(struct FsHandle *sys, struct BlockInterface *dev)
{
  return sys->type->mount ? sys->type->mount(sys, dev) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsUmount(struct FsHandle *sys)
{
  if (sys->type->umount)
    sys->type->umount(sys);
}
/*----------------------------------------------------------------------------*/
enum fsResult fsStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  return sys->type->stat ? sys->type->stat(sys, path, result) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
struct FsFile *fsOpen(struct FsHandle *sys, const char *path, enum fsMode mode)
{
  struct FsFile *file;

/*
  FIXME
  if (!sys || !sys->type || !sys->type->open || !(file = init(sys->File, 0))) 
 */
  if (!sys->type->open || !(file = init(sys->type->File, 0)))
    return 0;
  if (sys->type->open(sys, file, path, mode) != FS_OK)
  {
    deinit(file);
    return 0;
  }
  return file;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsRemove(struct FsHandle *sys, const char *path)
{
  return sys->type->remove ? sys->type->remove(sys, path) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsMove(struct FsHandle *sys, const char *src, const char *dest)
{
  return sys->type->move ? sys->type->move(sys, src, dest) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
struct FsDir *fsOpenDir(struct FsHandle *sys, const char *path)
{
  struct FsDir *dir;

  if (!sys || !sys->type->openDir || !(dir = init(sys->type->Dir, 0)))
    return 0;
  if (sys->type->openDir(sys, dir, path) != FS_OK)
  {
    deinit(dir);
    return 0;
  }
  return dir;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsMakeDir(struct FsHandle *sys, const char *path)
{
  return sys->type->makeDir ? sys->type->makeDir(sys, path) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsClose(struct FsFile *file)
{
  if (file->type->close)
    file->type->close(file);
  deinit(file);
}
/*----------------------------------------------------------------------------*/
bool fsEof(struct FsFile *file)
{
  /* Return EOF when no EOF detection function */
  return file->type->eof ? file->type->eof(file) : true;
}
/*----------------------------------------------------------------------------*/
/* enum fsResult fsTell(struct FsFile *, uint32_t); TODO */
/*----------------------------------------------------------------------------*/
enum fsResult fsSeek(struct FsFile *file, uint32_t pos)
{
  return file->type->seek ? file->type->seek(file, pos) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsRead(struct FsFile *file, uint8_t *buf, uint16_t len,
    uint16_t *res)
{
  return file->type->read ? file->type->read(file, buf, len, res) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsWrite(struct FsFile *file, const uint8_t *buf, uint16_t len,
    uint16_t *res)
{
  return file->type->write ? file->type->write(file, buf, len, res) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsCloseDir(struct FsDir *dir)
{
  if (dir->type->close)
    dir->type->close(dir);
  deinit(dir);
}
/*----------------------------------------------------------------------------*/
/* bool (*fsEofDir)(struct FsFile *); TODO */
/*----------------------------------------------------------------------------*/
enum fsResult fsReadDir(struct FsDir *dir, char *buf)
{
  return dir->type->read ? dir->type->read(dir, buf) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
/* enum fsResult fsRewindDir(struct FsDir *); TODO */
/*----------------------------------------------------------------------------*/
/* enum fsResult fsSeekDir(struct FsDir *, uint16_t); TODO */
/*----------------------------------------------------------------------------*/
/* uint16_t fsTellDir(struct FsDir *); TODO */
