/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "default_fs.h"
#include "helpers.h"
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testNodeHead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenNode(context.handle, PATH_HOME_USER);
  ck_assert_ptr_nonnull(parent);

  PointerQueue nodes = drainNodePool(context.handle);

  struct FsNode * const node = fsNodeHead(parent);
  ck_assert_ptr_null(node);

  /* Release all resources */
  restoreNodePool(context.handle, &nodes);
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testRootNode)
{
  struct TestContext context = makeTestHandle();
  PointerQueue nodes = drainNodePool(context.handle);

  struct FsNode * const node = fsHandleRoot(context.handle);
  ck_assert_ptr_null(node);

  /* Release all resources */
  restoreNodePool(context.handle, &nodes);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("NodeFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testNodeHead);
  tcase_add_test(testcase, testRootNode);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
