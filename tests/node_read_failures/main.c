/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <xcore/realtime.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testAuxStreamErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  char buffer[MAX_BUFFER_LENGTH];
  time64_t timestamp;
  enum Result res;

  /* Try to read node name */
  vmemAddRegion(context.interface,
      vmemExtractDataRegion(context.interface));
  res = fsNodeRead(node, FS_NODE_NAME, 0, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);
    vmemClearRegions(context.interface);

  /* Try to read node time */
  vmemAddRegion(context.interface,
      vmemExtractDataRegion(context.interface));
  res = fsNodeRead(node, FS_NODE_TIME, 0, &timestamp, sizeof(timestamp), 0);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDataReadErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddRegion(context.interface,
      vmemExtractDataRegion(context.interface));

  char buffer[MAX_BUFFER_LENGTH];
  enum Result res;

  /* Aligned read */
  res = fsNodeRead(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer), buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Unaligned read */
  res = fsNodeRead(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer) * 3 / 2, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Seek error */
  vmemAddRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0));
  res = fsNodeRead(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer), buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Address setup allowed, but reading is forbidden */
  vmemClearRegions(context.interface);
  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), false, false, true);

  /* Aligned read */
  res = fsNodeRead(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer), buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Unaligned read */
  res = fsNodeRead(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer) * 3 / 2, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Restore access */
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testSequentialReadErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  FsLength position = 0;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  for (; position < FS_CLUSTER_SIZE; position += sizeof(buffer))
  {
    res = fsNodeRead(node, FS_NODE_DATA, position, buffer, sizeof(buffer), 0);
    ck_assert_uint_eq(res, E_OK);
  }

  vmemAddRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0));
  res = fsNodeRead(node, FS_NODE_DATA, position, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("NodeReadFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAuxStreamErrors);
  tcase_add_test(testcase, testDataReadErrors);
  tcase_add_test(testcase, testSequentialReadErrors);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
