#ifndef IO_H_
#define IO_H_
/*----------------------------------------------------------------------------*/
#include "bdev.h"
#include "mmi.h"
/*----------------------------------------------------------------------------*/
struct mmdConfig
{
  struct Interface *stream;
};
/*----------------------------------------------------------------------------*/
extern const struct BlockInterfaceClass *Mmd;
/*----------------------------------------------------------------------------*/
enum result mmdReadTable(struct BlockInterface *, uint32_t, uint8_t);
uint8_t mmdGetType(struct BlockInterface *);
/*----------------------------------------------------------------------------*/
#endif
