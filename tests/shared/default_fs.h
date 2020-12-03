/*
 * yaf/tests/shared/default_fs.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_TESTS_SHARED_DEFAULT_FS_H_
#define YAF_TESTS_SHARED_DEFAULT_FS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/fs/fs.h>
#include <xcore/interface.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
/* January 1, 2020, 12:00:00 */
#define RTC_INITIAL_TIME      1577836800LL
/*----------------------------------------------------------------------------*/
#define FS_CLUSTER_SIZE       (CONFIG_SECTOR_SIZE * 2)
#define FS_NODE_POOL_SIZE     4
#define FS_TABLE_COUNT        2
#define FS_THREAD_POOL_SIZE   0
#define FS_TOTAL_SIZE         (16 * 1024 * 1024)
/*----------------------------------------------------------------------------*/
#define MAX_BUFFER_LENGTH     CONFIG_SECTOR_SIZE

#define ALIG_FILE_SIZE        (FS_CLUSTER_SIZE * 4)
#define UNALIG_FILE_SIZE      (FS_CLUSTER_SIZE * 8 / 3)
/*----------------------------------------------------------------------------*/
#define PATH_BOOT             "/BOOT"
#define PATH_HOME             "/HOME"
#define PATH_LIB              "/LIB"
#define PATH_SYS              "/SYS"

#define PATH_HOME_ROOT        "/HOME/ROOT"
#define PATH_HOME_USER        "/HOME/USER"

#define PATH_HOME_ROOT_ALIG   "/HOME/ROOT/ALIG.TXT"
#define PATH_HOME_ROOT_UNALIG "/HOME/ROOT/UNALIG.TXT"
#define PATH_HOME_ROOT_NOEXT  "/HOME/ROOT/NOEXT"
#define PATH_HOME_ROOT_RO     "/HOME/ROOT/RO.EXE"
#define PATH_HOME_ROOT_SHORT  "/HOME/ROOT/SHORT.A"

#define PATH_HOME_USER_TEMP1  "/HOME/USER/TEMP1.TXT"
#define PATH_HOME_USER_TEMP2  "/HOME/USER/TEMP2.TXT"
#define PATH_HOME_USER_TEMP3  "/HOME/USER/TEMP3.TXT"
#define PATH_HOME_USER_TEMP4  "/HOME/USER/TEMP4.TXT"
/*----------------------------------------------------------------------------*/
struct TestContext
{
  struct Interface *interface;
  struct FsHandle *handle;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void freeFillingNodes(struct FsHandle *, const char *, size_t);
void freeNode(struct FsHandle *, const char *);
void freeTestHandle(struct TestContext);
void makeFillingNodes(struct FsHandle *, const char *, size_t);
void makeNode(struct FsHandle *, const char *, bool, bool);
struct TestContext makeTestHandle(void);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* YAF_TESTS_SHARED_DEFAULT_FS_H_ */
