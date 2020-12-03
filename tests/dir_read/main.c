/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testAuxStreams)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, "/");
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  res = fsNodeRead(node, FS_TYPE_END, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeRead(node, FS_NODE_NAME, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, 1);
  ck_assert_str_eq(buffer, "");

  res = fsNodeRead(node, FS_NODE_DATA, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testIteration)
{
  static const char path[] = PATH_HOME_USER "/NONE.TXT";

  struct TestContext context = makeTestHandle();
  struct FsNode *node;
  struct FsNode *parent;
  enum Result res;

  /* Open non-existent node */
  node = fsOpenNode(context.handle, path);
  ck_assert_ptr_null(node);

  /* Try to read directory entries from regular file */
  parent = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(parent);
  node = fsNodeHead(parent);
  ck_assert_ptr_null(node);
  fsNodeFree(parent);

  /* Iterate using root node */
  parent = fsOpenNode(context.handle, "/");
  ck_assert_ptr_nonnull(parent);
  res = fsNodeNext(parent);
  ck_assert_uint_ne(res, E_OK);
  fsNodeFree(parent);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testLength)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, "/");
  ck_assert_ptr_nonnull(node);

  FsLength count;
  enum Result res;

  res = fsNodeLength(node, FS_NODE_DATA, &count);
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeLength(node, FS_NODE_NAME, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, 1);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("DirRead");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAuxStreams);
  tcase_add_test(testcase, testIteration);
  tcase_add_test(testcase, testLength);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
