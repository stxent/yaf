/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "default_fs.h"
#include "helpers.h"
#include <yaf/fat32.h>
#include <xcore/fs/utils.h>
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
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
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeCreate(parent, longExtDesc, ARRAY_SIZE(longExtDesc));
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeCreate(parent, longDirNameDesc, ARRAY_SIZE(longDirNameDesc));
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeCreate(parent, longNameDesc, ARRAY_SIZE(longNameDesc));
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeCreate(parent, lowerCaseNameDesc, ARRAY_SIZE(lowerCaseNameDesc));
  ck_assert_uint_ne(res, E_OK);

  res = fsNodeCreate(parent, nameWithSpacesDesc,
      ARRAY_SIZE(nameWithSpacesDesc));
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(parent);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("UnicodeUnsupported");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testNodeNames);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
