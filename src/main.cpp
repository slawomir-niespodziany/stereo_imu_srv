#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include "StereoCamera.h"
#include "ImuDataSource.h"
#include "StereoDataSource.h"
#include "TcpServer.h"

using namespace std::string_literals;

// static const uint16_t mg_port = 12000;
static const std::string mg_path = "/dev/stereo_imu"s;
static bool mg_stop = false;

void signal_handler(int) { mg_stop = true; }

int main(/* int argc, char* argv[] */) {
    signal(SIGINT, signal_handler);
    std::srand(std::time(nullptr));

    TcpServer server;
    // server.addDataSource(64u, ImuDataSource::getInstance());
    // server.addDataSource(32u, StereoDataSource::getInstance());

    // server.addMsgHandler(32u, StereoDataSource::getInstance());

    server.addDataSource(32u, StereoCamera::getInstance());
    server.addMsgHandler(32u, StereoCamera::getInstance());

    while (!mg_stop) {
        server.process();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
