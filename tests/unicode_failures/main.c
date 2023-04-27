/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <yaf/fat32.h>
#include <xcore/fs/utils.h>
#include <check.h>
/*----------------------------------------------------------------------------*/
START_TEST(testLongNameErrors)
{
  static const char path0[] = PATH_HOME_USER "/image.jpg";
  static const char path1[] = PATH_HOME_USER "/padding_padding.jpg";

  struct TestContext context = makeTestHandle();

  makeNode(context.handle, path0, false, false);
  makeNode(context.handle, path1, false, false);

  struct FsNode * const node0 = fsOpenNode(context.handle, path0);
  ck_assert_ptr_nonnull(node0);
  struct FsNode * const node1 = fsOpenNode(context.handle, path1);
  ck_assert_ptr_nonnull(node1);

  char buffer[MAX_BUFFER_LENGTH];
  enum Result res;

  changeLfnCount(node0, 2);
  res = fsNodeRead(node0, FS_NODE_NAME, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ENTRY);

  changeLfnCount(node1, 1);
  res = fsNodeRead(node1, FS_NODE_NAME, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_MEMORY);

  changeLfnCount(node1, 127);
  res = fsNodeRead(node1, FS_NODE_NAME, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_MEMORY);

  /* Release all resources */
  fsNodeFree(node1);
  fsNodeFree(node0);
  freeNode(context.handle, path1);
  freeNode(context.handle, path0);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("UnicodeFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testLongNameErrors);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
