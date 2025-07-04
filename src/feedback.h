#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <cstdint>
#include <string>
#include <mutex>

class Feedback {
public:
    Feedback(uint32_t min_bitrate, uint32_t max_bitrate,
             const std::string& ip, int port);
    ~Feedback();
    
    // Process network stats and send feedback
    void process_stats(uint32_t bitrate_avg, float packet_loss, uint32_t rtt);

    // Number of consecutive failures when sending feedback
    size_t get_failure_count() const { return m_failure_count; }
    
private:
    // Initialize UDP socket for feedback
    bool init_socket();
    
    // Send feedback to encoder
    bool send_feedback(uint32_t bitrate_hint, float packet_loss, uint32_t rtt);
    
    // Calculate suggested bitrate based on network conditions
    uint32_t calculate_bitrate_hint(uint32_t current_bitrate, float packet_loss, uint32_t rtt);
    
    // Clamp bitrate within min/max range
    uint32_t clamp_bitrate(uint32_t bitrate);
    
    uint32_t m_min_bitrate;
    uint32_t m_max_bitrate;
    std::string m_ip;
    int m_port;
    int m_socket_fd = -1;
    std::mutex m_mutex;
    
    // Last sent values (to avoid duplicate messages)
    uint32_t m_last_bitrate = 0;
    float m_last_packet_loss = 0.0f;
    uint32_t m_last_rtt = 0;

    // Track consecutive send failures
    size_t m_failure_count = 0;
};

#endif // FEEDBACK_H
