/*
 * main.c
 * Copyright (C) 2021 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <yaf/fat32.h>
#include <yaf/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testCapacityReading)
{
  struct TestContext context = makeTestHandle();
  enum Result res;

  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  /* Read capacity length */

  FsLength length;

  res = fsNodeLength(node, FS_NODE_CAPACITY, &length);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(length, sizeof(FsCapacity));

  /* Read capacity */

  FsCapacity capacity;
  size_t count;

  res = fsNodeRead(node, FS_NODE_CAPACITY, 0, &capacity, sizeof(capacity),
      &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(capacity, ALIG_FILE_SIZE);
  ck_assert_uint_eq(count, sizeof(capacity));

  /* Read capacity failure */

  res = fsNodeRead(node, FS_NODE_CAPACITY, 0, &capacity, 0, 0);
  ck_assert_uint_ne(res, E_OK);

  fsNodeFree(node);
  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
START_TEST(testClusterSizeReading)
{
  struct TestContext context = makeTestHandle();

  const size_t size = fat32GetClusterSize(context.interface);
  ck_assert_uint_eq(size, FS_CLUSTER_SIZE);

  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
START_TEST(testEmptyVolumeUsage)
{
  enum Result res;

  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);

  static const struct Fat32FsConfig makeFsConfig =  {
      .clusterSize = FS_CLUSTER_SIZE,
      .tableCount = FS_TABLE_COUNT
  };
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_eq(res, E_OK);

  struct Fat32Config fsConfig = {
      .interface = vmem,
      .nodes = FS_NODE_POOL_SIZE,
      .threads = FS_THREAD_POOL_SIZE
  };
  struct FsHandle * const handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_nonnull(handle);

  /* Read file system statistics */

  FsCapacity used;
  uint8_t buffer[MAX_BUFFER_LENGTH];

  res = fat32GetUsage(handle, buffer, sizeof(buffer), &used);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(used, FS_CLUSTER_SIZE);

  /* Free all resources */

  deinit(handle);
  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testFullVolumeUsage)
{
  enum Result res;

  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);

  static const struct Fat32FsConfig makeFsConfig =  {
      .clusterSize = FS_CLUSTER_SIZE,
      .tableCount = FS_TABLE_COUNT
  };
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_eq(res, E_OK);

  struct Fat32Config fsConfig = {
      .interface = vmem,
      .nodes = FS_NODE_POOL_SIZE,
      .threads = FS_THREAD_POOL_SIZE
  };
  struct FsHandle * const handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_nonnull(handle);

  makeNode(handle, PATH_IMAGE, false, false);

  /* Create a very long file */

  struct FsNode * const node = fsOpenNode(handle, PATH_IMAGE);
  ck_assert_ptr_nonnull(node);

  for (size_t i = 0; i < UINT32_MAX / MAX_BUFFER_LENGTH; ++i)
  {
    size_t count = 0;
    char buffer[MAX_BUFFER_LENGTH];

    memset(buffer, (char)i, MAX_BUFFER_LENGTH);

    res = fsNodeWrite(node, FS_NODE_DATA, i * MAX_BUFFER_LENGTH,
        buffer, MAX_BUFFER_LENGTH, &count);
    ck_assert(res == E_OK || res == E_FULL);

    if (res == E_FULL)
      break;
  }

  fsNodeFree(node);

  /* Read file system statistics */

  FsCapacity size;
  FsCapacity used;
  uint8_t buffer[MAX_BUFFER_LENGTH];

  size = fat32GetCapacity(handle);
  ck_assert_uint_ne(size, 0);

  res = fat32GetUsage(handle, buffer, sizeof(buffer), &used);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(used, size);

  /* Cross-verification */

  size = fsFindUsedSpace(handle, 0);
  ck_assert_uint_eq(size, used);

  /* Read failures */

  vmemAddMarkedRegion(vmem, vmemExtractTableSectorRegion(vmem, 0, 0),
      false, true, true);

  res = fat32GetUsage(handle, buffer, sizeof(buffer), &used);
  ck_assert_uint_ne(res, E_OK);

  vmemClearRegions(vmem);

  /* Free all resources */

  freeNode(handle, PATH_IMAGE);
  deinit(handle);
  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testUsedSpaceCalculation)
{
  static const FsCapacity TOTAL_SPACE_USED =
      7 * FS_CLUSTER_SIZE + ALIG_FILE_SIZE
      + ((UNALIG_FILE_SIZE + FS_CLUSTER_SIZE - 1) & ~(FS_CLUSTER_SIZE - 1));

  struct TestContext context = makeTestHandle();
  FsCapacity used;

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

  tcase_add_test(testcase, testCapacityReading);
  tcase_add_test(testcase, testClusterSizeReading);
  tcase_add_test(testcase, testEmptyVolumeUsage);
  tcase_add_test(testcase, testFullVolumeUsage);
  tcase_add_test(testcase, testUsedSpaceCalculation);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
