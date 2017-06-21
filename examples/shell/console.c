/*
 * console.c
 * Copyright (C) 2015 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <stdio.h>
#include <stdint.h>
#include "shell/console.h"
/*----------------------------------------------------------------------------*/
static enum Result consoleInit(void *, const void *);
static void consoleDeinit(void *);
static enum Result consoleSetCallback(void *, void (*)(void *), void *);
static enum Result consoleGetParam(void *, enum IfParameter, void *);
static enum Result consoleSetParam(void *, enum IfParameter, const void *);
static size_t consoleRead(void *, void *, size_t);
static size_t consoleWrite(void *, const void *, size_t);
/*----------------------------------------------------------------------------*/
struct Console
{
  struct Interface base;
};
/*----------------------------------------------------------------------------*/
static const struct InterfaceClass consoleTable = {
    .size = sizeof(struct Console),
    .init = consoleInit,
    .deinit = consoleDeinit,

    .setCallback = consoleSetCallback,
    .getParam = consoleGetParam,
    .setParam = consoleSetParam,
    .read = consoleRead,
    .write = consoleWrite
};
/*----------------------------------------------------------------------------*/
const struct InterfaceClass * const Console = &consoleTable;
/*----------------------------------------------------------------------------*/
static enum Result consoleInit(void *object __attribute__((unused)),
    const void *configBase __attribute__((unused)))
{
  return E_OK;
}
//------------------------------------------------------------------------------
static void consoleDeinit(void *object __attribute__((unused)))
{

}
//------------------------------------------------------------------------------
static enum Result consoleSetCallback(void *object __attribute__((unused)),
    void (*callback)(void *) __attribute__((unused)),
    void *argument __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static enum Result consoleGetParam(void *object __attribute__((unused)),
    enum IfParameter option __attribute__((unused)),
    void *data __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static enum Result consoleSetParam(void *object __attribute__((unused)),
    enum IfParameter option __attribute__((unused)),
    const void *data __attribute__((unused)))
{
  return E_ERROR;
}
//------------------------------------------------------------------------------
static size_t consoleRead(void *object __attribute__((unused)),
    void *buffer __attribute__((unused)),
    size_t length __attribute__((unused)))
{
  return 0;
}
//------------------------------------------------------------------------------
static size_t consoleWrite(void *object __attribute__((unused)),
    const void *buffer, size_t length)
{
  printf("%s", (const char *)buffer);
  return length;
}
