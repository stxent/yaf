/*
 * mmi.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef MMI_H_
#define MMI_H_
/*----------------------------------------------------------------------------*/
#include <interface.h>
/*----------------------------------------------------------------------------*/
extern const struct InterfaceClass * const Mmi;
/*----------------------------------------------------------------------------*/
struct MbrDescriptor
{
  uint8_t type;
  uint32_t offset, size;
};
/*----------------------------------------------------------------------------*/
enum result mmiSetPartition(void *, struct MbrDescriptor *);
enum result mmiReadTable(void *, uint32_t, uint8_t, struct MbrDescriptor *);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MMI_STATUS
void mmiGetStatus(void *, uint64_t *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* MMI_H_ */
