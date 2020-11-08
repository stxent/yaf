/*
 * yaf/tests/shared/helpers.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef YAF_TESTS_SHARED_HELPERS_H_
#define YAF_TESTS_SHARED_HELPERS_H_
/*----------------------------------------------------------------------------*/
#include <yaf/pointer_queue.h>
#include <xcore/helpers.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void changeLastAllocatedCluster(void *, uint32_t);
void changeLfnCount(void *, uint8_t);
PointerQueue drainContextPool(void *);
PointerQueue drainNodePool(void *);
size_t getMaxEntriesPerCluster(const void *);
size_t getMaxEntriesPerSector(void);
size_t getMaxSimilarNamesCount(void);
size_t getTableEntriesPerSector(void);
void restoreContextPool(void *, PointerQueue *);
void restoreNodePool(void *, PointerQueue *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_TESTS_SHARED_HELPERS_H_ */
