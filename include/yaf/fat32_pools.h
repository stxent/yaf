/*
 * yaf/fat32_pools.h
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_FAT32_POOLS_H_
#define YAF_FAT32_POOLS_H_
/*----------------------------------------------------------------------------*/
bool allocatePool(struct Pool *, size_t, size_t);
struct CommandContext *allocatePoolContext(struct FatHandle *);
void *allocatePoolNode(struct FatHandle *);
void allocateStaticNode(struct FatHandle *, struct FatNode *);
void freePool(struct Pool *);
void freePoolContext(struct FatHandle *, struct CommandContext *);
void freePoolNode(struct FatNode *);
void freeStaticNode(struct FatNode *);
/*----------------------------------------------------------------------------*/
#endif /* YAF_FAT32_POOLS_H_ */
