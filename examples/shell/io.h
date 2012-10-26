#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "fs.h"
#include "bdev.h"
#include "mmi.h"
/*----------------------------------------------------------------------------*/
enum fsResult mmdOpen(struct FsDevice *, struct Interface *, uint8_t *);
void mmdClose(struct FsDevice *);
/*----------------------------------------------------------------------------*/
enum fsResult mmdReadTable(struct FsDevice *, uint32_t, uint8_t);
/*----------------------------------------------------------------------------*/
#endif
