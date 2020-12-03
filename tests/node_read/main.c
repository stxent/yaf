/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include <xcore/fs/utils.h>
#include <xcore/realtime.h>
#include <check.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
START_TEST(testAccessRead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count = 0;
  FsAccess access = 0;
  enum Result res;

  /* Read current access flags */
  res = fsNodeRead(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(access));
  ck_assert_uint_eq(access, FS_ACCESS_READ | FS_ACCESS_WRITE);

  /* Try to partially read access flags */
  res = fsNodeRead(node, FS_NODE_ACCESS, 1,
      &access, sizeof(access), &count);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeRead(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access) - 1, &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testAuxStreams)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  uint64_t id = 0;
  size_t count = 0;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* Read internal identifier */
  res = fsNodeRead(node, FS_NODE_ID, 0, &id, sizeof(id), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(id));
  ck_assert_uint_ne(id, 0);

  /* Try to partially read identifier */
  res = fsNodeRead(node, FS_NODE_ID, 1, &id, sizeof(id), &count);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeRead(node, FS_NODE_ID, 0, &id, sizeof(id) - 1, &count);
  ck_assert_uint_ne(res, E_OK);

  /* Try to read incorrect stream */
  res = fsNodeRead(node, FS_TYPE_END, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDataRead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* Read zero bytes */
  count = (size_t)-1;
  res = fsNodeRead(node, FS_NODE_DATA, 0, buffer, 0, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, 0);

  /* Read at the end of file */
  count = (size_t)-1;
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE,
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, 0);

  /* Try to read after the end of file */
  res = fsNodeRead(node, FS_NODE_DATA, ALIG_FILE_SIZE + 1,
      buffer, sizeof(buffer), &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testLength)
{
  static const char path[] = PATH_HOME_ROOT_ALIG;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  FsLength count;
  enum Result res;

  res = fsNodeLength(node, FS_NODE_DATA, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, ALIG_FILE_SIZE);

  res = fsNodeLength(node, FS_NODE_ACCESS, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(FsAccess));

  res = fsNodeLength(node, FS_NODE_ID, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(uint64_t));

  res = fsNodeLength(node, FS_NODE_NAME, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, strlen(fsExtractName(path)) + 1);

  res = fsNodeLength(node, FS_NODE_TIME, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(time64_t));

  res = fsNodeLength(node, FS_TYPE_END, &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNameRead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* Try to partially read name */
  res = fsNodeRead(node, FS_NODE_NAME, 1, buffer, sizeof(buffer), &count);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeRead(node, FS_NODE_NAME, 0, buffer, 1, &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testSparseRead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  /* Sparse read */
  size_t count;
  size_t sparseChunkNumber;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];
  char pattern[MAX_BUFFER_LENGTH];

  ck_assert_uint_eq(sizeof(buffer), sizeof(pattern));

  sparseChunkNumber = ALIG_FILE_SIZE / sizeof(buffer) - 1;
  memset(pattern, sparseChunkNumber, sizeof(pattern));
  res = fsNodeRead(node, FS_NODE_DATA, sparseChunkNumber * sizeof(buffer),
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(buffer));
  ck_assert_mem_eq(buffer, pattern, sizeof(buffer));

  sparseChunkNumber = 0;
  memset(pattern, sparseChunkNumber, sizeof(pattern));
  res = fsNodeRead(node, FS_NODE_DATA, sparseChunkNumber * sizeof(buffer),
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(buffer));
  ck_assert_mem_eq(buffer, pattern, sizeof(buffer));

  sparseChunkNumber = ALIG_FILE_SIZE / sizeof(buffer) - 1;
  memset(pattern, sparseChunkNumber, sizeof(pattern));
  res = fsNodeRead(node, FS_NODE_DATA, sparseChunkNumber * sizeof(buffer),
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(buffer));
  ck_assert_mem_eq(buffer, pattern, sizeof(buffer));

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testTimeRead)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  time64_t timestamp = 0;
  size_t count = 0;
  enum Result res;

  /* Read time */
  res = fsNodeRead(node, FS_NODE_TIME, 0,
      &timestamp, sizeof(timestamp), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(timestamp));
  ck_assert(timestamp == RTC_INITIAL_TIME * 1000000);

  /* Try to partially read time */
  res = fsNodeRead(node, FS_NODE_TIME, 1,
      &timestamp, sizeof(timestamp), &count);
  ck_assert_uint_ne(res, E_OK);
  res = fsNodeRead(node, FS_NODE_TIME, 0,
      &timestamp, sizeof(timestamp) - 1, &count);
  ck_assert_uint_ne(res, E_OK);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("NodeRead");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAccessRead);
  tcase_add_test(testcase, testAuxStreams);
  tcase_add_test(testcase, testDataRead);
  tcase_add_test(testcase, testLength);
  tcase_add_test(testcase, testNameRead);
  tcase_add_test(testcase, testSparseRead);
  tcase_add_test(testcase, testTimeRead);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int numberFailed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return numberFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
