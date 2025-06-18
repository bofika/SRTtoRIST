#include "feedback.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// Feedback destination (encoder)
#define FEEDBACK_IP "192.168.1.50"
#define FEEDBACK_PORT 5005

Feedback::Feedback(uint32_t min_bitrate, uint32_t max_bitrate)
    : m_min_bitrate(min_bitrate), m_max_bitrate(max_bitrate) {
    // Initialize socket
    init_socket();
}

Feedback::~Feedback() {
    // Close socket
    if (m_socket_fd >= 0) {
        close(m_socket_fd);
        m_socket_fd = -1;
    }
}

bool Feedback::init_socket() {
    // Create UDP socket
    m_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket_fd < 0) {
        std::cerr << "Failed to create feedback socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void Feedback::process_stats(uint32_t bitrate_avg, float packet_loss, uint32_t rtt) {
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Calculate suggested bitrate
    uint32_t bitrate_hint = calculate_bitrate_hint(bitrate_avg, packet_loss, rtt);
    
    // Clamp to min/max range
    bitrate_hint = clamp_bitrate(bitrate_hint);
    
    // Check if values have changed significantly to avoid spamming
    const float packet_loss_threshold = 0.1f;  // 0.1% change
    const uint32_t rtt_threshold = 5;          // 5ms change
    const uint32_t bitrate_threshold = 100;    // 100kbps change
    
    bool should_send = 
        (std::abs(m_last_packet_loss - packet_loss) >= packet_loss_threshold) ||
        (std::abs(static_cast<int>(m_last_rtt) - static_cast<int>(rtt)) >= static_cast<int>(rtt_threshold)) ||
        (std::abs(static_cast<int>(m_last_bitrate) - static_cast<int>(bitrate_hint)) >= static_cast<int>(bitrate_threshold));
    
    if (should_send) {
        if (send_feedback(bitrate_hint, packet_loss, rtt)) {
            // Update last sent values
            m_last_bitrate = bitrate_hint;
            m_last_packet_loss = packet_loss;
            m_last_rtt = rtt;
        }
    }
}

uint32_t Feedback::calculate_bitrate_hint(uint32_t current_bitrate, float packet_loss, uint32_t rtt) {
    // Simple algorithm to adjust bitrate based on packet loss and RTT
    // This can be improved with more sophisticated algorithms
    
    if (packet_loss > 5.0f) {
        // High packet loss, reduce bitrate significantly
        return current_bitrate * 0.7;
    } else if (packet_loss > 2.0f) {
        // Moderate packet loss, reduce bitrate slightly
        return current_bitrate * 0.9;
    } else if (packet_loss < 0.1f && rtt < 50) {
        // Low packet loss and RTT, can increase bitrate
        return current_bitrate * 1.1;
    }
    
    // Default case, maintain current bitrate
    return current_bitrate;
}

uint32_t Feedback::clamp_bitrate(uint32_t bitrate) {
    if (bitrate < m_min_bitrate) {
        return m_min_bitrate;
    } else if (bitrate > m_max_bitrate) {
        return m_max_bitrate;
    }
    return bitrate;
}

bool Feedback::send_feedback(uint32_t bitrate_hint, float packet_loss, uint32_t rtt) {
    if (m_socket_fd < 0) {
        ++m_failure_count;
        spdlog::error("Feedback socket not initialized");
        return false;
    }
    
    // Format feedback message
    std::stringstream ss;
    ss << "bitrate_hint: " << bitrate_hint << " kbps\n"
       << "packet_loss: " << std::fixed << std::setprecision(2) << packet_loss << "%\n"
       << "rtt_ms: " << rtt << "\n";
    
    std::string feedback_msg = ss.str();
    
    // Alternative HTTP GET format
    std::stringstream http_ss;
    http_ss << "GET /ctrl/stream_setting?index=stream1&width=1920&height=1080&bitrate=" 
            << (bitrate_hint * 1000) // Convert to bps
            << " HTTP/1.1\r\n"
            << "Host: " << FEEDBACK_IP << "\r\n"
            << "\r\n";
    
    std::string http_msg = http_ss.str();
    
    // Set up destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(FEEDBACK_PORT);
    inet_pton(AF_INET, FEEDBACK_IP, &dest_addr.sin_addr);
    
    // Send feedback message
    ssize_t sent = sendto(m_socket_fd, http_msg.c_str(), http_msg.size(), 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent < 0) {
        ++m_failure_count;
        spdlog::error("Failed to send feedback: {}", strerror(errno));
        return false;
    }

    // Reset failure counter on success
    m_failure_count = 0;

    std::cout << "Sent feedback to encoder - " << feedback_msg;
    return true;
}