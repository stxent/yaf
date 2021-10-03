/*
 * main.c
 * Copyright (C) 2021 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testUsedSpaceCalculation)
{
  static const FsSpace TOTAL_SPACE_USED =
      7 * FS_CLUSTER_SIZE + ALIG_FILE_SIZE
      + ((UNALIG_FILE_SIZE + FS_CLUSTER_SIZE - 1) & ~(FS_CLUSTER_SIZE - 1));

  struct TestContext context = makeTestHandle();
  FsSpace used;

  used = fsFindUsedSpace(context.handle, 0);
  ck_assert_uint_eq(used, TOTAL_SPACE_USED);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);
  used = fsFindUsedSpace(context.handle, 0);
  ck_assert_uint_eq(used, 0);
  restoreContextPool(context.handle, &contexts);

  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("HandleUsage");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testUsedSpaceCalculation);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
