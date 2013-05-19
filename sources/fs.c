/*
 * fs.c
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "fs.h"
/*----------------------------------------------------------------------------*/
/**
 * Read block of data from physical device.
 * @param interface Pointer to an Interface object.
 * @param address Address on the physical device in bytes.
 * @param buffer Pointer to a block of memory with a size of @b size bytes.
 * @param size Number of bytes to be read.
 * @return E_OK on success.
 */
enum result fsBlockRead(void *interface, asize_t address,
    uint8_t *buffer, uint32_t length)
{
  if (ifSet(interface, IF_ADDRESS, &address) != E_OK)
    return E_DEVICE;
  if (ifRead(interface, buffer, length) != length)
    return E_INTERFACE;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
/**
 * Write block of data to physical device.
 * @param interface Pointer to an Interface object.
 * @param address Address on the physical device in bytes.
 * @param buffer Pointer to a block of memory with a size of @b size bytes.
 * @param size Number of bytes to be written.
 * @return E_OK on success.
 */
enum result fsBlockWrite(void *interface, asize_t address,
    const uint8_t *buffer, uint32_t size)
{
  if (ifSet(interface, IF_ADDRESS, &address) != E_OK)
    return E_ERROR;
  if (ifWrite(interface, buffer, size) != size)
    return E_ERROR;
  return E_OK;
}
/*----------------------------------------------------------------------------*/
/**
 * Move or rename filesystem entry.
 * @param handle Pointer to an FsHandle object.
 * @param src Source path.
 * @param dest Destination path.
 * @return E_OK on success.
 */
enum result fsMove(void *handle, const char *src, const char *dest)
{
  return ((struct FsHandleClass *)CLASS(handle))->move(handle, src, dest);
}
/*----------------------------------------------------------------------------*/
/**
 * Read filesystem entry information.
 * @param handle Pointer to an FsHandle object.
 * @param result Pointer to an FsStat object to store information.
 * @param path Path to a file.
 * @return E_OK on success.
 */
enum result fsStat(void *handle, struct FsStat *result, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->stat(handle, result, path);
}
/*----------------------------------------------------------------------------*/
/**
 * Open a file for read, write or append operations.
 * Function opens a file and associates an FsFile object with it.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to a file to be opened.
 * @param mode Type of access to file.
 * @return Pointer to an FsFile object on success, or zero on error.
 */
void *fsOpen(void *handle, const char *path, enum fsMode mode)
{
  return ((struct FsHandleClass *)CLASS(handle))->open(handle, path, mode);
}
/*----------------------------------------------------------------------------*/
/**
 * Remove a specified file.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to a file to be removed.
 * @return E_OK on success.
 */
enum result fsRemove(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->remove(handle, path);
}
/*----------------------------------------------------------------------------*/
/**
 * Open a directory.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to a directory to be opened.
 * @return Pointer to an FsDir object on success, or zero on error.
 */
void *fsOpenDir(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->openDir(handle, path);
}
/*----------------------------------------------------------------------------*/
/**
 * Create directory, if it does not already exist.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to a new directory.
 * @return E_OK on success.
 */
enum result fsMakeDir(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->makeDir(handle, path);
}
/*----------------------------------------------------------------------------*/
/**
 * Remove empty directory.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to a directory.
 * @return E_OK on success.
 */
enum result fsRemoveDir(void *handle, const char *path)
{
  return ((struct FsHandleClass *)CLASS(handle))->removeDir(handle, path);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/**
 * Close file and free file descriptor data.
 * @param file Pointer to an FsFile object.
 */
void fsClose(void *file)
{
  ((struct FsFileClass *)CLASS(file))->close(file);
}
/*----------------------------------------------------------------------------*/
/**
 * Check whether a position in file reached the end of file.
 * @param file Pointer to an FsFile object.
 * @return @b true when reached the end of file or @b false otherwise.
 */
bool fsEof(void *file)
{
  return ((struct FsFileClass *)CLASS(file))->eof(file);
}
/*----------------------------------------------------------------------------*/
/**
 * Write changed file information to physical device.
 * @param file Pointer to an FsFile object.
 * @return E_OK on success.
 */
enum result fsFlush(void *file)
{
  return ((struct FsFileClass *)CLASS(file))->flush(file);
}
/*----------------------------------------------------------------------------*/
/**
 * Read block of data from file.
 * @param file Pointer to an FsFile object.
 * @param buffer Pointer to a buffer with a size of at least @b length bytes.
 * @param length Number of data bytes to be read.
 * @param result Pointer to value where total number of bytes successfully
 * read will be stored.
 * @return E_OK on success.
 */
uint32_t fsRead(void *file, uint8_t *buffer, uint32_t length)
{
  return ((struct FsFileClass *)CLASS(file))->read(file, buffer, length);
}
/*----------------------------------------------------------------------------*/
/**
 * Set the position in file to a new position.
 * @param file Pointer to an FsFile object.
 * @param offset Number of bytes to offset from origin.
 * @param origin Position used as reference for the offset.
 * @return E_OK on success.
 */
enum result fsSeek(void *file, asize_t offset, enum fsSeekOrigin origin)
{
  return ((struct FsFileClass *)CLASS(file))->seek(file, offset, origin);
}
/*----------------------------------------------------------------------------*/
/**
 * Get the current position in file.
 * @param file Pointer to an FsFile object.
 * @return Position in a file.
 */
asize_t fsTell(void *file)
{
  return ((struct FsFileClass *)CLASS(file))->tell(file);
}
/*----------------------------------------------------------------------------*/
/**
 * Write block of data to file.
 * @param file Pointer to an FsFile object.
 * @param buffer Pointer to a buffer with a size of at least @b length bytes.
 * @param length Number of data bytes to be written.
 * @param result Pointer to value where total number of bytes successfully
 * written will be stored.
 * @return E_OK on success.
 */
uint32_t fsWrite(void *file, const uint8_t *buffer, uint32_t length)
{
  return ((struct FsFileClass *)CLASS(file))->write(file, buffer, length);
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/**
 * Close directory and free directory descriptor data.
 * @param dir Pointer to an FsDir object.
 */
void fsCloseDir(void *dir)
{
  ((struct FsDirClass *)CLASS(dir))->close(dir);
}
/*----------------------------------------------------------------------------*/
/**
 * Read next directory entry.
 * @param dir Pointer to an FsDir object.
 * @param buffer Pointer to a buffer where entry name will be placed.
 */
enum result fsReadDir(void *dir, char *buffer)
{
  return ((struct FsDirClass *)CLASS(dir))->read(dir, buffer);
}
