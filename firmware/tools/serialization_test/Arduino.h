#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
struct FakeSerial {
    std::vector<uint8_t> bytes;
    int availableForWrite() {return 65536;}
    size_t write(const uint8_t* data,size_t count) {bytes.insert(bytes.end(),data,data+count);return count;}
};
extern FakeSerial Serial;
