#include "drivers/ads1115_backend.h"

namespace ppgfw {

bool Ads1115Backend::begin() {
    return false;
}

int16_t Ads1115Backend::readRaw() {
    return 0;
}

bool Ads1115Backend::leadOff() const {
    return true;
}

const char* Ads1115Backend::name() const {
    return "ADS1115_AB_TEST_NOT_CONFIGURED";
}

}

