/*
 * main.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the MIT License
 */

#include <yaf/fat32_helpers.h>
#include <yaf/pointer_array.h>
#include <check.h>
/*----------------------------------------------------------------------------*/
#define MAX_CAPACITY 17
/*----------------------------------------------------------------------------*/
START_TEST(testArrayErasure)
{
  PointerArray array;

  const bool res = pointerArrayInit(&array, MAX_CAPACITY);
  ck_assert_uint_eq(res, true);

  for (size_t i = 0; i < pointerArrayCapacity(&array); ++i)
    pointerArrayPushBack(&array, (void *)i);

  pointerArrayEraseBySwap(&array, 0);
  ck_assert(*pointerArrayAt(&array, 0) == (void *)(MAX_CAPACITY - 1));

  pointerArrayDeinit(&array);
}
END_TEST
/*----------------------------------------------------------------------------*/
START_TEST(testTimeConversion)
{
  static const uint16_t date = 1 + (13 << 5) + (40 << 9);
  static const uint16_t time = 0;
  time64_t timestamp;

  const bool res = rawDateTimeToTimestamp(&timestamp, date, time);
  ck_assert_uint_eq(res, false);
}
END_TEST
/*----------------------------------------------------------------------------*/
int main(void)
{
  Suite * const suite = suite_create("SystemFuncs");
  TCase * const testcase = tcase_create("Core");

  tcase_add_test(testcase, testArrayErasure);
  tcase_add_test(testcase, testTimeConversion);
  suite_add_tcase(suite, testcase);

  SRunner * const runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  const int failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  return failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
