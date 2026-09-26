#pragma once

#include "types/result_types.h"

namespace ppgfw {

class DecisionAggregator {
public:
    static ResultSnapshot aggregate(uint64_t timestamp_us, const FeaturePacket& packet,
                                    const BranchResult& stress, const BranchResult& low_o2,
                                    const BranchResult& rhythm);
};

}

