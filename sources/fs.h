/*
 * fs.h
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

/**
 * @file
 * Abstract filesystem interface for embedded applications.
 */

#ifndef FS_H_
#define FS_H_
/*----------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <entity.h>
#include <error.h>
/*----------------------------------------------------------------------------*/
#define FS_NAME_LENGTH 128
/*----------------------------------------------------------------------------*/
/* Type which represents address space size */
// typedef int64_t address_t; //FIXME
typedef int64_t time_t; //FIXME
/*----------------------------------------------------------------------------*/
typedef uint8_t access_t;
/*----------------------------------------------------------------------------*/
enum
{
  FS_ACCESS_READ = 0x01,
  FS_ACCESS_WRITE = 0x02
};
/*----------------------------------------------------------------------------*/
// enum fsNodeOption //FIXME
// {
//   /** Type of the node. */
//   FS_NODE_TYPE = 0,
//   /** Symbolic name of the node. */
//   FS_NODE_NAME,
//   /** Node size in bytes or elements. */
//   FS_NODE_SIZE,
//   /** Access rights to the node available for user. */
//   FS_NODE_ACCESS,
//   /** Owner of the node. */
//   FS_NODE_OWNER,
//   /** Node change time. */
//   FS_NODE_TIME
// };
/*----------------------------------------------------------------------------*/
enum fsNodeType
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
struct FsMetadata
{
  uint64_t size;
  time_t time;
  char name[FS_NAME_LENGTH];
  access_t access;
  enum fsNodeType type;

#ifdef DEBUG //FIXME
  uint32_t cluster;
  uint32_t pcluster;
  uint16_t pindex;
#endif
};
/*----------------------------------------------------------------------------*/
struct FsHandleClass
{
  CLASS_HEADER

  void *(*allocate)(void *);
  void *(*follow)(void *, const char *, const void *);
};
/*----------------------------------------------------------------------------*/
struct FsHandle
{
  struct Entity parent;
};
/*----------------------------------------------------------------------------*/
struct FsNodeClass
{
  CLASS_HEADER

  void (*free)(void *);
  enum result (*get)(void *, struct FsMetadata *);
  enum result (*make)(void *, const struct FsMetadata *, void *);
  void *(*open)(void *, access_t);
  enum result (*remove)(void *);
  enum result (*set)(void *, const struct FsMetadata *);
  enum result (*truncate)(void *);
};
/*----------------------------------------------------------------------------*/
struct FsNode
{
  struct Entity parent;
};
/*----------------------------------------------------------------------------*/
struct FsEntryClass
{
  CLASS_HEADER

  enum result (*close)(void *);
  bool (*end)(void *);
  enum result (*fetch)(void *, void *);
  uint32_t (*read)(void *, uint8_t *, uint32_t);
  enum result (*rewind)(void *);
  enum result (*seek)(void *, uint64_t, enum fsSeekOrigin);
  enum result (*sync)(void *);
  uint64_t (*tell)(void *);
  uint32_t (*write)(void *, const uint8_t *, uint32_t);
};
/*----------------------------------------------------------------------------*/
struct FsEntry
{
  struct Entity parent;
};
/*----------------------------------------------------------------------------*/
/**
 * Allocate a node for further use.
 * @param handle Pointer to an FsHandle object.
 * @return Pointer to an allocated node on success or zero otherwise.
 */
static inline void *fsAllocate(void *handle)
{
  return ((struct FsHandleClass *)CLASS(handle))->allocate(handle);
}
/*----------------------------------------------------------------------------*/
/**
 * Free the memory allocated for a node.
 * @param node Pointer to a previously allocated FsNode object.
 */
static inline void fsFree(void *node)
{
  ((struct FsNodeClass *)CLASS(node))->free(node);
}
/*----------------------------------------------------------------------------*/
/**
 * Follow symbolic path and get node pointing to this entry.
 * @param handle Pointer to an FsHandle object.
 * @param path Path to an entry to be located.
 * @param root Local root entry. Zero value may be used to rewind search and
 * follow the path from global root entry.
 * @return Pointer to an allocated and filled node on sucess or zero otherwise.
 * Allocated node may belong to other handle when current one
 * has nested partitions.
 */
static inline void *fsFollow(void *handle, const char *path, const void *root)
{
  return ((struct FsHandleClass *)CLASS(handle))->follow(handle, path, root);
}
/*----------------------------------------------------------------------------*/
/**
 * Load information about the node.
 * @param node The node with information about entry location.
 * @param metadata Pointer to an entry information structure to be filled.
 * @return @b E_OK on success.
 */
static inline enum result fsGet(void *node, struct FsMetadata *metadata)
{
  return ((struct FsNodeClass *)CLASS(node))->get(node, metadata);
}
/*----------------------------------------------------------------------------*/
/**
 * Create a new entry.
 * @param node The node pointing to a location where new entry should be placed.
 * @param metadata Pointer to an entry information.
 * @param target Pointer to a previously allocated node where the information
 * about newly created entry should be stored. May be left zero when such
 * information is not needed.
 * @return E_OK on success.
 */
static inline enum result fsMake(void *node, const struct FsMetadata *metadata,
    void *target)
{
  return ((struct FsNodeClass *)CLASS(node))->make(node, metadata, target);
}
/*----------------------------------------------------------------------------*/
/**
 * Open an entry.
 * @param node The node with information about entry location.
 * @param access Requested access rights.
 * @return Pointer to a newly allocated entry on success or zero on error.
 */
static inline void *fsOpen(void *node, access_t access)
{
  return ((struct FsNodeClass *)CLASS(node))->open(node, access);
}
/*----------------------------------------------------------------------------*/
/**
 * Remove entry information but preserve data.
 * @param node The node with information about entry location.
 * @return @b E_OK on success.
 */
static inline enum result fsRemove(void *node)
{
  return ((struct FsNodeClass *)CLASS(node))->remove(node);
}
/*----------------------------------------------------------------------------*/
/**
 * Modify information about the node.
 * @param metadata Pointer to an entry information structure to be saved.
 * @return @b E_OK on success.
 */
static inline enum result fsSet(void *node, const struct FsMetadata *metadata)
{
  return ((struct FsNodeClass *)CLASS(node))->set(node, metadata);
}
/*----------------------------------------------------------------------------*/
/**
 * Truncate node data.
 * @param node The node with information about entry location.
 * @return E_OK on success.
 */
static inline enum result fsTruncate(void *node)
{
  return ((struct FsNodeClass *)CLASS(node))->truncate(node);
}
/*----------------------------------------------------------------------------*/
/**
 * Close entry and free descriptor data.
 * @param entry Pointer to a previously opened entry.
 */
static inline enum result fsClose(void *entry)
{
  return ((struct FsEntryClass *)CLASS(entry))->close(entry);
}
/*----------------------------------------------------------------------------*/
/**
 * Check whether a position in entry reached the end of entry.
 * @param entry Pointer to a previously opened entry.
 * @return @b true when reached the end of entry or @b false otherwise.
 */
static inline bool fsEnd(void *entry)
{
  return ((struct FsEntryClass *)CLASS(entry))->end(entry);
}
/*----------------------------------------------------------------------------*/
/**
 * Read next directory entry.
 * @param entry Pointer to a previously opened entry.
 * @param node Pointer to a previously allocated node where the information
 * about next entry should be stored.
 * @return E_OK on success.
 */
static inline enum result fsFetch(void *entry, void *node)
{
  return ((struct FsEntryClass *)CLASS(entry))->fetch(entry, node);
}
/*----------------------------------------------------------------------------*/
/**
 * Read block of data from the entry.
 * @param entry Pointer to a previously opened entry.
 * @param buffer Pointer to a buffer with a size of at least @b length bytes.
 * @param length Number of data bytes to be read.
 * @return E_OK on success.
 */
static inline uint32_t fsRead(void *entry, uint8_t *buffer, uint32_t length)
{
  return ((struct FsEntryClass *)CLASS(entry))->read(entry, buffer, length);
}
/*----------------------------------------------------------------------------*/
/**
 * Move directory pointer to a first entry.
 * @param entry Pointer to a previously opened entry.
 * @return E_OK on success.
 */
static inline enum result fsRewind(void *entry)
{
  return ((struct FsEntryClass *)CLASS(entry))->rewind(entry);
}
/*----------------------------------------------------------------------------*/
/**
 * Set the position in the entry to a new position.
 * @param entry Pointer to a previously opened entry.
 * @param offset Number of bytes to offset from origin.
 * @param origin Position used as reference for the offset.
 * @return E_OK on success.
 */
static inline enum result fsSeek(void *entry, uint64_t offset,
    enum fsSeekOrigin origin)
{
  return ((struct FsEntryClass *)CLASS(entry))->seek(entry, offset, origin);
}
/*----------------------------------------------------------------------------*/
/**
 * Write changed entry information to physical device.
 * @param entry Pointer to a previously opened entry.
 */
static inline enum result fsSync(void *entry)
{
  return ((struct FsEntryClass *)CLASS(entry))->sync(entry);
}
/*----------------------------------------------------------------------------*/
/**
 * Get the current position in entry.
 * @param entry Pointer to a previously opened entry.
 * @return Position in a file.
 */
static inline uint64_t fsTell(void *entry)
{
  return ((struct FsEntryClass *)CLASS(entry))->tell(entry);
}
/*----------------------------------------------------------------------------*/
/**
 * Write block of data to entry.
 * @param entry Pointer to a previously opened entry.
 * @param buffer Pointer to a buffer with a size of at least @b length bytes.
 * @param length Number of data bytes to be written.
 * @return E_OK on success.
 */
static inline uint32_t fsWrite(void *entry, const uint8_t *buffer,
    uint32_t length)
{
  return ((struct FsEntryClass *)CLASS(entry))->write(entry, buffer, length);
}
/*----------------------------------------------------------------------------*/
#endif /* FS_H_ */
