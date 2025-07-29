/*
 * test_aspep.c
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

// test/test_aspep.c

#include "unity.h"
#include "aspep.h"

void test_crch_calculation(void)
{
    uint8_t header[3] = {0xF1, 0x23, 0x4C};
    uint8_t crch = calculate_crch(header);
    TEST_ASSERT_EQUAL(0x4, crch);  // Предполагаемое значение CRC (нужно будет откалибровать)
}

void app_main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crch_calculation);
    UNITY_END();
}



