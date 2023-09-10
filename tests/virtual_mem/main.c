/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "virtual_mem.h"
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

void *calloc(size_t number, size_t elementSize)
{
  return malloc(number * elementSize);
}
/*----------------------------------------------------------------------------*/
START_TEST(testAllocationErrors)
{
  static const struct VirtualMemConfig vmemConfigDefault = {
      .size = FS_TOTAL_SIZE
  };

  struct Interface *vmem;

  /* Memory initialization error */
  mallocHookFails = 2;
  vmem = init(VirtualMem, &vmemConfigDefault);
  ck_assert_ptr_null(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testInterfaceParams)
{
  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);

  uint64_t value;
  enum Result res;

  /* Try to read incorrect parameter */
  res = ifGetParam(vmem, IF_ZEROCOPY, NULL);
  ck_assert_uint_eq(res, E_INVALID);

  /* Try to write incorrect parameter */
  res = ifSetParam(vmem, IF_ZEROCOPY, NULL);
  ck_assert_uint_eq(res, E_INVALID);

  /* Try to set incorrect position */
  value = FS_TOTAL_SIZE;
  res = ifSetParam(vmem, IF_POSITION_64, &value);
  ck_assert_uint_eq(res, E_ADDRESS);

  /* Read default position */
  value = FS_TOTAL_SIZE;
  res = ifGetParam(vmem, IF_POSITION_64, &value);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(value, 0);

  /* Read partition size */
  value = 0;
  res = ifGetParam(vmem, IF_SIZE_64, &value);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(value, FS_TOTAL_SIZE);

  /* Read status */
  res = ifGetParam(vmem, IF_STATUS, NULL);
  ck_assert_uint_eq(res, E_OK);

  /* Set and verify position */
  value = FS_TOTAL_SIZE / 2;
  res = ifSetParam(vmem, IF_POSITION_64, &value);
  ck_assert_uint_eq(res, E_OK);
  value = 0;
  res = ifGetParam(vmem, IF_POSITION_64, &value);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(value, FS_TOTAL_SIZE / 2);

  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testMatchSkip)
{
  static const uint64_t position = CONFIG_SECTOR_SIZE;
  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };

  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);

  size_t count;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH] = {0};

  /* Match skip during parameter setup */
  vmemAddMarkedRegion(vmem, vmemExtractInfoRegion(), true, true, false);
  vmemSetMatchCounter(vmem, 1);
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_OK);
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_ADDRESS);
  vmemClearRegions(vmem);

  /* Match skip during write */
  vmemAddMarkedRegion(vmem, vmemExtractInfoRegion(), true, false, true);
  vmemSetMatchCounter(vmem, 1);
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_OK);
  count = ifWrite(vmem, buffer, sizeof(buffer));
  ck_assert_uint_eq(count, sizeof(buffer));
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_OK);
  count = ifWrite(vmem, buffer, sizeof(buffer));
  ck_assert_uint_eq(count, 0);
  vmemClearRegions(vmem);

  /* Match skip during read */
  vmemAddMarkedRegion(vmem, vmemExtractInfoRegion(), false, true, true);
  vmemSetMatchCounter(vmem, 1);
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_OK);
  count = ifRead(vmem, buffer, sizeof(buffer));
  ck_assert_uint_eq(count, sizeof(buffer));
  res = ifSetParam(vmem, IF_POSITION_64, &position);
  ck_assert_uint_eq(res, E_OK);
  count = ifRead(vmem, buffer, sizeof(buffer));
  ck_assert_uint_eq(count, 0);
  vmemClearRegions(vmem);

  /* Release resources */
  deinit(vmem);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("VirtualMem");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAllocationErrors);
  tcase_add_test(testcase, testInterfaceParams);
  tcase_add_test(testcase, testMatchSkip);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
