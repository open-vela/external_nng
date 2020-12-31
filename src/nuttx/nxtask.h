/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    nxtask.h
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-12-30 22:59:30
 */

#ifndef __NXTASK_H__
#define __NXTASK_H__
#ifdef __cplusplus
extern "C" {
#endif

typedef void(*nxtask)(void*);

/**
 * @brief:nxtask_delay
 *
 * @param ms
 * @param task
 * @param arg
 *
 * @return 0 if can be scheduled
 */
int nxtask_delay(int ms, nxtask task, void* arg);

/**
 * @brief:nxtask_async
 *
 * @param task
 * @param arg
 *
 * @return
 */
int nxtask_async(nxtask task, void* arg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __NXTASK_H__ */

