#pragma once

#include <array>
#include <chrono>
#include <sstream>
#include "Camera.h"
#include "DataSource.h"
#include <dirent.h>

using namespace std::string_literals;

inline std::vector<std::string> allFilesIn(const std::string& directory) {
    std::vector<std::string> ret;
    DIR* dirp = opendir(directory.c_str());
    if (dirp == NULL) {
        system(("mkdir -p " + directory).c_str());
        return {};
    }
    struct dirent* dp;
    while ((dp = readdir(dirp)) != NULL) {
        if (strcmp(".", dp->d_name) != 0 && strcmp("..", dp->d_name) != 0) {
            ret.push_back(dp->d_name);
        }
    }
    closedir(dirp);
    return ret;
}

class StereoDataSource : public DataSource, public MsgHandler {
public:
    StereoDataSource() : left_{0u, allFilesIn("calibration/default/left")}, right_{1u, allFilesIn("calibration/default/right")} {}
    virtual ~StereoDataSource() = default;

    static StereoDataSource& getInstance() {
        static StereoDataSource instance;
        return instance;
    }

    virtual bool handle_msg(const uint8_t *msg, size_t len) {
        if (strncmp("do_photo", (char*)msg, len) == 0) {
            left_.camera.savePhoto();
            right_.camera.savePhoto();
            return true;
        }
        if (strncmp("calibrate", (char*)msg, len) == 0) {
            left_.camera.recalibrate();
            right_.camera.recalibrate();
            return true;
        }
        if (strncmp("clear_photos", (char*)msg, len) == 0) {
            left_.camera.resetPhotos();
            right_.camera.resetPhotos();
            return true;
        }
        return false;
    }

    virtual void open(bool enable) {
        if (enable) {
            left_.valid = false;
            right_.valid = false;
        }
    }
    virtual uint32_t read(std::vector<uint8_t>& buffer) {
        uint32_t result = 0u;

        if (!left_.valid) {
            left_.valid = left_.camera.read(left_.ts, left_.data);
        }
        if (!right_.valid) {
            right_.valid = right_.camera.read(right_.ts, right_.data);
        }

        if (left_.valid && right_.valid) {
            uint32_t leftSize = left_.data.size();
            uint32_t rightSize = right_.data.size();

            std::vector<uint8_t> header;
            {
                std::stringstream ss;
                ss << std::setfill('0') << "ts="s << std::setw(11)
                   << std::chrono::duration_cast<std::chrono::milliseconds>(left_.ts.time_since_epoch()).count()
                   << ", type=DATA_STEREO, left="s << std::setw(8) << leftSize << ", right="s << std::setw(8) << rightSize << '\n';
                std::string s = ss.str();
                header = std::vector<uint8_t>(s.cbegin(), s.cend());
            }
            uint32_t headerSize = header.size();

            if ((headerSize + leftSize + rightSize) <= buffer.size()) {
                std::memcpy(&buffer[0], &header[0], headerSize);
                result += headerSize;

                std::memcpy(&buffer[result], &(left_.data[0]), leftSize);
                result += leftSize;

                std::memcpy(&buffer[result], &(right_.data[0]), rightSize);
                result += rightSize;

                left_.valid = false;
                right_.valid = false;
            }
        }

        return result;
    }

private:
    struct CameraMetadata {
        CameraMetadata(uint8_t sensorId, const std::vector<std::string> &filenames) : valid{false}, camera{sensorId, CameraParams{filenames}} {}

        Camera camera;

        bool valid;
        std::chrono::time_point<std::chrono::high_resolution_clock> ts;
        std::vector<uint8_t> data;
    };
    CameraMetadata left_, right_;
};
