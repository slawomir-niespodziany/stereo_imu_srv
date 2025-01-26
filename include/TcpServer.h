#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <set>
#include <map>
#include <utility>
#include "DataSource.h"

class TcpServer {
public:
    TcpServer(uint16_t port = 12000u);
    virtual ~TcpServer() {
        if (-1 != clientFd_) {
            shutdown(clientFd_, SHUT_RDWR);
            close(clientFd_);
            clientFd_ = -1;
        }
        if (-1 != srvFd_) {
            shutdown(srvFd_, SHUT_RDWR);
            close(srvFd_);
            srvFd_ = -1;
        }
        for (auto &dataSource : dataSources_) {
            dataSource.second.get().open(false);
        }
    }

    void addDataSource(int8_t priority, DataSource &dataSource) { dataSources_.emplace(priority, dataSource); dataSourcesCooldowns_[priority] = priority; }
    void addMsgHandler(int8_t priority, MsgHandler &msgHandler) { msgHandlers_.emplace(priority, msgHandler); }
    void process();

private:
    const uint16_t port_;
    int srvFd_, clientFd_;

    std::vector<uint8_t> buffer_;
    uint32_t size_, n_;

    struct Comparator {
        template <typename T>
        bool operator()(const std::pair<uint8_t, T> &first,
                        const std::pair<uint8_t, T> &second) const {
            return first.first < second.first;
        }
    };

    std::set<std::pair<uint8_t, std::reference_wrapper<DataSource>>, Comparator> dataSources_;
    std::set<std::pair<uint8_t, std::reference_wrapper<MsgHandler>>, Comparator> msgHandlers_;

    std::map<uint8_t, uint8_t> dataSourcesCooldowns_;
};
