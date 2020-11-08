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
START_TEST(testDirOpsFailure)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const root = fsOpenNode(context.handle, "/");
  ck_assert_ptr_nonnull(root);
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER);
  ck_assert_ptr_nonnull(node);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);

  struct FsNode * const head = fsNodeHead(root);
  ck_assert_ptr_null(head);

  const enum Result res = fsNodeNext(node);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  fsNodeFree(node);
  fsNodeFree(root);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testHandleSyncFailure)
{
  struct TestContext context = makeTestHandle();
  PointerQueue contexts = drainContextPool(context.handle);

  const enum Result res = fsHandleSync(context.handle);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeCreationFailure)
{
  static const char path[] = PATH_HOME_USER_TEMP1;

  const char * const name = fsExtractName(path);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);

  const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeReadingFailure)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);
  char data[MAX_BUFFER_LENGTH];

  const enum Result res = fsNodeRead(node, FS_NODE_DATA, 0,
      data, sizeof(data), 0);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeRemovingFailure)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle,
      PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(parent);
  struct FsNode * const node = fsOpenNode(context.handle,
      PATH_HOME_ROOT_ALIG);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);

  const enum Result res = fsNodeRemove(parent, node);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  fsNodeFree(node);
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeWritingFailure)
{
  static const char data[MAX_BUFFER_LENGTH] = {0};

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  /* Simulate context allocation error */
  PointerQueue contexts = drainContextPool(context.handle);

  const enum Result res = fsNodeWrite(node, FS_NODE_DATA, 0,
      data, sizeof(data), 0);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  restoreContextPool(context.handle, &contexts);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("ContextFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testDirOpsFailure);
  tcase_add_test(testcase, testHandleSyncFailure);
  tcase_add_test(testcase, testNodeCreationFailure);
  tcase_add_test(testcase, testNodeReadingFailure);
  tcase_add_test(testcase, testNodeRemovingFailure);
  tcase_add_test(testcase, testNodeWritingFailure);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
