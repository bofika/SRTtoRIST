cmake_minimum_required(VERSION 3.10)
project(srt_to_rist_gateway)

find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pkg_check_modules(SRT REQUIRED srt)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(RIST REQUIRED librist)

include_directories(
    ${SRT_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${RIST_INCLUDE_DIRS}
)

include_directories(${CMAKE_SOURCE_DIR}/third_party)

set(SOURCES
    src/main.cpp
    src/config_parser.cpp
    src/srt_input.cpp
    src/rtsp_input.cpp
    src/feedback.cpp
    src/network_utils.cpp
    src/rist_output.cpp
)

add_executable(srt_to_rist_gateway ${SOURCES})

target_link_libraries(srt_to_rist_gateway
    ${SRT_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${RIST_LIBRARIES}
    pthread
    spdlog::spdlog
)

install(TARGETS srt_to_rist_gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/srt_to_rist_gateway)
