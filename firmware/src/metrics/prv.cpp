#include "metrics/prv.h"

#include "metrics/hrv.h"

namespace ppgfw {

HrvMetrics calculatePrv(const float* intervals_ms, size_t count) {
    return calculateHrv(intervals_ms, count);
}

}

