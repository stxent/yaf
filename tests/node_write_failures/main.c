/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
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

  vmemAddRegion(context.interface,
      vmemExtractDataRegion(context.interface));

  FsAccess access = FS_ACCESS_READ;
  time64_t timestamp = 0;
  enum Result res;

  /* Try to write node access */
  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &access, sizeof(access), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Try to write node time */
  res = fsNodeWrite(node, FS_NODE_TIME, 0, &timestamp, sizeof(timestamp), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Restore access */
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testClusterAllocationErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  static const char buffer[MAX_BUFFER_LENGTH] = {0};
  enum Result res;

  changeLastAllocatedCluster(context.handle, getTableEntriesPerSector() - 1);
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableSectorRegion(context.interface, 0, 0), true, false, true);
  res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDataWriteErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), true, false, true);

  static const char buffer[MAX_BUFFER_LENGTH] = {0};
  enum Result res;

  /* Aligned write */
  res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE, buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Unaligned write */
  res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE, buffer, sizeof(buffer) / 2, 0);
  ck_assert_uint_ne(res, E_OK);

  /* Unaligned write error during sector read */
  vmemClearRegions(context.interface);
  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), false, true, true);
  res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE, buffer, sizeof(buffer) / 2, 0);
  ck_assert_uint_ne(res, E_OK);

  /* Seek error */
  vmemClearRegions(context.interface);
  vmemAddRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0));
  res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE - sizeof(buffer), buffer, sizeof(buffer), 0);
  ck_assert_uint_ne(res, E_OK);

  /* Restore access */
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeMaxLength)
{
  static const char buffer = 0;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddRegion(context.interface, vmemExtractDataRegion(context.interface));

  /*
   * Test chunk truncation. The requested length is much greater than
   * the size of the buffer, but there will be no segmentation fault,
   * because data writing will stop at allocation of a first cluster,
   * before writing to memory.
   */
  const enum Result res = fsNodeWrite(node, FS_NODE_DATA,
      ALIG_FILE_SIZE, &buffer, (size_t)UINT32_MAX, 0);
  ck_assert_uint_ne(res, E_OK);

  /* Restore access */
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("NodeWriteFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAuxStreamErrors);
  tcase_add_test(testcase, testClusterAllocationErrors);
  tcase_add_test(testcase, testDataWriteErrors);
  tcase_add_test(testcase, testNodeMaxLength);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
