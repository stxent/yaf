/*
 * yaf/tests/shared/virtual_mem.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_TESTS_SHARED_VIRTUAL_MEM_H_
#define YAF_TESTS_SHARED_VIRTUAL_MEM_H_
/*----------------------------------------------------------------------------*/
#include <xcore/interface.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
extern const struct InterfaceClass * const VirtualMem;

enum
{
  /* Data reading */
  VMEM_R = 0x01,
  /* Data writing */
  VMEM_W = 0x02,
  /* Data addressing */
  VMEM_A = 0x04
};

struct VirtualMemConfig
{
  /** Mandatory: region size. */
  size_t size;
};

struct VirtualMemRegion
{
  uint64_t begin;
  uint64_t end;
  uint8_t flags;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

uint8_t *vmemGetAddress(void *);

void vmemAddMarkedRegion(void *, struct VirtualMemRegion, bool, bool, bool);
void vmemAddRegion(void *, struct VirtualMemRegion);
void vmemClearRegions(void *);
void vmemSetMatchCounter(void *, unsigned int);

struct VirtualMemRegion vmemExtractBootRegion(void);
struct VirtualMemRegion vmemExtractDataRegion(const void *);
struct VirtualMemRegion vmemExtractInfoRegion(void);
struct VirtualMemRegion vmemExtractNodeDataRegion(const void *, const void *,
    size_t);
struct VirtualMemRegion vmemExtractRootDataRegion(const void *);
struct VirtualMemRegion vmemExtractTableRegion(const void *, size_t);
struct VirtualMemRegion vmemExtractTableSectorRegion(const void *, size_t,
    size_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_TESTS_SHARED_VIRTUAL_MEM_H_ */
