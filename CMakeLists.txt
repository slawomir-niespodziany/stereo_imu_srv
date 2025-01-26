cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ERR_FLAGS -Wall -Wextra -Wpedantic)

project(stereo_imu_srv VERSION 0.1 DESCRIPTION "Server for stereo_imu.")

find_package( OpenCV REQUIRED )
find_package( Threads REQUIRED )

include_directories( ${OpenCV_INCLUDE_DIRS} )

add_executable(srv "src/main.cpp" "src/TcpServer.cpp" "src/ImuDataSource.cpp" "src/StereoDataSource.cpp" "src/Camera.cpp" "src/StereoCamera.cpp")

target_compile_options(srv PRIVATE ${ERR_FLAGS})

target_include_directories(srv PUBLIC "include" ${OpenCV_INCLUDE_DIRS})
target_link_libraries(srv ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})
