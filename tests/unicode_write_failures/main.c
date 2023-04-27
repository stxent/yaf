/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <check.h>
/*----------------------------------------------------------------------------*/
START_TEST(testNameAllocationError0)
{
  static const char path[] = PATH_SYS "/output file.txt";

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  makeFillingNodes(context.handle, PATH_SYS, getMaxEntriesPerSector() - 3);

  struct FsNode * const parent = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  enum Result res;

  /* Directory entry read failure */
  vmemSetMatchCounter(context.interface, 1);
  vmemAddMarkedRegion(context.interface,
      vmemExtractNodeDataRegion(context.interface, parent, 1),
      false, true, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_INTERFACE);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeFillingNodes(context.handle, PATH_SYS, getMaxEntriesPerSector() - 3);
  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
START_TEST(testNameAllocationError1)
{
  static const char path[] = PATH_SYS "/output file.txt";

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  makeFillingNodes(context.handle, PATH_SYS, getMaxEntriesPerSector() - 3);

  struct FsNode * const parent = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  enum Result res;

  /* Diectory entry write failure */
  vmemAddMarkedRegion(context.interface,
      vmemExtractNodeDataRegion(context.interface, parent, 0),
      true, false, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_INTERFACE);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeFillingNodes(context.handle, PATH_SYS, getMaxEntriesPerSector() - 3);
  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
START_TEST(testNameAllocationError2)
{
  static const char path[] = PATH_SYS "/output file.txt";

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  makeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) - 3);

  struct FsNode * const parent = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  enum Result res;

  /* Cluster fetch error */
  vmemSetMatchCounter(context.interface, 2);
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), false, true, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_INTERFACE);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) - 3);
  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("UnicodeWriteFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testNameAllocationError0);
  tcase_add_test(testcase, testNameAllocationError1);
  tcase_add_test(testcase, testNameAllocationError2);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
