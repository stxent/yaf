/*
 * yaf/pointer_queue.h
 * Copyright (C) 2019 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_POINTER_QUEUE_H_
#define YAF_POINTER_QUEUE_H_
/*----------------------------------------------------------------------------*/
#include <xcore/containers/tg_queue.h>
#include <xcore/helpers.h>
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

DEFINE_QUEUE(void *, Pointer, pointer)

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_POINTER_QUEUE_H_ */
