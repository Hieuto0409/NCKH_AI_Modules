#ifndef AD8232_H
#define AD8232_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool initialized;
    uint32_t sample_count;
    uint16_t lo_plus;
    uint16_t lo_minus;
    bool leads_off;
} Ad8232State;

bool ad8232_init(Ad8232State *state);
/* Returns true when an ADC sample was read. `state->leads_off` is separate;
 * leads-off must not create a fake timestamp gap by itself. */
bool ad8232_read_sample(Ad8232State *state, uint16_t *ecg_raw);

#ifdef __cplusplus
}
#endif

#endif /* AD8232_H */
