#pragma once

#include "network/mqtt_client.h"
#include "types/result_types.h"

namespace ppgfw {

class TelemetryPublisher {
public:
    explicit TelemetryPublisher(MqttClient& client);
    void publish(const ResultSnapshot& result);

private:
    MqttClient& client_;
};

}

