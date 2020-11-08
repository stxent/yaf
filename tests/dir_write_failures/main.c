/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "default_fs.h"
#include "helpers.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testClusterAllocationErrors)
{
  static const char path[] = PATH_SYS "/FILE.JPG";

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  makeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) - 2);

  struct FsNode * const parent = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  enum Result res;

  /* Directory cluster allocation failure at table read */
  changeLastAllocatedCluster(context.handle, getTableEntriesPerSector() - 1);
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableSectorRegion(context.interface, 0, 1), false, true, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Directory cluster allocation failure at table write */
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), true, false, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Directory cluster clear failure */
  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), true, false, true);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) - 2);
  freeTestHandle(context);
}
/*----------------------------------------------------------------------------*/
START_TEST(testDirCreationErrors)
{
  static const char path[] = PATH_HOME_USER "/OUTPUT";

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }
  };

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), true, false, true);
  const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeCreationErrors)
{
  static const char path[] = PATH_HOME_USER "/FILE.JPG";
  static const char buffer[MAX_BUFFER_LENGTH] = {0};

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor nodeWithDataDesc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          buffer,
          sizeof(buffer),
          FS_NODE_DATA
      }
  };
  const struct FsFieldDescriptor nodeWithoutDataDesc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  enum Result res;

  /* Data allocation failure */

  vmemAddRegion(context.interface,
      vmemExtractNodeDataRegion(context.interface, parent, 0));
  res = fsNodeCreate(parent, nodeWithDataDesc,
      ARRAY_SIZE(nodeWithDataDesc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Entry write failures */

  vmemAddMarkedRegion(context.interface,
      vmemExtractNodeDataRegion(context.interface, parent, 0),
      true, false, true);
  res = fsNodeCreate(parent, nodeWithoutDataDesc,
      ARRAY_SIZE(nodeWithoutDataDesc));
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  /* Release all resources */
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testRemovingErrors)
{
  struct TestContext context = makeTestHandle();
  struct FsNode *parent;
  struct FsNode *node;
  enum Result res;

  /* Table read failure */

  parent = fsOpenNode(context.handle, PATH_HOME_ROOT);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), false, true, true);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Table update failure */

  parent = fsOpenNode(context.handle, PATH_HOME_ROOT);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_ROOT_UNALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), true, false, true);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Info sector update failure */

  parent = fsOpenNode(context.handle, PATH_HOME_ROOT);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_ROOT_UNALIG);
  ck_assert_ptr_nonnull(node);

  vmemAddRegion(context.interface, vmemExtractInfoRegion());
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Data cluster update failure */

  parent = fsOpenNode(context.handle, PATH_HOME_USER);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  vmemAddMarkedRegion(context.interface,
      vmemExtractDataRegion(context.interface), true, false, true);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  vmemClearRegions(context.interface);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("DirWriteFailures");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testClusterAllocationErrors);
  tcase_add_test(testcase, testDirCreationErrors);
  tcase_add_test(testcase, testNodeCreationErrors);
  tcase_add_test(testcase, testRemovingErrors);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
