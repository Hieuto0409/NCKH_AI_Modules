#pragma once
#include <cstddef>
#include <cstdint>
namespace ppgfw {
enum class UploadStage : uint8_t {
    Offline, NotConfigured, Ready, Queued, Wifi, Clock, Sending, Sent, Deferred, Rejected
};
struct UploadSnapshot {
    UploadStage stage{UploadStage::Offline};
    size_t pending{};
    uint32_t acknowledged{};
    uint32_t rejected{};
};
}
