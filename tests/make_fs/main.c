/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "virtual_mem.h"
#include <yaf/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testMemoryErrors)
{
  static const struct Fat32FsConfig makeFsConfig =  {
      .clusterSize = FS_CLUSTER_SIZE,
      .tableCount = FS_TABLE_COUNT,
      .label = "TEST"
  };
  static const struct VirtualMemConfig vmemConfigDefault = {
      .size = FS_TOTAL_SIZE
  };
  static const struct VirtualMemConfig vmemConfigTiny = {
      .size = CONFIG_SECTOR_SIZE
  };
  static const struct VirtualMemConfig vmemConfigZero = {
      .size = 0
  };

  struct Interface *vref;
  struct Interface *vmem;
  enum Result res;

  /* Make reference partition */
  vref = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_nonnull(vref);
  res = fat32MakeFs(vref, &makeFsConfig);
  ck_assert_uint_eq(res, E_OK);

  /* Partition size reading failure */
  vmem = init(VirtualMem, &vmemConfigZero);
  ck_assert_ptr_nonnull(vmem);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Cluster calculation error */
  vmem = init(VirtualMem, &vmemConfigTiny);
  ck_assert_ptr_nonnull(vmem);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Boot sector addressing and writing errors */
  vmem = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_nonnull(vmem);
  vmemAddMarkedRegion(vmem, vmemExtractBootRegion(), false, false, true);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  vmemAddMarkedRegion(vmem, vmemExtractBootRegion(), false, false, false);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Info sector addressing and writing errors */
  vmem = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_nonnull(vmem);
  vmemAddMarkedRegion(vmem, vmemExtractInfoRegion(), false, false, true);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  vmemAddMarkedRegion(vmem, vmemExtractInfoRegion(), false, false, false);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Table addressing and writing errors */
  vmem = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_nonnull(vmem);
  vmemAddMarkedRegion(vmem, vmemExtractTableRegion(vref, 0),
      false, false, true);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  vmemAddMarkedRegion(vmem, vmemExtractTableRegion(vref, 0),
      false, false, false);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Root cluster addressing and writing errors */
  vmem = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_nonnull(vmem);
  vmemAddMarkedRegion(vmem, vmemExtractRootDataRegion(vref),
      false, false, true);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  vmemAddMarkedRegion(vmem, vmemExtractRootDataRegion(vref),
      false, false, false);
  res = fat32MakeFs(vmem, &makeFsConfig);
  ck_assert_uint_ne(res, E_OK);
  deinit(vmem);

  /* Free reference partition */
  deinit(vref);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("MakeFs");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testMemoryErrors);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
