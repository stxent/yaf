#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "bdev.h"
#include "mmi.h"
/*----------------------------------------------------------------------------*/
/* TODO add buffering */
#define FS_BUFFER (SECTOR_SIZE * 1)
/*----------------------------------------------------------------------------*/
enum ifResult mmdOpen(struct BlockDevice *, struct Interface *, uint8_t *);
void mmdClose(struct BlockDevice *);
/*----------------------------------------------------------------------------*/
enum ifResult mmdReadTable(struct BlockDevice *, uint32_t, uint8_t);
uint8_t mmdGetType(struct BlockDevice *);
/*----------------------------------------------------------------------------*/
#endif
