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
START_TEST(testAlignedRW)
{
  static const size_t bufferLength = MAX_BUFFER_LENGTH;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  /* Read after each write */
  for (size_t i = 0; i < ALIG_FILE_SIZE / bufferLength; ++i)
  {
    size_t count;
    enum Result res;
    char buffer[bufferLength];
    char pattern[bufferLength];

    memset(pattern, i, sizeof(pattern));

    count = 0;
    res = fsNodeWrite(node, FS_NODE_DATA, i * sizeof(pattern),
        pattern, sizeof(pattern), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(pattern));

    count = 0;
    res = fsNodeRead(node, FS_NODE_DATA, i * sizeof(buffer),
        buffer, sizeof(buffer), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(buffer));
    ck_assert_mem_eq(buffer, pattern, sizeof(buffer));
  }

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testContinuousAlignedRW)
{
  static const size_t BUFFER_LENGTH = MAX_BUFFER_LENGTH;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  /* Continuous write, then continuous read */
  for (size_t i = 0; i < ALIG_FILE_SIZE / BUFFER_LENGTH; ++i)
  {
    size_t count;
    enum Result res;
    char pattern[BUFFER_LENGTH];

    memset(pattern, i, sizeof(pattern));

    res = fsNodeWrite(node, FS_NODE_DATA, i * sizeof(pattern),
        pattern, sizeof(pattern), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(pattern));
  }
  for (size_t i = 0; i < ALIG_FILE_SIZE / BUFFER_LENGTH; ++i)
  {
    size_t count;
    enum Result res;
    char buffer[BUFFER_LENGTH];
    char pattern[BUFFER_LENGTH];

    memset(pattern, i, sizeof(pattern));

    res = fsNodeRead(node, FS_NODE_DATA, i * sizeof(buffer),
        buffer, sizeof(buffer), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(buffer));
    ck_assert_mem_eq(buffer, pattern, sizeof(buffer));
  }

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testUnalignedRW)
{
  static const size_t BUFFER_LENGTH = (MAX_BUFFER_LENGTH * 3) / 4;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  /* Read after each write */
  for (size_t i = 0; i < UNALIG_FILE_SIZE / BUFFER_LENGTH; ++i)
  {
    size_t count;
    enum Result res;
    char buffer[BUFFER_LENGTH];
    char pattern[BUFFER_LENGTH];

    memset(pattern, i, sizeof(pattern));

    count = 0;
    res = fsNodeWrite(node, FS_NODE_DATA, i * sizeof(pattern),
        pattern, sizeof(pattern), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(pattern));

    count = 0;
    res = fsNodeRead(node, FS_NODE_DATA, i * sizeof(buffer),
        buffer, sizeof(buffer), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(buffer));
    ck_assert_mem_eq(buffer, pattern, sizeof(buffer));
  }

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testContinuousUnalignedRW)
{
  static const size_t BUFFER_LENGTH = (MAX_BUFFER_LENGTH * 3) / 4;

  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_USER_TEMP1);
  ck_assert_ptr_nonnull(node);

  /* Continuous write, then continuous read */
  for (size_t i = 0; i < UNALIG_FILE_SIZE / BUFFER_LENGTH; ++i)
  {
    size_t count;
    enum Result res;
    char pattern[BUFFER_LENGTH];

    memset(pattern, i, sizeof(pattern));

    res = fsNodeWrite(node, FS_NODE_DATA, i * sizeof(pattern),
        pattern, sizeof(pattern), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(pattern));
  }
  for (size_t i = 0; i < UNALIG_FILE_SIZE / BUFFER_LENGTH; ++i)
  {
    size_t count;
    enum Result res;
    char buffer[BUFFER_LENGTH];
    char pattern[BUFFER_LENGTH];

    memset(pattern, i, sizeof(pattern));

    res = fsNodeRead(node, FS_NODE_DATA, i * sizeof(buffer),
        buffer, sizeof(buffer), &count);
    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, sizeof(buffer));
    ck_assert_uint_eq(sizeof(buffer), sizeof(pattern));
    ck_assert_mem_eq(buffer, pattern, sizeof(buffer));
  }

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testAccessWrite)
{
  struct TestContext context = makeTestHandle();
  struct FsNode *node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count;
  FsAccess access;
  enum Result res;

  /* Write and verify access flags */
  access = FS_ACCESS_READ;
  count = 0;

  res = fsNodeWrite(node, FS_NODE_ACCESS, 0, &access,
      sizeof(access), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(access));

  access = 0;
  count = 0;

  res = fsNodeRead(node, FS_NODE_ACCESS, 0, &access,
      sizeof(access), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(access));
  ck_assert_uint_eq(access, FS_ACCESS_READ);

  /* Reopen and check access again */
  fsNodeFree(node);
  node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  access = 0;
  count = 0;

  res = fsNodeRead(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(access));
  ck_assert_uint_eq(access, FS_ACCESS_READ);

  /* Restore access flags */
  access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  count = 0;

  res = fsNodeWrite(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(access));

  /* Try to write incorrect access flags */
  access = FS_ACCESS_WRITE;
  res = fsNodeWrite(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access), &count);
  ck_assert_uint_eq(res, E_VALUE);

  /* Try unaligned write to access flags */
  res = fsNodeWrite(node, FS_NODE_ACCESS, 1,
      &access, sizeof(access), &count);
  ck_assert_uint_eq(res, E_VALUE);
  res = fsNodeWrite(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access) - 1, &count);
  ck_assert_uint_eq(res, E_VALUE);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testDataWrite)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count = (size_t)-1;
  enum Result res;
  char buffer[MAX_BUFFER_LENGTH];

  /* Write zero bytes */
  res = fsNodeWrite(node, FS_NODE_DATA, 0, NULL, 0, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, 0);

  /* Try to write after the end of the file */
  res = fsNodeWrite(node, FS_NODE_DATA, ALIG_FILE_SIZE + 1,
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_VALUE);

  /* Try to write incorrect stream */
  res = fsNodeWrite(node, FS_TYPE_END, 0,
      buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_INVALID);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testNameWrite)
{
  static const char newNodeName[] = "NEW_NAME.TXT";

  struct TestContext context = makeTestHandle();
  struct FsNode *node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;

  /* Try to write new file name */
  res = fsNodeWrite(node, FS_NODE_NAME, 0, newNodeName,
      strlen(newNodeName) + 1, &count);
  ck_assert_uint_eq(res, E_INVALID);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testTimeWrite)
{
  static const time64_t newNodeTime = (RTC_INITIAL_TIME + 3600) * 1000000;

  struct TestContext context = makeTestHandle();
  struct FsNode *node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  time64_t timestamp;
  size_t count;
  enum Result res;

  /* Write a new node time */
  timestamp = newNodeTime;
  count = 0;

  res = fsNodeWrite(node, FS_NODE_TIME, 0,
      &timestamp, sizeof(timestamp), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(timestamp));

  /* Verify node time */
  timestamp = 0;
  count = 0;

  res = fsNodeRead(node, FS_NODE_TIME, 0,
      &timestamp, sizeof(timestamp), &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, sizeof(timestamp));
  ck_assert(timestamp == newNodeTime);

  /* Try unaligned write to time */
  res = fsNodeWrite(node, FS_NODE_TIME, 1,
      &timestamp, sizeof(timestamp), &count);
  ck_assert_uint_eq(res, E_VALUE);
  res = fsNodeWrite(node, FS_NODE_TIME, 0,
      &timestamp, sizeof(timestamp) - 1, &count);
  ck_assert_uint_eq(res, E_VALUE);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testWriteOverflow)
{
  struct TestContext context = makeTestHandle();
  struct FsNode * const node = fsOpenNode(context.handle, PATH_HOME_ROOT_ALIG);
  ck_assert_ptr_nonnull(node);

  /* Try to write a very long file */
  for (size_t i = 0; i < UINT32_MAX / MAX_BUFFER_LENGTH; ++i)
  {
    size_t count = 0;
    char buffer[MAX_BUFFER_LENGTH];

    memset(buffer, (char)i, MAX_BUFFER_LENGTH);

    const enum Result res = fsNodeWrite(node, FS_NODE_DATA,
        i * MAX_BUFFER_LENGTH, buffer, MAX_BUFFER_LENGTH, &count);
    ck_assert(res == E_OK || res == E_FULL);

    if (res == E_FULL)
      break;
  }

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testWriteToReadOnly)
{
  static const char buffer[MAX_BUFFER_LENGTH] = {0};

  struct TestContext context = makeTestHandle();
  struct FsNode *node = fsOpenNode(context.handle, PATH_HOME_ROOT_RO);
  ck_assert_ptr_nonnull(node);

  size_t count;
  enum Result res;

  /* Try to write to the read-only file */
  res = fsNodeWrite(node, FS_NODE_DATA, 0, buffer, sizeof(buffer), &count);
  ck_assert_uint_eq(res, E_ACCESS);

  /* Release all resources */
  fsNodeFree(node);
  freeTestHandle(context);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("NodeWrite");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testAlignedRW);
  tcase_add_test(testcase, testContinuousAlignedRW);
  tcase_add_test(testcase, testUnalignedRW);
  tcase_add_test(testcase, testContinuousUnalignedRW);
  tcase_add_test(testcase, testAccessWrite);
  tcase_add_test(testcase, testDataWrite);
  tcase_add_test(testcase, testNameWrite);
  tcase_add_test(testcase, testTimeWrite);
  tcase_add_test(testcase, testWriteOverflow);
  tcase_add_test(testcase, testWriteToReadOnly);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
