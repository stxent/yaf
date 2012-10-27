#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "fs.h"
#include "bdev.h"
#include "mmi.h"
/*----------------------------------------------------------------------------*/
enum ifResult mmdOpen(struct BlockDevice *, struct Interface *, uint8_t *);
void mmdClose(struct BlockDevice *);
/*----------------------------------------------------------------------------*/
enum ifResult mmdReadTable(struct BlockDevice *, uint32_t, uint8_t);
/*----------------------------------------------------------------------------*/
#endif
