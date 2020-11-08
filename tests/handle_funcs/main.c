/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "default_fs.h"
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testHandleSync)
{
  static const char data[MAX_BUFFER_LENGTH] = {0};

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  enum Result res;

  /* Write and sync handle */
  res = fsNodeWrite(node, FS_NODE_DATA, 0, data, MAX_BUFFER_LENGTH, 0);
  ck_assert_uint_eq(res, E_OK);
  res = fsHandleSync(context.handle);
  ck_assert_uint_eq(res, E_OK);

  /* Write and delete node */
  res = fsNodeWrite(node, FS_NODE_DATA, 0, data, MAX_BUFFER_LENGTH, 0);
  ck_assert_uint_eq(res, E_OK);
  fsNodeFree(node);

  /* Redundant sync, all nodes have been already closed */
  res = fsHandleSync(context.handle);
  ck_assert_uint_eq(res, E_OK);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("HandleFuncs");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testHandleSync);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
