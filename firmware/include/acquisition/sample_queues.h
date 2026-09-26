#pragma once

#include "config/sampling.h"
#include "types/raw_samples.h"
#include "utils/spsc_ring_buffer.h"

namespace ppgfw {

using PpgSampleQueue = SpscRingBuffer<PpgSample, config::kPpgQueueCapacity>;
using EcgSampleQueue = SpscRingBuffer<EcgSample, config::kEcgQueueCapacity>;

}

