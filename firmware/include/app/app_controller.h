#pragma once

#include "acquisition/ecg_acquisition.h"
#include "acquisition/ppg_acquisition.h"
#include "ai/decision_aggregator.h"
#include "ai/low_o2_engine.h"
#include "ai/rhythm_engine.h"
#include "ai/stress_engine.h"
#include "app/measurement_state_machine.h"
#include "config/pins.h"
#include "drivers/button_driver.h"
#include "drivers/esp32_adc_backend.h"
#include "drivers/max30102_driver.h"
#include "features/feature_builder.h"
#include "logging/binary_logger.h"
#include "logging/csv_logger.h"
#include "logging/telemetry_logger.h"
#include "network/mqtt_client.h"
#include "network/telemetry_publisher.h"
#include "ui/oled_ui.h"

namespace ppgfw {

class AppController {
public:
    void begin();
    void loop();

private:
    bool runSelfTest(uint32_t now_ms);
    void consumeSamples(bool include_in_measurement);
    void startMeasurement(uint64_t now_us);
    void finishMeasurement(uint64_t now_us, uint32_t now_ms);
    void updatePresentation(uint32_t now_ms);

    Max30102Driver ppg_driver_{};
    Esp32AdcBackend ecg_backend_{};
    PpgSampleQueue ppg_queue_{};
    EcgSampleQueue ecg_queue_{};
    PpgAcquisition ppg_acquisition_{ppg_driver_, ppg_queue_};
    EcgAcquisition ecg_acquisition_{ecg_backend_, ecg_queue_};
    ButtonDriver start_button_{config::pins::kButton1};
    ButtonDriver cancel_button_{config::pins::kButton2};
    OledUi oled_{};
    MeasurementStateMachine state_machine_{};
    FeatureBuilder feature_builder_{};
    StressEngine stress_engine_{};
    LowO2Engine low_o2_engine_{};
    RhythmEngine rhythm_engine_{};
    BinaryLogger binary_logger_{};
    CsvLogger csv_logger_{};
    TelemetryLogger telemetry_logger_{};
    MqttClient mqtt_client_{};
    TelemetryPublisher telemetry_publisher_{mqtt_client_};
    ResultSnapshot result_{};
    MeasurementState previous_state_{MeasurementState::Boot};
    uint32_t last_oled_update_ms_{};
    uint32_t last_telemetry_ms_{};
};

}
