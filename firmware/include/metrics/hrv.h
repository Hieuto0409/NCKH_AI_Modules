#pragma once

#include "types/metric_types.h"

#include <cstddef>

namespace ppgfw {

HrvMetrics calculateHrv(const float* intervals_ms, size_t count);

}

