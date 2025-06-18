#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

// Input modes
enum class InputMode {
    SRT,
    RTSP
};

// SRT specific modes
enum class SRTMode {
    CALLER,
    LISTENER,
    MULTI
};

// Multi-route configuration for each interface
struct MultiRouteConfig {
    std::string interface_ip;
    std::string rist_dst;
    int rist_port;
};

// Configuration structure
struct Config {
    // General settings
    InputMode mode;
    
    // SRT settings
    SRTMode srt_mode;
    std::string input_url;
    int listen_port;
    bool filter_to_wan = true;
    
    // RIST settings
    std::string rist_dst;
    int rist_port;
    
    // Bitrate settings
    int min_bitrate;
    int max_bitrate;
    
    // Multi-route settings
    std::vector<MultiRouteConfig> multi_routes;

    struct PathSwitch {
        bool enable = false;
        float max_packet_loss = 0.0f;
        uint32_t max_rtt_ms = 0;
    } path_switch;
};

#endif // CONFIG_H