/*
 * shell/mmi.h
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_SHELL_MMI_H_
#define YAF_SHELL_MMI_H_
/*----------------------------------------------------------------------------*/
#include <xcore/interface.h>
/*----------------------------------------------------------------------------*/
extern const struct InterfaceClass * const Mmi;
/*----------------------------------------------------------------------------*/
struct MbrDescriptor
{
  uint32_t offset;
  uint32_t size;
  uint8_t type;
};
/*----------------------------------------------------------------------------*/
enum Result mmiSetPartition(void *, struct MbrDescriptor *);
enum Result mmiReadTable(void *, uint32_t, uint8_t, struct MbrDescriptor *);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MMI_STATUS
void mmiGetStatus(void *, uint64_t *);
#endif
/*----------------------------------------------------------------------------*/
#endif /* YAF_SHELL_MMI_H_ */
