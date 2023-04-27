/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <yaf/fat32.h>
#include <yaf/utils.h>
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
extern void *__libc_malloc(size_t);

static unsigned int mallocHookFails = 0;

void *malloc(size_t size)
{
  bool allocate = true;

  if (mallocHookFails && !--mallocHookFails)
    allocate = false;

  return allocate ? __libc_malloc(size) : 0;
}
/*----------------------------------------------------------------------------*/
START_TEST(testClusterAllocationErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode *node;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* First cluster allocation failure */

  node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), true, false, true);
  res = fsNodeWrite(node, FS_NODE_DATA, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_INTERFACE);
  vmemClearRegions(context.interface);

  fsNodeFree(node);

  /* Info sector update failure */

  node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP2);
  ck_assert_ptr_nonnull(node);

  vmemAddRegion(context.interface, vmemExtractInfoRegion());
  res = fsNodeWrite(node, FS_NODE_DATA, 0, buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_ADDRESS);
  vmemClearRegions(context.interface);

  fsNodeFree(node);

  /* Table read failure */

  node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  /* Read last chunk to update internal file pointer */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE - sizeof(buffer),
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_OK);

  changeLastAllocatedCluster(context.handle, getTableEntriesPerSector() - 1);
  vmemSetMatchCounter(context.interface, 1);
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableSectorRegion(context.interface, 0, 0), false, true, true);
  res = fsNodeWrite(node, FS_NODE_DATA, ALIG_FILE_SIZE,
      buffer, sizeof(buffer), NULL);
  ck_assert_uint_eq(res, E_INTERFACE);
  vmemClearRegions(context.interface);

  fsNodeFree(node);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testMountErrors)
{
  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);
  uint8_t * const arena = vmemGetAddress(vmem);

  static const struct Fat32FsConfig makeFsConfig =  {
      .clusterSize = FS_CLUSTER_SIZE,
      .tableCount = FS_TABLE_COUNT
  };
  const enum Result res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_eq(res, E_OK);

  struct Fat32Config fsConfig = {
      .interface = vmem,
      .nodes = 0,
      .threads = 0
  };
  struct FsHandle *handle;
  uint32_t value32;
  uint16_t value16;

  /* Incorrect settings */
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);

  /* Restore node count */
  fsConfig.nodes = FS_NODE_POOL_SIZE;

  /* Incorrect bytes per sector */
  memcpy(&value16, &arena[0x00B], sizeof(value16));
  memset(&arena[0x00B], 0xFF, sizeof(value16));
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  memcpy(&arena[0x00B], &value16, sizeof(value16));

  /* Incorrect boot signature */
  memcpy(&value16, &arena[0x1FE], sizeof(value16));
  memset(&arena[0x1FE], 0xFF, sizeof(value16));
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  memcpy(&arena[0x1FE], &value16, sizeof(value16));

  /* Incorrect info signature 1 */
  memcpy(&value32, &arena[0x200 + 0x000], sizeof(value32));
  memset(&arena[0x200 + 0x000], 0xFF, sizeof(value32));
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  memcpy(&arena[0x200 + 0x000], &value32, sizeof(value32));

  /* Incorrect info signature 2 */
  memcpy(&value32, &arena[0x200 + 0x1E4], sizeof(value32));
  memset(&arena[0x200 + 0x1E4], 0xFF, sizeof(value32));
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  memcpy(&arena[0x200 + 0x1E4], &value32, sizeof(value32));

  /* Boot sector read failure */
  vmemAddRegion(vmem, vmemExtractBootRegion());
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  vmemClearRegions(vmem);

  /* Info sector read failure */
  vmemAddRegion(vmem, vmemExtractInfoRegion());
  handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_null(handle);
  vmemClearRegions(vmem);

#ifdef CONFIG_WRITE
  const unsigned int memoryFailureCount = 6;
#else
  const unsigned int memoryFailureCount = 5;
#endif

  for (unsigned int i = 2; i <= memoryFailureCount; ++i)
  {
    mallocHookFails = i;
    handle = init(FatHandle, &fsConfig);
    ck_assert_ptr_null(handle);
  }

  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testSyncErrors)
{
  static const char data[MAX_BUFFER_LENGTH] = {0};

  struct TestContext context = makeTestHandle();
  enum Result res;

  struct FsNode * const node = fsOpenNode(context.handle,
      PATH_HOME_ROOT_UNALIG);
  ck_assert_ptr_nonnull(node);
  res = fsNodeWrite(node, FS_NODE_DATA, 0, data, MAX_BUFFER_LENGTH, NULL);
  ck_assert_uint_eq(res, E_OK);

  /* Add directory data region to forbidden regions */
  vmemAddRegion(context.interface, vmemExtractDataRegion(context.interface));
  /* Failure in handle sync function */
  res = fsHandleSync(context.handle);
  ck_assert_uint_eq(res, E_ADDRESS);
  /* Failure in node destructor */
  fsNodeFree(node);
  /* Remove all forbidden regions */
  vmemClearRegions(context.interface);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("HandleFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testClusterAllocationErrors);
  tcase_add_test(testcase, testMountErrors);
  tcase_add_test(testcase, testSyncErrors);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
