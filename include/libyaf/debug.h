/*
 * libyaf/debug.h
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LIBYAF_DEBUG_H_
#define LIBYAF_DEBUG_H_
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_DEBUG
#include <inttypes.h>
#include <stdio.h>

#ifndef CONFIG_DEBUG_LEVEL
#define CONFIG_DEBUG_LEVEL 0
#endif

#define DEBUG_PRINT(level, ...) \
    do { if ((level) <= CONFIG_DEBUG_LEVEL) printf(__VA_ARGS__); } while (0)
#else
#define DEBUG_PRINT(...) \
    do {} while (0)
#endif
/*----------------------------------------------------------------------------*/
#endif /* LIBYAF_DEBUG_H_ */
