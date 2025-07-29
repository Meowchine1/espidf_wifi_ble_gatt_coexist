/*
 * test_motor_control_protocol.c
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

// test/test_motor_control_protocol.c

#include "unity.h"
#include "motor_control_protocol.h"

static mcps_context_t ctx;

void test_connection_establishment(void)
{
    ctx.uart_port = 1;
    TEST_ASSERT_TRUE(mcps_start_connection(&ctx));
    TEST_ASSERT_TRUE(ctx.connected);
}

void app_main()
{
    UNITY_BEGIN();
    RUN_TEST(test_connection_establishment);
    UNITY_END();
}



