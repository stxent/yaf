/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "fs.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
uint64_t readCount = 0, writeCount = 0;
#endif
/*----------------------------------------------------------------------------*/
enum result fsBlockRead(struct Interface *iface, uint64_t address,
    uint8_t *buffer, uint32_t length)
{
  if (ifSetOpt(iface, IF_ADDRESS, &address) != E_OK)
  {
#ifdef DEBUG
    printf("block_read: error: io control\n");
#endif
    return E_ERROR;
  }
  if (ifRead(iface, buffer, length) != length)
  {
#ifdef DEBUG
    printf("block_read: error: read position: %u\n", (unsigned int)address);
#endif
    return E_ERROR;
  }
#ifdef DEBUG
  readCount++;
#endif
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum result fsBlockWrite(struct Interface *iface, uint64_t address,
    const uint8_t *buffer, uint32_t length)
{
  if (ifSetOpt(iface, IF_ADDRESS, &address) != E_OK)
  {
#ifdef DEBUG
    printf("block_read: error: io control\n");
#endif
    return E_ERROR;
  }
  if (ifWrite(iface, buffer, length) != length)
  {
#ifdef DEBUG
    printf("block_read: error: read position: %u\n", (unsigned int)address);
#endif
    return E_ERROR;
  }
#ifdef DEBUG
  writeCount++;
#endif
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum result fsStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  return ((struct FsHandleClass *)CLASS(sys))->stat(sys, path, result);
}
/*----------------------------------------------------------------------------*/
struct FsFile *fsOpen(struct FsHandle *sys, const char *path, enum fsMode mode)
{
  struct FsFile *file;

  if (!(file = init(((struct FsHandleClass *)CLASS(sys))->File, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(sys))->open(sys, file, path, mode) != E_OK)
  {
    deinit(file);
    return 0;
  }
  return file;
}
/*----------------------------------------------------------------------------*/
enum result fsRemove(struct FsHandle *sys, const char *path)
{
  return ((struct FsHandleClass *)CLASS(sys))->remove(sys, path);
}
/*----------------------------------------------------------------------------*/
enum result fsMove(struct FsHandle *sys, const char *src, const char *dest)
{
  return ((struct FsHandleClass *)CLASS(sys))->move(sys, src, dest);
}
/*----------------------------------------------------------------------------*/
struct FsDir *fsOpenDir(struct FsHandle *sys, const char *path)
{
  struct FsDir *dir;

  if (!(dir = init(((struct FsHandleClass *)CLASS(sys))->Dir, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(sys))->openDir(sys, dir, path) != E_OK)
  {
    deinit(dir);
    return 0;
  }
  return dir;
}
/*----------------------------------------------------------------------------*/
enum result fsMakeDir(struct FsHandle *sys, const char *path)
{
  return ((struct FsHandleClass *)CLASS(sys))->makeDir(sys, path);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void fsClose(struct FsFile *file)
{
  ((struct FsFileClass *)CLASS(file))->close(file);
  deinit(file);
}
/*----------------------------------------------------------------------------*/
bool fsEof(struct FsFile *file)
{
  return ((struct FsFileClass *)CLASS(file))->eof(file);
}
/*----------------------------------------------------------------------------*/
int64_t fsTell(struct FsFile *file)
{
  return ((struct FsFileClass *)CLASS(file))->tell(file);
}
/*----------------------------------------------------------------------------*/
enum result fsSeek(struct FsFile *file, int64_t offset,
    enum fsSeekOrigin origin)
{
  return ((struct FsFileClass *)CLASS(file))->seek(file, offset, origin);
}
/*----------------------------------------------------------------------------*/
enum result fsRead(struct FsFile *file, uint8_t *buffer, uint16_t length,
    uint16_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->read(file, buffer, length,
      result);
}
/*----------------------------------------------------------------------------*/
enum result fsWrite(struct FsFile *file, const uint8_t *buffer, uint16_t length,
    uint16_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->write(file, buffer, length,
      result);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void fsCloseDir(struct FsDir *dir)
{
  ((struct FsDirClass *)CLASS(dir))->close(dir);
  deinit(dir);
}
/*----------------------------------------------------------------------------*/
/* bool (*fsEofDir)(struct FsFile *); TODO */
/*----------------------------------------------------------------------------*/
enum result fsReadDir(struct FsDir *dir, char *buffer)
{
  return ((struct FsDirClass *)CLASS(dir))->read(dir, buffer);
}
/*----------------------------------------------------------------------------*/
/* enum result fsRewindDir(struct FsDir *); TODO */
/*----------------------------------------------------------------------------*/
/* enum result fsSeekDir(struct FsDir *, uint16_t); TODO */
/*----------------------------------------------------------------------------*/
/* uint16_t fsTellDir(struct FsDir *); TODO */
