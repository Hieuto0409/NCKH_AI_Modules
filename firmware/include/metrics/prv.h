#pragma once

#include "types/metric_types.h"

#include <cstddef>

namespace ppgfw {

HrvMetrics calculatePrv(const float* intervals_ms, size_t count);

}

