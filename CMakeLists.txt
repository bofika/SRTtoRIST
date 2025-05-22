cmake_minimum_required(VERSION 3.10)
project(srt_to_rist_gateway)

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional dependency
pkg_check_modules(LIBRIST librist)
pkg_check_modules(SRT REQUIRED srt)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)

# Header-only JSON
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Fetch json.hpp if missing
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/nlohmann)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp)
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp
        SHOW_PROGRESS
    )
endif()

# Sources
set(SOURCES
    src/main.cpp
    src/srt_input.cpp
    src/rtsp_input.cpp
    src/rist_output.cpp
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
    pthread
)

if(LIBRIST_FOUND)
    message(STATUS "librist found, linking with it.")
    include_directories(${LIBRIST_INCLUDE_DIRS})
    target_link_libraries(srt_to_rist_gateway ${LIBRIST_LIBRARIES})
else()
    message(WARNING "librist not found, continuing without RIST support.")
endif()

# Install
install(TARGETS srt_to_rist_gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/srt_to_rist_gateway)
