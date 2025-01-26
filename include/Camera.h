#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <vector>
#include "opencv2/calib3d.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/core/types.hpp"
#include "opencv2/imgcodecs.hpp"

using namespace std::string_literals;

class CameraParams {
public:
    CameraParams(std::vector<std::string> calibrationImages) {
        std::vector<cv::Mat> imgPoints;
        std::vector<cv::Mat> objPoints;
        for (auto imgPath : calibrationImages) {
            cv::Mat img = cv::imread(imgPath);
            size = img.size();
            cv::Mat gray, corners;
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            if (cv::findChessboardCorners(gray, cv::Size(10, 7), corners)) {
                cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                                 cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 30, 0.001));
                cv::Mat objp = cv::Mat::zeros(corners.size(), corners.type());

                objPoints.push_back(objp);
                imgPoints.push_back(corners);
            }
        }
        try {
            reprojection_error = cv::calibrateCamera(objPoints, imgPoints, size, cameraMatrix, distCoeffs, rvecs, tvecs);
            optimalCameraMatrix = cv::getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, size, 0.0, cv::Size(), &validPixRoi);
        } catch (cv::Exception e) {
            working = false;
            std::cout << "creation of camera matrix unsuccessful, posibly because of zero calibration files\n";
        }
    }

    cv::Mat optimalCameraMatrix;
    cv::Mat cameraMatrix, distCoeffs;
    cv::Mat rvecs, tvecs;
    cv::Rect validPixRoi;
    cv::Size size;
    double reprojection_error;
    bool working = true;
};

class Camera {
public:
    Camera(uint8_t sensorId, CameraParams params)
        : pipeline_(pipeline(sensorId)), vc_{pipeline_, cv::CAP_GSTREAMER}, cameraParams{params}, run_{true}, id_{sensorId} {
        if (!vc_.isOpened()) {
            throw std::runtime_error("Camera("s + std::to_string(sensorId) + ") failed, VideoCapture not opened, pipeline=\""s + pipeline_ +
                                     "\""s);
        }

        // without C++17's std::filesystem, this is the easiest way
        system(("mkdir -p calibration/" + std::to_string(id_)).c_str());
        system(("rm -rf calibration/" + std::to_string(id_) + "/*").c_str());

        std::cout << pipeline_ << std::endl;

        thread_ = std::thread(&Camera::run, this);
    }

    virtual ~Camera() {
        run_ = false;
        thread_.join();
    }

    bool read(std::chrono::time_point<std::chrono::high_resolution_clock> &ts, std::vector<uint8_t> &buffer) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (0u != result_.second.size()) {
            ts = result_.first;
            result_.second.swap(buffer);
            result_.second.resize(0u);

            return true;
        }

        return false;
    }

    void savePhoto() { saveImg_ = true; }

    void recalibrate() {
        std::vector<std::string> paths;
        for (size_t id = 0; id < nextPhotoId_; id++) {
            paths.push_back("calibration/" + std::to_string(id_) + "/" + std::to_string(id) + ".png");
        }
        cameraParams = CameraParams(paths);
    }

    void resetPhotos() { nextPhotoId_ = 0; }

private:
    static std::string pipeline(uint8_t sensorId, uint32_t captureWidth = 1280u, uint32_t captureHeight = 720u,
                                uint32_t displayWidth = 1280u, uint32_t displayHeight = 720u, uint32_t frameRate = 2u,
                                uint32_t flipMethod = 0u);
    void run() {
        std::chrono::time_point<std::chrono::high_resolution_clock> ts;
        cv::Mat mat0, mat1;
        std::vector<uint8_t> buffer;

        while (run_) {
            if (vc_.grab()) {
                ts = std::chrono::high_resolution_clock::now();
                switch (getUndistortedImageAsPNG(buffer)) {
                    case ReturnCode::OK: {
                        std::lock_guard<std::mutex> lock(mutex_);
                        result_.first = ts;
                        result_.second.swap(buffer);
                        break;
                    }
                    case ReturnCode::RETRIEVE_ERR: {
                        std::cerr << "Could not retrieve!"s << std::endl;
                        break;
                    }
                    case ReturnCode::ENCODE_ERR: {
                        std::cerr << "Could not encode!"s << std::endl;
                        break;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    enum class ReturnCode { OK = 0, RETRIEVE_ERR = 1, ENCODE_ERR = 2 };

    void saveImage(const std::vector<uint8_t> &buffer) {
        std::ofstream outfile("calibration/" + std::to_string(id_) + "/" + std::to_string(nextPhotoId_++) + ".png");
        outfile.write((const char *)buffer.data(), buffer.size());
        outfile.flush();
        outfile.close();
        saveImg_ = false;
    }

    ReturnCode getUndistortedImageAsPNG(std::vector<uint8_t> &buffer) {
        cv::Mat mat;
        if (!vc_.retrieve(mat)) {
            return ReturnCode::RETRIEVE_ERR;
        }
        if (cameraParams.working) {
        cv::undistort(mat, mat, cameraParams.cameraMatrix, cameraParams.distCoeffs);
        }
        if (saveImg_) {
            if (!cv::imencode(".png"s, mat, buffer)) {
                return ReturnCode::ENCODE_ERR;
            }
            saveImage(buffer);
        }
        cv::resize(mat, mat, cv::Size(), 0.5f, 0.5, cv::INTER_AREA);
        if (!cv::imencode(".png"s, mat, buffer)) {
            return ReturnCode::ENCODE_ERR;
        }
        return ReturnCode::OK;
    }

    const std::string pipeline_;
    cv::VideoCapture vc_;
    CameraParams cameraParams;

    uint8_t id_;
    intmax_t nextPhotoId_ = 0;
    bool saveImg_ = false;

    bool run_;
    std::thread thread_;

    std::mutex mutex_;
    std::pair<std::chrono::time_point<std::chrono::high_resolution_clock>, std::vector<uint8_t>> result_;
};
