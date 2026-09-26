#pragma once

#include "types/metric_types.h"

#include <cstddef>

namespace ppgfw {

MetricValue heartRateFromIntervals(const float* intervals_ms, size_t count);

}

