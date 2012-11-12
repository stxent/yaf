#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "bdev.h"
#include "mmi.h"
/*----------------------------------------------------------------------------*/
enum result mmdInit(struct BlockDevice *, struct Interface *);
/*----------------------------------------------------------------------------*/
enum result mmdReadTable(struct BlockDevice *, uint32_t, uint8_t);
uint8_t mmdGetType(struct BlockDevice *);
/*----------------------------------------------------------------------------*/
#endif
