#include <unity.h>

#include "utils/spsc_ring_buffer.h"

using ppgfw::SpscRingBuffer;

void test_fifo_order_and_capacity() {
    SpscRingBuffer<int, 4> queue;
    TEST_ASSERT_TRUE(queue.push(10));
    TEST_ASSERT_TRUE(queue.push(20));
    TEST_ASSERT_TRUE(queue.push(30));
    TEST_ASSERT_FALSE(queue.push(40));
    TEST_ASSERT_EQUAL_UINT32(3, queue.highWaterMark());
    int value = 0;
    TEST_ASSERT_TRUE(queue.pop(value));
    TEST_ASSERT_EQUAL_INT(10, value);
    TEST_ASSERT_TRUE(queue.push(40));
    TEST_ASSERT_TRUE(queue.pop(value));
    TEST_ASSERT_EQUAL_INT(20, value);
    TEST_ASSERT_TRUE(queue.pop(value));
    TEST_ASSERT_EQUAL_INT(30, value);
    TEST_ASSERT_TRUE(queue.pop(value));
    TEST_ASSERT_EQUAL_INT(40, value);
    TEST_ASSERT_FALSE(queue.pop(value));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fifo_order_and_capacity);
    return UNITY_END();
}
