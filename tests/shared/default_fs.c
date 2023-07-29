/*
 * yaf/tests/shared/default_fs.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include "default_fs.h"
#include "virtual_mem.h"
#include <xcore/fs/utils.h>
#include <xcore/realtime.h>
#include <yaf/fat32.h>
#include <yaf/utils.h>
#include <check.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
static void prepareNodeData(struct FsHandle *, const char *, FsLength);
static void restoreNodeAccess(struct FsHandle *, const char *);
/*----------------------------------------------------------------------------*/
static void prepareNodeData(struct FsHandle *handle, const char *path,
    FsLength length)
{
  FsLength left = length;
  int iteration = 0;
  char buffer[MAX_BUFFER_LENGTH];

  struct FsNode * const node = fsOpenNode(handle, path);
  ck_assert_ptr_nonnull(node);

  while (left)
  {
    const size_t chunk = MIN((size_t)left, MAX_BUFFER_LENGTH);
    memset(buffer, iteration, chunk);

    size_t count = 0;
    const enum Result res = fsNodeWrite(node, FS_NODE_DATA,
        (FsLength)iteration * MAX_BUFFER_LENGTH, buffer, chunk, &count);

    ck_assert_uint_eq(res, E_OK);
    ck_assert_uint_eq(count, chunk);

    left -= (FsLength)chunk;
    ++iteration;
  }

  FsLength count = 0;
  const enum Result res = fsNodeLength(node, FS_NODE_DATA, &count);
  ck_assert_uint_eq(res, E_OK);
  ck_assert_uint_eq(count, length);

  fsNodeFree(node);
}
/*----------------------------------------------------------------------------*/
static void restoreNodeAccess(struct FsHandle *handle, const char *path)
{
  static const FsAccess access = FS_ACCESS_READ | FS_ACCESS_WRITE;

  struct FsNode * const node = fsOpenNode(handle, path);
  ck_assert_ptr_nonnull(node);

  const enum Result res = fsNodeWrite(node, FS_NODE_ACCESS, 0,
      &access, sizeof(access), NULL);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
}
/*----------------------------------------------------------------------------*/
void freeFillingNodes(struct FsHandle *handle, const char *dir, size_t count)
{
  struct FsNode * const parent = fsOpenNode(handle, dir);
  ck_assert_ptr_nonnull(parent);

  for (size_t i = 0; i < count; ++i)
  {
    char path[128];
    sprintf(path, "%s/F_%05u.TXT", dir, (unsigned int)i);

    struct FsNode * const node = fsOpenNode(handle, path);
    ck_assert_ptr_nonnull(node);
    const enum Result res = fsNodeRemove(parent, node);
    ck_assert_uint_eq(res, E_OK);

    fsNodeFree(node);
  }

  fsNodeFree(parent);
}
/*----------------------------------------------------------------------------*/
void freeNode(struct FsHandle *handle, const char *path)
{
  struct FsNode * const root = fsOpenBaseNode(handle, path);
  ck_assert_ptr_nonnull(root);

  struct FsNode * const node = fsOpenNode(handle, path);
  ck_assert_ptr_nonnull(node);
  const enum Result res = fsNodeRemove(root, node);
  ck_assert_uint_eq(res, E_OK);

  fsNodeFree(node);
  fsNodeFree(root);
}
/*----------------------------------------------------------------------------*/
void freeTestHandle(struct TestContext context)
{
  restoreNodeAccess(context.handle, PATH_HOME_ROOT_RO);

  /* Level 3 inside "/HOME/USER" */
  freeNode(context.handle, PATH_HOME_ROOT_SHORT);
  freeNode(context.handle, PATH_HOME_ROOT_RO);
  freeNode(context.handle, PATH_HOME_ROOT_NOEXT);
  freeNode(context.handle, PATH_HOME_ROOT_UNALIG);
  freeNode(context.handle, PATH_HOME_ROOT_ALIG);

  /* Level 3 inside "/HOME/ROOT" */
  freeNode(context.handle, PATH_HOME_USER_TEMP4);
  freeNode(context.handle, PATH_HOME_USER_TEMP3);
  freeNode(context.handle, PATH_HOME_USER_TEMP2);
  freeNode(context.handle, PATH_HOME_USER_TEMP1);

  /* Level 2 inside "/HOME" */
  freeNode(context.handle, PATH_HOME_USER);
  freeNode(context.handle, PATH_HOME_ROOT);

  /* Level 1 inside "/" */
  freeNode(context.handle, PATH_SYS);
  freeNode(context.handle, PATH_LIB);
  freeNode(context.handle, PATH_HOME);
  freeNode(context.handle, PATH_BOOT);

  deinit(context.handle);
  deinit(context.interface);
}
/*----------------------------------------------------------------------------*/
void makeFillingNodes(struct FsHandle *handle, const char *dir, size_t count)
{
  struct FsNode * const parent = fsOpenNode(handle, dir);
  ck_assert_ptr_nonnull(parent);

  for (size_t i = 0; i < count; ++i)
  {
    char name[128];
    sprintf(name, "F_%05u.TXT", (unsigned int)i);

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
  }

  fsNodeFree(parent);
}
/*----------------------------------------------------------------------------*/
void makeNode(struct FsHandle *handle, const char *path, bool dir, bool ro)
{
  const time64_t timestamp = RTC_INITIAL_TIME * 1000000;
  const FsAccess access = ro ?
      FS_ACCESS_READ : (FS_ACCESS_READ | FS_ACCESS_WRITE);

  const char * const name = fsExtractName(path);
  ck_assert_ptr_nonnull(name);

  struct FsNode * const parent = fsOpenBaseNode(handle, path);
  ck_assert_ptr_nonnull(parent);

  const struct FsFieldDescriptor desc[] = {
      {
          name,
          strlen(name) + 1,
          FS_NODE_NAME
      }, {
          &access,
          sizeof(access),
          FS_NODE_ACCESS
      }, {
          &timestamp,
          sizeof(timestamp),
          FS_NODE_TIME
      }, {
          NULL,
          0,
          FS_NODE_DATA
      }
  };
  const size_t count = dir ? ARRAY_SIZE(desc) - 1: ARRAY_SIZE(desc);
  const enum Result res = fsNodeCreate(parent, desc, count);
  ck_assert_uint_eq(res, E_OK);
  fsNodeFree(parent);

  struct FsNode *node = fsOpenNode(handle, path);
  ck_assert_ptr_nonnull(node);
  fsNodeFree(node);
}
/*----------------------------------------------------------------------------*/
struct TestContext makeTestHandle(void)
{
  static const struct VirtualMemConfig vmemConfig = {
      .size = FS_TOTAL_SIZE
  };
  struct Interface * const vmem = init(VirtualMem, &vmemConfig);
  ck_assert_ptr_nonnull(vmem);

  static const struct Fat32FsConfig makeFsConfig =  {
      .cluster = FS_CLUSTER_SIZE,
      .reserved = 0,
      .tables = FS_TABLE_COUNT,
      .label = "TEST"
  };
  const enum Result res = fat32MakeFs(vmem, &makeFsConfig, NULL, 0);
  ck_assert_uint_eq(res, E_OK);

  const struct Fat32Config fsConfig = {
      .interface = vmem,
      .nodes = FS_NODE_POOL_SIZE,
      .threads = FS_THREAD_POOL_SIZE
  };

  struct FsHandle * const handle = init(FatHandle, &fsConfig);
  ck_assert_ptr_nonnull(handle);

  /* Level 1 inside "/" */
  makeNode(handle, PATH_BOOT, true, false);
  makeNode(handle, PATH_HOME, true, false);
  makeNode(handle, PATH_LIB, true, false);
  makeNode(handle, PATH_SYS, true, false);

  /* Level 2 inside "/HOME" */
  makeNode(handle, PATH_HOME_ROOT, true, false);
  makeNode(handle, PATH_HOME_USER, true, false);

  /* Level 3 inside "/HOME/ROOT" directory */
  makeNode(handle, PATH_HOME_ROOT_ALIG, false, false);
  makeNode(handle, PATH_HOME_ROOT_UNALIG, false, false);
  makeNode(handle, PATH_HOME_ROOT_NOEXT, false, false);
  makeNode(handle, PATH_HOME_ROOT_RO, false, true);
  makeNode(handle, PATH_HOME_ROOT_SHORT, false, false);

  /* Level 3 inside "/HOME/USER" directory */
  makeNode(handle, PATH_HOME_USER_TEMP1, false, false);
  makeNode(handle, PATH_HOME_USER_TEMP2, false, false);
  makeNode(handle, PATH_HOME_USER_TEMP3, false, false);
  makeNode(handle, PATH_HOME_USER_TEMP4, false, false);

  /* Prepare data streams */
  prepareNodeData(handle, PATH_HOME_ROOT_ALIG, ALIG_FILE_SIZE);
  prepareNodeData(handle, PATH_HOME_ROOT_UNALIG, UNALIG_FILE_SIZE);

  return (struct TestContext){
      .interface = vmem,
      .handle = handle
  };
}
