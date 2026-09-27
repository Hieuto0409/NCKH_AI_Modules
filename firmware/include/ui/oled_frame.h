#pragma once
#include "app/measurement_state_machine.h"
#include "types/result_types.h"
#include "types/upload_status.h"
#include <array>
namespace ppgfw {
// Six rows of at most 21 ASCII glyphs at 6x10 on a 128x64 display.
struct OledFrame { std::array<std::array<char, 22>, 6> rows{}; };
OledFrame makeOledFrame(MeasurementState state, const ResultSnapshot& result,
    bool ppg_available, bool contact, bool lead_off, uint8_t page, UploadSnapshot upload);
}
