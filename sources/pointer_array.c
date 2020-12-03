/*
 * pointer_array.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

 #include <yaf/pointer_array.h>
 /*----------------------------------------------------------------------------*/
void pointerArrayEraseBySwap(PointerArray *array, size_t index)
{
  assert(index < array->size);

  const size_t last = array->size - 1;

  if (index != last)
    array->data[index] = array->data[last];

  --array->size;
}
