/*
 * yaf/pointer_array.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_POINTER_ARRAY_H_
#define YAF_POINTER_ARRAY_H_
/*----------------------------------------------------------------------------*/
#include <xcore/containers/tg_array.h>
/*----------------------------------------------------------------------------*/
DEFINE_ARRAY(void *, Pointer, pointer)

void pointerArrayEraseBySwap(PointerArray *, size_t);
/*----------------------------------------------------------------------------*/
#endif /* YAF_POINTER_ARRAY_H_ */
