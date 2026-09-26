#include "logging/csv_logger.h"

#include "config/board_config.h"

#include <Arduino.h>

namespace ppgfw {

void CsvLogger::begin() {
#if APP_ENABLE_CSV_LOG
    Serial.println("type,timestamp_us,sequence,value_a,value_b,flags");
#endif
}

void CsvLogger::log(const PpgSample& sample) {
#if APP_ENABLE_CSV_LOG
    if (Serial.availableForWrite() < 96) {
        ++dropped_;
        return;
    }
    Serial.printf("PPG,%llu,%lu,%lu,%lu,%u\n",
                  static_cast<unsigned long long>(sample.timestamp_us),
                  static_cast<unsigned long>(sample.seq),
                  static_cast<unsigned long>(sample.red),
                  static_cast<unsigned long>(sample.ir), sample.flags);
#else
    (void)sample;
#endif
}

void CsvLogger::log(const EcgSample& sample) {
#if APP_ENABLE_CSV_LOG
    if (Serial.availableForWrite() < 80) {
        ++dropped_;
        return;
    }
    Serial.printf("ECG,%llu,%lu,%d,0,%u\n",
                  static_cast<unsigned long long>(sample.timestamp_us),
                  static_cast<unsigned long>(sample.seq), sample.raw, sample.flags);
#else
    (void)sample;
#endif
}

uint32_t CsvLogger::dropped() const {
    return dropped_;
}

}
