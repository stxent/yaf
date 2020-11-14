/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
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
  sprintf(path, "%s/filename_%05u.txt", dir, (unsigned int)number);

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
  sprintf(name, "filename_%05u.txt", (unsigned int)number);

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
  const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(parent);
}
/*----------------------------------------------------------------------------*/
START_TEST(testDirOverflow)
{
  struct TestContext context = makeTestHandle();

  /* Make temporary nodes */
  struct FsNode * const parent = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);

  for (size_t i = 0; i <= getMaxSimilarNamesCount(); ++i)
  {
    char name[64];
    sprintf(name, "filename_%05u.txt", (unsigned int)i);

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
    const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));

    if (i == getMaxSimilarNamesCount())
      ck_assert_uint_eq(res, E_EXIST);
    else
      ck_assert_uint_eq(res, E_OK);
  }

  fsNodeFree(parent);

  /* Free temporary nodes */
  for (size_t i = 0; i < getMaxSimilarNamesCount(); ++i)
    freeFillingNode(context.handle, PATH_SYS, i);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testGapFind)
{
  struct TestContext context = makeTestHandle();
  const size_t maxEntryNumber =
      (getMaxEntriesPerCluster(context.handle) - 2) / 3
      + getMaxEntriesPerCluster(context.handle);

  for (size_t i = 0; i < maxEntryNumber; ++i)
    insertFillingNode(context.handle, PATH_SYS, i);

  vmemAddMarkedRegion(context.interface,
      vmemExtractTableRegion(context.interface, 0), true, false, true);

  /* Replace all created nodes one by one */
  for (size_t i = 0; i < maxEntryNumber; ++i)
  {
    freeFillingNode(context.handle, PATH_SYS, i);
    insertFillingNode(context.handle, PATH_SYS, i);
  }

  /* Release all resources */
  vmemClearRegions(context.interface);

  for (size_t i = 0; i < maxEntryNumber; ++i)
    freeFillingNode(context.handle, PATH_SYS, i);

  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNameOverflow)
{
  char path[CONFIG_NAME_LENGTH];
  char *position = path;

  *position++ = '/';
  for (size_t i = 0; i < CONFIG_NAME_LENGTH * 2 / 3; ++i)
    *position++ = '_';
  memcpy(position, ".txt\0", 5);

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);
  printf("path = %s\r\n", path);
  printf("name = %s\r\n", name);

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenBaseNode(context.handle, path);
  ck_assert_ptr_nonnull(parent);

  /* Name overflow */
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
  const enum Result res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNameWithSpaces)
{
  static const char pathA[] = PATH_HOME_USER "/        .A";
  static const char pathB[] = PATH_HOME_USER "/        .B";

  struct TestContext context = makeTestHandle();
  struct FsNode *node;

  makeNode(context.handle, pathA, false, false);
  node = fsOpenNode(context.handle, pathA);
  ck_assert_ptr_nonnull(node);
  fsNodeFree(node);

  makeNode(context.handle, pathB, false, false);
  node = fsOpenNode(context.handle, pathB);
  ck_assert_ptr_nonnull(node);
  fsNodeFree(node);

  freeNode(context.handle, pathB);
  freeNode(context.handle, pathA);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNameWithoutExtension)
{
  static const char path[] = PATH_HOME_USER "/file name without ext";

  struct TestContext context = makeTestHandle();

  /* Create node with a name but without an extension */
  makeNode(context.handle, path, false, false);
  freeNode(context.handle, path);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNodeNames)
{
  static const char path0[] = PATH_HOME_USER "/DIR.EXT";
  static const char path1[] = PATH_HOME_USER "/LONG.EXTENSION";
  static const char path2[] = PATH_HOME_USER "/LONG_DIR_NAME";
  static const char path3[] = PATH_HOME_USER "/LONG_NAME.TXT";
  static const char path4[] = PATH_HOME_USER "/lower.txt";
  static const char path5[] = PATH_HOME_USER "/S P C E.TXT";

  /* Character conversion required */
  const struct FsFieldDescriptor incorrectDirDesc[] = {
      {
          fsExtractName(path0),
          strlen(fsExtractName(path0)) + 1,
          FS_NODE_NAME
      }
  };
  /* Extension is longer than 8.3 */
  const struct FsFieldDescriptor longExtDesc[] = {
      {
          fsExtractName(path1),
          strlen(fsExtractName(path1)) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };
  /* Directory name is longer than 8.3 */
  const struct FsFieldDescriptor longDirNameDesc[] = {
      {
          fsExtractName(path2),
          strlen(fsExtractName(path2)) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };
  /* File name is longer than 8.3 */
  const struct FsFieldDescriptor longNameDesc[] = {
      {
          fsExtractName(path3),
          strlen(fsExtractName(path3)) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };
  /* File name is in lower case */
  const struct FsFieldDescriptor lowerCaseNameDesc[] = {
      {
          fsExtractName(path4),
          strlen(fsExtractName(path4)) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };
  /* Name with spaces */
  const struct FsFieldDescriptor nameWithSpacesDesc[] = {
      {
          fsExtractName(path5),
          strlen(fsExtractName(path5)) + 1,
          FS_NODE_NAME
      }, {
          0,
          0,
          FS_NODE_DATA
      }
  };

  struct TestContext context = makeTestHandle();
  struct FsNode * const parent = fsOpenNode(context.handle, PATH_HOME_USER);
  ck_assert_ptr_nonnull(parent);

  enum Result res;

  res = fsNodeCreate(parent, incorrectDirDesc, ARRAY_SIZE(incorrectDirDesc));
  ck_assert_uint_eq(res, E_OK);

  res = fsNodeCreate(parent, longExtDesc, ARRAY_SIZE(longExtDesc));
  ck_assert_uint_eq(res, E_OK);

  res = fsNodeCreate(parent, longDirNameDesc, ARRAY_SIZE(longDirNameDesc));
  ck_assert_uint_eq(res, E_OK);

  res = fsNodeCreate(parent, longNameDesc, ARRAY_SIZE(longNameDesc));
  ck_assert_uint_eq(res, E_OK);

  res = fsNodeCreate(parent, lowerCaseNameDesc, ARRAY_SIZE(lowerCaseNameDesc));
  ck_assert_uint_eq(res, E_OK);

  res = fsNodeCreate(parent, nameWithSpacesDesc,
      ARRAY_SIZE(nameWithSpacesDesc));
  ck_assert_uint_eq(res, E_OK);

  /* Remove temporary nodes */
  freeNode(context.handle, path5);
  freeNode(context.handle, path4);
  freeNode(context.handle, path3);
  freeNode(context.handle, path2);
  freeNode(context.handle, path1);
  freeNode(context.handle, path0);

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
          0,
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

  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess), 0);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeCreate(parent, desc, ARRAY_SIZE(desc));
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess), 0);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(parent);

  /* Try to remove from a read-only directory */
  parent = fsOpenBaseNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess), 0);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeWrite(parent, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess), 0);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Try to remove empty read-only directory */
  parent = fsOpenBaseNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(parent);
  node = fsOpenNode(context.handle, PATH_SYS);
  ck_assert_ptr_nonnull(node);

  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &roAccess, sizeof(roAccess), 0);
  ck_assert_uint_eq(res, E_OK);
  res = fsNodeRemove(parent, node);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &rwAccess, sizeof(rwAccess), 0);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(parent);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testShortNameReplace)
{
  static const char path[] = PATH_HOME_USER "/FILE.JPG";

  struct TestContext context = makeTestHandle();

  makeNode(context.handle, path, true, false);
  freeNode(context.handle, path);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testSimilarShortNames)
{
  static const char pathA[] = PATH_HOME_USER "/long_file_name_a.txt";
  static const char pathB[] = PATH_HOME_USER "/long_file_name_b.txt";

  struct TestContext context = makeTestHandle();

  /* Similar short names */
  makeNode(context.handle, pathA, false, false);
  makeNode(context.handle, pathB, false, false);
  freeNode(context.handle, pathB);
  freeNode(context.handle, pathA);

  /* Release all resources */
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("UnicodeWrite");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testDirOverflow);
  tcase_add_test(testcase, testGapFind);
  tcase_add_test(testcase, testNameOverflow);
  tcase_add_test(testcase, testNameWithoutExtension);
  tcase_add_test(testcase, testNameWithSpaces);
  tcase_add_test(testcase, testNodeNames);
  tcase_add_test(testcase, testReadOnlyDirWriting);
  tcase_add_test(testcase, testShortNameReplace);
  tcase_add_test(testcase, testSimilarShortNames);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
