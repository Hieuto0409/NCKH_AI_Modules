#ifndef MAX30102_H
#define MAX30102_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX30102_ADDR 0x57U
#define MAX30102_REG_INTR_STATUS_1 0x00U
#define MAX30102_REG_INTR_ENABLE_1 0x02U
#define MAX30102_REG_FIFO_WR_PTR 0x04U
#define MAX30102_REG_OVF_COUNTER 0x05U
#define MAX30102_REG_FIFO_RD_PTR 0x06U
#define MAX30102_REG_FIFO_DATA 0x07U
#define MAX30102_REG_FIFO_CONFIG 0x08U
#define MAX30102_REG_MODE_CONFIG 0x09U
#define MAX30102_REG_SPO2_CONFIG 0x0AU
#define MAX30102_REG_LED1_PA 0x0CU /* RED */
#define MAX30102_REG_LED2_PA 0x0DU /* IR */
#define MAX30102_REG_PART_ID 0xFFU
#define MAX30102_PART_ID_VALUE 0x15U

typedef struct {
    bool initialized;
    uint32_t sample_count;
    uint32_t overflow_count;
    uint8_t part_id;
} Max30102State;

bool max30102_init(Max30102State *state);
bool max30102_read_sample(Max30102State *state,
                          uint32_t *red,
                          uint32_t *ir);
uint8_t max30102_fifo_count(const Max30102State *state);

#ifdef __cplusplus
}
#endif

#endif /* MAX30102_H */
