/*
 * yaf/debug.h
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef YAF_DEBUG_H_
#define YAF_DEBUG_H_
/*----------------------------------------------------------------------------*/
#if CONFIG_DEBUG != 0
#include <inttypes.h>
#include <stdio.h>

#define DEBUG_PRINT(level, ...) \
    do { if ((level) <= CONFIG_DEBUG) printf(__VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) \
    do {} while (0)
#endif
/*----------------------------------------------------------------------------*/
#endif /* YAF_DEBUG_H_ */
