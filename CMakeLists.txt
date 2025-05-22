cmake_minimum_required(VERSION 3.10)
project(srt_to_rist_gateway)

find_package(PkgConfig REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for dependencies
pkg_check_modules(SRT REQUIRED srt)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)

# Optional: librist
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBRIST QUIET librist)

    if(LIBRIST_FOUND)
        message(STATUS "librist found, enabling support.")
        include_directories(${LIBRIST_INCLUDE_DIRS})
        set(HAVE_LIBRIST TRUE)
    else()
        message(WARNING "librist not found; building without it.")
    endif()
endif()

# Header-only JSON library
include_directories(${CMAKE_SOURCE_DIR}/third_party)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/nlohmann)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp)
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp
        SHOW_PROGRESS
    )
endif()

# Include paths
include_directories(
    ${SRT_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/srt_input.cpp
    src/rtsp_input.cpp
    src/rist_output.cpp
    src/feedback.cpp
    src/network_utils.cpp
)

# Create executable
add_executable(srt_to_rist_gateway ${SOURCES})

# Link required libraries
target_link_libraries(srt_to_rist_gateway
    ${SRT_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    pthread
)

# Conditionally link librist if found
if(HAVE_LIBRIST)
    target_link_libraries(srt_to_rist_gateway ${LIBRIST_LIBRARIES})
endif()

# Install binary and config
install(TARGETS srt_to_rist_gateway DESTINATION bin)
install(FILES config.json DESTINATION etc/srt_to_rist_gateway)
