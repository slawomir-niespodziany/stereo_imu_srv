#include "TcpServer.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <iostream>
#include <string>

using namespace std::string_literals;

TcpServer::TcpServer(uint16_t port) : port_{port}, srvFd_{-1}, clientFd_{-1}, buffer_(64u * 1024u * 1024u), size_{0u}, n_{0u} {
    srvFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > srvFd_) {
        throw std::runtime_error("socket() failed"s);
    }

    struct sockaddr_in sockAddrIn = {.sin_family = AF_INET, .sin_port = htons(port_), .sin_addr = {.s_addr = INADDR_ANY}, .sin_zero = {}};
    if (0 != bind(srvFd_, (struct sockaddr *) &sockAddrIn, sizeof(sockAddrIn))) {
        throw std::runtime_error("bind() failed"s);
    }

    if (0 != listen(srvFd_, 1)) {
        throw std::runtime_error("listen() failed, errno="s + std::to_string(errno) + " ("s + std::string(strerror(errno)) + ")"s);
    }
}

void TcpServer::process() {
    if (-1 != clientFd_) {
        fd_set fdSetRd, fdSetWr;

        FD_ZERO(&fdSetRd);
        FD_SET(clientFd_, &fdSetRd);

        FD_ZERO(&fdSetWr);
        if (n_ >= size_) {
            n_ = 0u;
            size_ = 0u;
            for (auto &dataSource : dataSources_) {
                if(--dataSourcesCooldowns_[dataSource.first] == 0) {
                    //std::cout << "data source ready\n";
                    dataSourcesCooldowns_[dataSource.first] = dataSource.first;
                    size_ = dataSource.second.get().read(buffer_);
                    if (0u != size_) {
                        break;
                    }
                }  
            }
        }
        if (n_ < size_) {
            FD_SET(clientFd_, &fdSetWr);
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if (0 < select(clientFd_ + 1, &fdSetRd, &fdSetWr, nullptr, &tv)) {
            if (FD_ISSET(clientFd_, &fdSetRd)) {
                uint8_t msg[32];
                ssize_t size = read(clientFd_, msg, std::extent<decltype(msg)>::value);
                if (0 <= size) {
                    bool handled = false;
                    std::cout << "handling msg = " << msg << std::endl;
                    for (auto &msgHandler : msgHandlers_) {
                        handled = msgHandler.second.get().handle_msg(msg, std::extent<decltype(msg)>::value) || handled;
                    }
                    if (!handled && size == 0) {
                        // if not known message, then FIN
                        close(clientFd_);
                        clientFd_ = -1;

                        for (auto &dataSource : dataSources_) {
                            dataSource.second.get().open(false);
                        }

                        std::cout << "close()" << std::endl;
                    }
                }
            } else if (FD_ISSET(clientFd_, &fdSetWr)) {
                int result = send(clientFd_, &buffer_[n_], size_ - n_, MSG_NOSIGNAL);

                if (0 < result) {
                    n_ += result;

                } else {
                    close(clientFd_);
                    clientFd_ = -1;

                    for (auto &dataSource : dataSources_) {
                        dataSource.second.get().open(false);
                    }

                    std::cerr << "send() failed" << std::endl;
                }
            }
        }

    } else {   // accept client
        fd_set fdSetRd;
        FD_ZERO(&fdSetRd);
        FD_SET(srvFd_, &fdSetRd);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if ((0 < select(srvFd_ + 1, &fdSetRd, nullptr, nullptr, &tv)) && FD_ISSET(srvFd_, &fdSetRd)) {
            struct sockaddr_in clientAddress;
            socklen_t clientAddressLen = sizeof(clientAddress);

            clientFd_ = accept(srvFd_, (struct sockaddr *) &clientAddress, &clientAddressLen);
            n_ = 0u;
            size_ = 0u;

            for (auto &dataSource : dataSources_) {
                dataSource.second.get().open(true);
            }

            uint32_t ip = ntohl(clientAddress.sin_addr.s_addr);
            uint16_t port = ntohs(clientAddress.sin_port);
            std::cout << "accept() ok, peer=" << (0xFFu & (ip >> 24u)) << '.' << (0xFFu & (ip >> 16u)) << '.' << (0xFFu & (ip >> 8u)) << '.'
                      << (0xFFu & (ip >> 0u)) << ':' << port << std::endl;
        }
    }
}
