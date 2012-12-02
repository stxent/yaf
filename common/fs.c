/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "fs.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
#endif
/*----------------------------------------------------------------------------*/
#undef DEBUG
/*----------------------------------------------------------------------------*/
/* TODO add constructor, destructor and members initialization */
/*----------------------------------------------------------------------------*/
enum result fsBlockRead(struct Interface *iface, uint64_t address,
    uint8_t *buffer, uint32_t length)
{
  static uint32_t rcount = 0;

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
  printf("block_read: read position: %X (%u), requests: %u\n",
      (unsigned int)address, (unsigned int)address, rcount);
#endif
  rcount++;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum result fsBlockWrite(struct Interface *iface, uint64_t address,
    const uint8_t *buffer, uint32_t length)
{
  static uint32_t wcount = 0;

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
  printf("block_read: write position: %X (%u), requests: %u\n",
      (unsigned int)address, (unsigned int)address, wcount);
#endif
  wcount++;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsMount(struct FsHandle *sys, struct Interface *device)
{
  return ((struct FsHandleClass *)CLASS(sys))->mount ?
      ((struct FsHandleClass *)CLASS(sys))->mount(sys, device) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsUmount(struct FsHandle *sys)
{
  if (((struct FsHandleClass *)CLASS(sys))->umount)
    ((struct FsHandleClass *)CLASS(sys))->umount(sys);
}
/*----------------------------------------------------------------------------*/
enum fsResult fsStat(struct FsHandle *sys, const char *path,
    struct FsStat *result)
{
  return ((struct FsHandleClass *)CLASS(sys))->stat ?
      ((struct FsHandleClass *)CLASS(sys))->stat(sys, path, result) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
struct FsFile *fsOpen(struct FsHandle *sys, const char *path, enum fsMode mode)
{
  struct FsFile *file;

/*
  FIXME
  if (!sys || !sys->type || !sys->type->open || !(file = init(sys->File, 0))) 
 */
  if (!((struct FsHandleClass *)CLASS(sys))->open ||
      !(file = init(((struct FsHandleClass *)CLASS(sys))->File, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(sys))->open(sys, file, path, mode) !=
      FS_OK)
  {
    deinit(file);
    return 0;
  }
  return file;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsRemove(struct FsHandle *sys, const char *path)
{
  return ((struct FsHandleClass *)CLASS(sys))->remove ?
      ((struct FsHandleClass *)CLASS(sys))->remove(sys, path) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsMove(struct FsHandle *sys, const char *src, const char *dest)
{
  return ((struct FsHandleClass *)CLASS(sys))->move ?
      ((struct FsHandleClass *)CLASS(sys))->move(sys, src, dest) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
struct FsDir *fsOpenDir(struct FsHandle *sys, const char *path)
{
  struct FsDir *dir;

  if (!sys || !((struct FsHandleClass *)CLASS(sys))->openDir ||
      !(dir = init(((struct FsHandleClass *)CLASS(sys))->Dir, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(sys))->openDir(sys, dir, path) != FS_OK)
  {
    deinit(dir);
    return 0;
  }
  return dir;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsMakeDir(struct FsHandle *sys, const char *path)
{
  return ((struct FsHandleClass *)CLASS(sys))->makeDir ?
      ((struct FsHandleClass *)CLASS(sys))->makeDir(sys, path) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsClose(struct FsFile *file)
{
  if (((struct FsFileClass *)CLASS(file))->close)
    ((struct FsFileClass *)CLASS(file))->close(file);
  deinit(file);
}
/*----------------------------------------------------------------------------*/
bool fsEof(struct FsFile *file)
{
  /* Return EOF when no EOF detection function */
  return ((struct FsFileClass *)CLASS(file))->eof ?
      ((struct FsFileClass *)CLASS(file))->eof(file) : true;
}
/*----------------------------------------------------------------------------*/
/* enum fsResult fsTell(struct FsFile *, uint32_t); TODO */
/*----------------------------------------------------------------------------*/
enum fsResult fsSeek(struct FsFile *file, uint32_t position)
{
  return ((struct FsFileClass *)CLASS(file))->seek ?
      ((struct FsFileClass *)CLASS(file))->seek(file, position) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsRead(struct FsFile *file, uint8_t *buffer, uint16_t length,
    uint16_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->read ?
      ((struct FsFileClass *)CLASS(file))->read(file, buffer, length, result) :
      FS_ERROR;
}
/*----------------------------------------------------------------------------*/
enum fsResult fsWrite(struct FsFile *file, const uint8_t *buffer,
    uint16_t length, uint16_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->write ?
      ((struct FsFileClass *)CLASS(file))->write(file, buffer, length, result) :
      FS_ERROR;
}
/*----------------------------------------------------------------------------*/
void fsCloseDir(struct FsDir *dir)
{
  if (((struct FsDirClass *)CLASS(dir))->close)
    ((struct FsDirClass *)CLASS(dir))->close(dir);
  deinit(dir);
}
/*----------------------------------------------------------------------------*/
/* bool (*fsEofDir)(struct FsFile *); TODO */
/*----------------------------------------------------------------------------*/
enum fsResult fsReadDir(struct FsDir *dir, char *buffer)
{
  return ((struct FsDirClass *)CLASS(dir))->read ?
      ((struct FsDirClass *)CLASS(dir))->read(dir, buffer) : FS_ERROR;
}
/*----------------------------------------------------------------------------*/
/* enum fsResult fsRewindDir(struct FsDir *); TODO */
/*----------------------------------------------------------------------------*/
/* enum fsResult fsSeekDir(struct FsDir *, uint16_t); TODO */
/*----------------------------------------------------------------------------*/
/* uint16_t fsTellDir(struct FsDir *); TODO */
