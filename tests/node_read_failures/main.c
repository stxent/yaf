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
  res = fsNodeRead(node, FS_NODE_NAME, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ADDRESS);
    vmemClearRegions(context.interface);

  /* Try to read node time */
  vmemAddRegion(context.interface,
      vmemExtractDataRegion(context.interface));
  res = fsNodeRead(node, FS_NODE_TIME, 0, &timestamp, sizeof(timestamp),
      NULL);
  ck_assert_uint_eq(res, E_ADDRESS);
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

  /* Read to zero pointer */
  res = fsNodeRead(node, FS_NODE_DATA, 0, NULL, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_VALUE);

  /* Aligned read */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer),
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ADDRESS);

  /* Unaligned read */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer) * 3 / 2,
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ADDRESS);

  /* Seek error */
  vmemAddRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0));
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer),
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ADDRESS);

  /* Address setup allowed, but reading is forbidden */
  vmemClearRegions(context.interface);
  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), false, false, true);

  /* Aligned read */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer),
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_INTERFACE);

  /* Unaligned read */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer) * 3 / 2,
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_INTERFACE);

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
    res = fsNodeRead(node, FS_NODE_DATA, position, buffer, sizeof(buffer),
        NULL);
    ck_assert_uint_eq(res, E_OK);
  }

  vmemAddRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0));
  res = fsNodeRead(node, FS_NODE_DATA, position, buffer, sizeof(buffer),
      NULL);
  ck_assert_uint_eq(res, E_ADDRESS);
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
  const int failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
