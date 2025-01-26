#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class DataSource {
public:
    virtual ~DataSource() = default;

    virtual void open(bool enable) = 0;
    virtual uint32_t read(std::vector<uint8_t> &buffer) = 0;
};

class MsgHandler {
public:
    virtual ~MsgHandler() = default;

    virtual bool handle_msg(const uint8_t *msg, size_t len) = 0;
};
