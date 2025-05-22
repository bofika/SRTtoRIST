cmake_minimum_required(VERSION 3.10)
project(srt_to_rist_gateway)

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required dependencies only
pkg_check_modules(SRT REQUIRED srt)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(RIST REQUIRED librist)

# Include paths
include_directories(
    ${SRT_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${RIST_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${SRT_LIBRARY_DIRS}
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVCODEC_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${RIST_LIBRARY_DIRS}
)

# Header-only nlohmann/json
include_directories(${CMAKE_SOURCE_DIR}/third_party)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/nlohmann)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp)
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp
        SHOW_PROGRESS
    )
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/srt_input.cpp
    src/rtsp_input.cpp
    src/feedback.cpp
    src/network_utils.cpp
)

add_executable(srt_to_rist_gateway ${SOURCES})

# Link libraries
target_link_libraries(srt_to_rist_gateway
    ${SRT_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${RIST_LIBRARIES}
    pthread
)

# Install binary and config
install(TARGETS srt_to_rist_gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/srt_to_rist_gateway)
