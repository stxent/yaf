/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

/**
 *  @file
 *  Abstract filesystem interface for embedded system applications
 */

#include "fs.h"
/*----------------------------------------------------------------------------*/
#ifdef DEBUG
#include <stdio.h>
uint64_t readCount = 0, writeCount = 0;
#endif
/*----------------------------------------------------------------------------*/
/** Read block from underlying physical device
 *  @param interface Pointer to Interface object
 *  @param address Address to write
 *  @param buffer Pointer to the data buffer
 *  @param size Data buffer size
 *  @return Returns E_OK on success
 */
enum result fsBlockRead(void *interface, asize_t address,
    uint8_t *buffer, uint32_t length)
{
  if (ifSetOpt(interface, IF_ADDRESS, &address) != E_OK)
  {
#ifdef DEBUG
    printf("block_read: error: io control\n");
#endif
    return E_ERROR;
  }
  if (ifRead(interface, buffer, length) != length)
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
/** Write block to underlying physical device
 *  @param interface Pointer to Interface object
 *  @param address Address to write
 *  @param buffer Pointer to the data buffer
 *  @param size Data buffer size
 *  @return Returns E_OK on success
 */
enum result fsBlockWrite(void *interface, asize_t address,
    const uint8_t *buffer, uint32_t size)
{
  if (ifSetOpt(interface, IF_ADDRESS, &address) != E_OK)
  {
#ifdef DEBUG
    printf("block_read: error: io control\n");
#endif
    return E_ERROR;
  }
  if (ifWrite(interface, buffer, size) != size)
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
/** Read filesystem entry information
 *  @param handle Pointer to FsHandle object
 *  @param src Source path
 *  @param dest Destination path
 *  @return Returns E_OK on success
 */
enum result fsMove(void *handle, const char *src, const char *dest)
{
  return ((struct FsHandleClass *)CLASS(handle))->move(handle, src, dest);
}
/*----------------------------------------------------------------------------*/
/** Read filesystem entry information
 *  @param handle Pointer to FsHandle object
 *  @param result Pointer to FsStat object to store information
 *  @param path Path to the file
 *  @return Returns E_OK on success
 */
enum result fsStat(void *handle, struct FsStat *result, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->stat(handle, result, path);
}
/*----------------------------------------------------------------------------*/
/** Open specified file for read, write or append operations
 *  @param handle Pointer to FsHandle object
 *  @param path Path to the file
 *  @param mode Open mode
 *  @return Pointer to opened file handler on success or zero on error
 */
void *fsOpen(void *handle, const char *path, enum fsMode mode)
{
  struct FsFile *file;

  if (!(file = init(((struct FsHandleClass *)CLASS(handle))->File, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(handle))->open(handle,
      file, path, mode) != E_OK)
  {
    deinit(file);
    return 0;
  }
  return file;
}
/*----------------------------------------------------------------------------*/
/** Remove file
 *  @param handle Pointer to FsHandle object
 *  @param path Path to the file
 *  @return Returns E_OK on success
 */
enum result fsRemove(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->remove(handle, path);
}
/*----------------------------------------------------------------------------*/
/** Open specified directory
 *  @param handle Pointer to FsHandle object
 *  @param path Path to the directory
 *  @return Pointer to opened directory handle on success or zero on error
 */
void *fsOpenDir(void *handle, const char *path)
{
  struct FsDir *dir;

  if (!(dir = init(((struct FsHandleClass *)CLASS(handle))->Dir, 0)))
    return 0;
  if (((struct FsHandleClass *)CLASS(handle))->openDir(handle,
      dir, path) != E_OK)
  {
    deinit(dir);
    return 0;
  }
  return dir;
}
/*----------------------------------------------------------------------------*/
/** Create directory, if it does not already exist
 *  @param handle Pointer to FsHandle object
 *  @param path Path to the new directory
 *  @return Returns E_OK on success
 */
enum result fsMakeDir(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->makeDir(handle, path);
}
/*----------------------------------------------------------------------------*/
/** Remove directory, directory must be empty
 *  @param handle Pointer to FsHandle object
 *  @param path Path to the directory
 *  @return Returns E_OK on success
 */
enum result fsRemoveDir(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->removeDir(handle, path);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/** Close recently opened file handler
 *  @param file Pointer to FsFile object
 */
void fsClose(void *file)
{
  ((struct FsFileClass *)CLASS(file))->close(file);
  deinit(file);
}
/*----------------------------------------------------------------------------*/
/** Check whether the position in file reached the last character
 *  @param file Pointer to FsFile object
 *  @return Returns true on when reached the end of file or false otherwise
 */
bool fsEof(const void *file)
{
  return ((struct FsFileClass *)CLASS(file))->eof(file);
}
/*----------------------------------------------------------------------------*/
/** Write updated file information on memory device
 *  @param file Pointer to FsFile object
 *  @return Returns E_OK on success
 */
enum result fsFlush(void *file)
{
  return ((struct FsFileClass *)CLASS(file))->flush(file);
}
/*----------------------------------------------------------------------------*/
/** Read data from the recently opened file
 *  @param file Pointer to FsFile object
 *  @param buffer Pointer to the data buffer with at least length bytes
 *  @param length Number of data bytes to be read
 *  @param result Pointer to value where total number of bytes successfully
 *  read will be stored
 *  @return Returns E_OK on success
 */
enum result fsRead(void *file, uint8_t *buffer, uint32_t length,
    uint32_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->read(file, buffer, length,
      result);
}
/*----------------------------------------------------------------------------*/
/** Set the position in file to a new position
 *  @param file Pointer to FsFile object
 *  @param offset Number of bytes to offset from origin
 *  @param origin Position used as reference for the offset
 *  @return Returns E_OK on success
 */
enum result fsSeek(void *file, asize_t offset, enum fsSeekOrigin origin)
{
  return ((struct FsFileClass *)CLASS(file))->seek(file, offset, origin);
}
/*----------------------------------------------------------------------------*/
/** Get the current position in file
 *  @param file Pointer to FsFile object
 *  @return Returns position in file
 */
asize_t fsTell(const void *file)
{
  return ((struct FsFileClass *)CLASS(file))->tell(file);
}
/*----------------------------------------------------------------------------*/
/** Write data to the recently opened file
 *  @param file Pointer to FsFile object
 *  @param buffer Pointer to the data buffer with at least length bytes
 *  @param length Number of data bytes to be written
 *  @param result Pointer to value where total number of bytes successfully
 *  written will be stored
 *  @return Returns E_OK on success
 */
enum result fsWrite(void *file, const uint8_t *buffer, uint32_t length,
    uint32_t *result)
{
  return ((struct FsFileClass *)CLASS(file))->write(file, buffer, length,
      result);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/** Close recently opened directory handler
 *  @param dir Pointer to FsDir object
 */
void fsCloseDir(void *dir)
{
  ((struct FsDirClass *)CLASS(dir))->close(dir);
  deinit(dir);
}
/*----------------------------------------------------------------------------*/
/* bool (*fsEofDir)(void *); TODO */
/*----------------------------------------------------------------------------*/
/** Read next directory entry
 *  @param dir Pointer to FsDir object
 *  @param buffer Pointer to the buffer where information will be placed
 */
enum result fsReadDir(void *dir, char *buffer)
{
  return ((struct FsDirClass *)CLASS(dir))->read(dir, buffer);
}
/*----------------------------------------------------------------------------*/
/* enum result fsSeekDir(void *, uint32_t); TODO */
/*----------------------------------------------------------------------------*/
/* uint32_t fsTellDir(void *); TODO */
