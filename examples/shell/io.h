#ifndef IO_H_
#define IO_H_
//---------------------------------------------------------------------------
#include "fat.h"
//---------------------------------------------------------------------------
uint8_t sOpen(struct FsDevice *, uint8_t *, const char *);
uint8_t sClose(struct FsDevice *);
uint8_t sReadTable(struct FsDevice *, uint32_t, uint8_t);
enum fsResult sRead(struct FsDevice *, uint8_t *, uint32_t, uint8_t);
enum fsResult sWrite(struct FsDevice *, const uint8_t *, uint32_t, uint8_t);
//---------------------------------------------------------------------------
#endif
