#include "Camera.h"
#include <sstream>


std::string Camera::pipeline(uint8_t sensorId, uint32_t captureWidth, uint32_t captureHeight, uint32_t displayWidth, uint32_t displayHeight,
                             uint32_t frameRate, uint32_t flipMethod) {
    std::stringstream ss;

    ss << "nvarguscamerasrc sensor-id="s << static_cast<uint16_t>(sensorId);
    ss << " ! video/x-raw(memory:NVMM), width=(int)"s << captureWidth << ", height=(int)"s << captureHeight << ", framerate=(fraction)"s
       << static_cast<uint16_t>(frameRate) << "/1"s;
    ss << " ! nvvidconv flip-method="s << static_cast<uint16_t>(flipMethod);
    ss << " ! video/x-raw, width=(int)"s << displayWidth << ", height=(int)"s << displayHeight << ", format=(string)BGRx"s;
    ss << " ! videoconvert"s;
    ss << " ! video/x-raw, format=(string)BGR"s;
    ss << " ! appsink"s;

    return ss.str();
}
