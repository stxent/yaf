#ifndef _SPI_H_
#define _SPI_H_
//---------------------------------------------------------------------------
#include <stdint.h>
#include "settings.h"
//---------------------------------------------------------------------------
#define BLOCK_SIZE_POW 9
//#pragma pack(1)
//---------------------------------------------------------------------------
struct sDevice
{
  uint8_t *buffer;
  uint8_t type;
  uint32_t offset;
  uint32_t size;
};
//---------------------------------------------------------------------------
uint8_t sOpen(struct sDevice *, uint8_t *, const char *);
uint8_t sClose(struct sDevice *);
uint8_t sReadTable(struct sDevice *, uint32_t, uint8_t);
uint8_t sRead(struct sDevice *, uint8_t *, uint32_t, uint8_t);
uint8_t sWrite(struct sDevice *, const uint8_t *, uint32_t, uint8_t);
//---------------------------------------------------------------------------
#endif
