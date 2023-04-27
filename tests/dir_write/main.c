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
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
static void freeFillingNode(struct FsHandle *, const char *, size_t);
static void insertFillingNode(struct FsHandle *, const char *, size_t);
/*----------------------------------------------------------------------------*/
static void freeFillingNode(struct FsHandle *handle, const char *dir,
    size_t number)
{
  struct FsNode * const parent = fsOpenNode(handle, dir);
  ck_assert_ptr_nonnull(parent);

  char path[128];
  sprintf(path, "%s/F_%05u.TXT", dir, (unsigned int)number);

  struct FsNode * const node = fsOpenNode(handle, path);
  ck_assert_ptr_nonnull(node);
  const enum Result res = fsNodeRemove(parent, node);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(parent);
}
/*----------------------------------------------------------------------------*/
static void insertFillingNode(struct FsHandle *handle, const char *dir,
    size_t number)
{
  struct FsNode * const parent = fsOpenNode(handle, dir);
  ck_assert_ptr_nonnull(parent);

  char name[128];
  sprintf(name, "F_%05u.TXT", (unsigned int)number);

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
  const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(parent);
}
/*----------------------------------------------------------------------------*/
START_TEST(testAuxStreams)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, "/");
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* Write incorrect stream */
  res = fsNodeWrite(node, FS_TYPE_END, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_INVALID);

  /* Try to write data to the directory */
  res = fsNodeWrite(node, FS_NODE_DATA, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_INVALID);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDirClusterAllocation)
{
  struct TestContext context = makeTestHandle();

  makeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) * 4 - 2);
  freeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) * 4 - 2);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDirWrite)
{
  static const char path[] = PATH_HOME_USER_TEMP1 "/FILE.JPG";

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
  struct FsNode *node;
  struct FsNode *parent;
  enum Result res;

  /* Try to remove non-empty directory */
  parent = fsOpenBaseNode(context.handle, PATH_HOME);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME);
  ck_assert_ptr_nonnull(node);

  res = fsNodeRemove(parent, node);
  ck_assert_uint_eq(res, E_EXIST);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Try to create file inside another file */
  parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_VALUE);

  fsNodeFree(parent);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testGapFind)
{
  struct TestContext context = makeTestHandle();
  makeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) * 4 - 2);
  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), true, false, true);

  const size_t numbers[] = {
      /* Replace node in the middle of the sector */
      (getMaxEntriesPerSector() - 2) / 2,
      /* Replace node at the end of the sector */
      getMaxEntriesPerSector() - 2,
      /* Replace node at the beginning of the sector */
      getMaxEntriesPerSector() - 1,
      /* Replace node at the end of the cluster */
      getMaxEntriesPerCluster(context.handle) - 2,
      /* Replace node at the beginning of the cluster */
      getMaxEntriesPerCluster(context.handle) - 1
  };

  for (size_t i = 0; i < ARRAY_SIZE(numbers); ++i)
  {
    freeFillingNode(context.handle, PATH_SYS, numbers[i]);
    insertFillingNode(context.handle, PATH_SYS, numbers[i]);
  }

  /* Release all resources */
  vmemClearRegions(context.interface);
  freeFillingNodes(context.handle, PATH_SYS,
      getMaxEntriesPerCluster(context.handle) * 4 - 2);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeCreation)
{
  static const char path[] = PATH_HOME_USER "/FILE.JPG";
  static const char buffer[MAX_BUFFER_LENGTH] = {0};

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  const struct FsFieldDescriptor emptyNameDesc[] = {
      {
          "",
          1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };
  const struct FsFieldDescriptor incorrectAccessDesc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_ACCESS
      }
  };
  const struct FsFieldDescriptor incorrectNameDesc[] = {
      {
          name,
          strlen(name) / 2,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };
  const struct FsFieldDescriptor incorrectTimeDesc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_TIME
      }
  };
  const struct FsFieldDescriptor nodeWithoutNameDesc[] = {
      {
          NULL,
          0,
          FS_NODE_DATA
      }
  };
  const struct FsFieldDescriptor nodeWithPayloadDesc[] = {
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
  const struct FsFieldDescriptor unknownValueDesc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }, {
          NULL,
          0,
          FS_TYPE_END
      }
  };

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  struct FsNode *node;
  enum Result res;

  /* Create file with allocated data cluster */
  res = fsNodeCreate(parent, nodeWithPayloadDesc,
      ARRAY_SIZE(nodeWithPayloadDesc));
  ck_assert_uint_eq(res, E_OK);

  node = fsOpenNode(context.handle, path);
  ck_assert_ptr_nonnull(node);
  fsNodeFree(node);

  freeNode(context.handle, path);

  /* Try to create file with empty name */
  res = fsNodeCreate(parent, emptyNameDesc,
      ARRAY_SIZE(emptyNameDesc));
  ck_assert_uint_eq(res, E_VALUE);

  /* Try to create file with incorrect access */
  res = fsNodeCreate(parent, incorrectAccessDesc,
      ARRAY_SIZE(incorrectAccessDesc));
  ck_assert_uint_eq(res, E_VALUE);

  /* Try to create file with incorrect name */
  res = fsNodeCreate(parent, incorrectNameDesc,
      ARRAY_SIZE(incorrectNameDesc));
  ck_assert_uint_eq(res, E_VALUE);

  /* Try to create file with incorrect time */
  res = fsNodeCreate(parent, incorrectTimeDesc,
      ARRAY_SIZE(incorrectTimeDesc));
  ck_assert_uint_eq(res, E_VALUE);

  /* Try to create file without name */
  res = fsNodeCreate(parent, nodeWithoutNameDesc,
      ARRAY_SIZE(nodeWithoutNameDesc));
  ck_assert_uint_eq(res, E_VALUE);

  /* Create file with unknown descriptor */
  res = fsNodeCreate(parent, unknownValueDesc,
      ARRAY_SIZE(unknownValueDesc));
  ck_assert_uint_eq(res, E_OK);

  node = fsOpenNode(context.handle, path);
  ck_assert_ptr_nonnull(node);
  fsNodeFree(node);

  freeNode(context.handle, path);

  /* Release all resources */
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testReadOnlyDirWriting)
{
  static const char path[] = PATH_HOME_USER "/FILE.JPG";
  static const FsAccess roAccess = FS_ACCESS_READ;
  static const FsAccess rwAccess = FS_ACCESS_READ | FS_ACCESS_WRITE;

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
  struct FsNode *node;
  struct FsNode *parent;
  enum Result res;

  /* Write to a read-only directory */
  parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_ACCESS);
  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(parent);

  /* Try to remove from a read-only directory */
  parent = fsOpenBaseNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_eq(res, E_ACCESS);
  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Try to remove empty read-only directory */
  parent = fsOpenBaseNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(node);

  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_eq(res, E_ACCESS);
  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess),
      NULL);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("DirWrite");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAuxStreams);
  tcase_add_test(testcase, testDirClusterAllocation);
  tcase_add_test(testcase, testDirWrite);
  tcase_add_test(testcase, testGapFind);
  tcase_add_test(testcase, testNodeCreation);
  tcase_add_test(testcase, testReadOnlyDirWriting);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
