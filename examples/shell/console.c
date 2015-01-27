/*
 * console.c
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <stdio.h>
#include "shell/console.h"
/*----------------------------------------------------------------------------*/
static enum result consoleInit(void *, const void *);
static void consoleDeinit(void *);
static enum result consoleCallback(void *, void (*)(void *), void *);
static enum result consoleGet(void *, enum ifOption, void *);
static enum result consoleSet(void *, enum ifOption, const void *);
static uint32_t consoleRead(void *, uint8_t *, uint32_t);
static uint32_t consoleWrite(void *, const uint8_t *, uint32_t);
/*----------------------------------------------------------------------------*/
struct Console
{
  struct Interface parent;
};
/*----------------------------------------------------------------------------*/
static const struct InterfaceClass consoleTable = {
    .size = sizeof(struct Console),
    .init = consoleInit,
    .deinit = consoleDeinit,

    .callback = consoleCallback,
    .get = consoleGet,
    .set = consoleSet,
    .read = consoleRead,
    .write = consoleWrite
};
/*----------------------------------------------------------------------------*/
const struct InterfaceClass * const Console = &consoleTable;
/*----------------------------------------------------------------------------*/
static enum result consoleInit(void *object __attribute__((unused)),
    const void *configBase __attribute__((unused)))
{
  return E_OK;
}
//------------------------------------------------------------------------------
static void consoleDeinit(void *object __attribute__((unused)))
{

}
//------------------------------------------------------------------------------
static enum result consoleCallback(void *object __attribute__((unused)),
    void (*callback)(void *) __attribute__((unused)),
    void *argument __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static enum result consoleGet(void *object __attribute__((unused)),
    enum ifOption option __attribute__((unused)),
    void *data __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static enum result consoleSet(void *object __attribute__((unused)),
    enum ifOption option __attribute__((unused)),
    const void *data __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static uint32_t consoleRead(void *object __attribute__((unused)),
    uint8_t *buffer __attribute__((unused)),
    uint32_t length __attribute__((unused)))
{
  return 0;
}
//------------------------------------------------------------------------------
static uint32_t consoleWrite(void *object __attribute__((unused)),
    const uint8_t *buffer, uint32_t length)
{
  printf((const char *)buffer);

  return length;
}
