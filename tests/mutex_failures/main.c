/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "default_fs.h"
#include "virtual_mem.h"
#include <yaf/fat32.h>
#include <yaf/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
extern int __pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);

static unsigned int pthreadHookFails = 0;

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
    const pthread_mutexattr_t *restrict attr)
{
  if (pthreadHookFails)
  {
    if (--pthreadHookFails)
      return __pthread_mutex_init(mutex, attr);
    else
      return -1;
  }
  else
    return __pthread_mutex_init(mutex, attr);
}
/*----------------------------------------------------------------------------*/
START_TEST(testMutexInitError)
{
  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);
  enum Result res;

  static const struct Fat32FsConfig makeFsConfig =  {
      .clusterSize = FS_CLUSTER_SIZE,
      .tableCount = FS_TABLE_COUNT,
      .label = "TEST"
  };
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert(res == E_OK);

  const struct Fat32Config fsConfig = {
      .interface = vmem,
      .nodes = FS_NODE_POOL_SIZE,
      .threads = FS_THREAD_POOL_SIZE
  };
  struct FsHandle *handle;

  /* Mount successfully */
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_nonnull(handle);
  deinit(handle);

  /* Mutex 1 failure */
  pthreadHookFails = 1;
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);

  /* Mutex 2 failure */
  pthreadHookFails = 2;
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);

  /* Release all resources */
  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("MutexFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testMutexInitError);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
