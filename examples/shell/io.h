#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "fat.h"
/*----------------------------------------------------------------------------*/
enum fsResult sOpen(struct FsDevice *, uint8_t *, const char *);
enum fsResult sClose(struct FsDevice *);
enum fsResult sReadTable(struct FsDevice *, uint32_t, uint8_t);
enum fsResult sRead(struct FsDevice *, uint32_t, uint8_t *, uint8_t);
enum fsResult sWrite(struct FsDevice *, uint32_t, const uint8_t *, uint8_t);
/*----------------------------------------------------------------------------*/
#endif
