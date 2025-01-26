#include "ImuDataSource.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

void ImuDataSource::open(bool enable) {
    if (-1 != fd_) {
        close(fd_);
        fd_ = -1;
    }

    if (true == enable) {
        int fd = ::open(PATH.c_str(), O_RDONLY);
        if (0 > fd) {
            throw std::runtime_error("open(\""s + PATH + "\") failed, errno="s + std::to_string(errno) + " ("s +
                                     std::string(strerror(errno)) + ")"s);
        }
        fd_ = fd;
        size_ = 0u;
    }
}

uint32_t ImuDataSource::read(std::vector<uint8_t>& buffer) {
    uint32_t result = 0u;

    std::cout << "reading magnetometer\n";

    if (size_ < buffer.size()) {
        uint32_t n = size_ + read_(&buffer[size_], buffer.size() - size_);
        if (size_ < n) {
            uint32_t end = size_;
            for (uint32_t idx1 = n, idx0 = idx1 - 1u; size_ < idx1; idx0--, idx1--) {
                if (static_cast<uint8_t>('\n') == buffer[idx0]) {
                    end = idx1;
                    break;
                }
            }

            if (end != size_) {
                std::memcpy(&buffer[0], &buffer_[0], size_);
                size_ = n - end;
                std::memcpy(&buffer_[0], &buffer[end], size_);
                result = end;
            } else {   // no delimiter
                if (buffer_.size() < n) {
                    buffer_.resize(n);
                }

                std::memcpy(&buffer_[size_], &buffer[size_], n - size_);
                size_ += n;
            }
        }
    }

    return result;
}

uint32_t ImuDataSource::read_(uint8_t* pBuffer, uint32_t size) {
    uint32_t n = 0u;

    if (-1 != fd_) {
        fd_set fdSetRd;
        FD_ZERO(&fdSetRd);
        FD_SET(fd_, &fdSetRd);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if ((0 < select(fd_ + 1, &fdSetRd, nullptr, nullptr, &tv)) && FD_ISSET(fd_, &fdSetRd)) {
            int result = ::read(fd_, pBuffer, size);

            if (0 < result) {
                n = result;
            } else {
                close(fd_);
                fd_ = -1;
            }
        }
    }

    return n;
}