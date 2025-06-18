#include "rist_output.h"
#include "feedback.h"
#include <iostream>
#include <thread>
#include <chrono>

RistOutput::RistOutput(const std::string& dst_ip, int dst_port)
    : m_dst_ip(dst_ip), m_dst_port(dst_port) {
}

RistOutput::~RistOutput() {
    // Stop event thread
    m_running = false;
    if (m_event_thread.joinable()) {
        m_event_thread.join();
    }
    
    // Clean up RIST resources
    if (m_ctx) {
        if (m_peer) {
            rist_peer_destroy(m_peer);
            m_peer = nullptr;
        }
        
        rist_destroy(m_ctx);
        m_ctx = nullptr;
    }
}

bool RistOutput::init() {
    // Initialize RIST library
    int ret;
    
    // Create RIST sender context
    struct rist_ctx_options options = {0};
    ret = rist_sender_create(&m_ctx, RIST_PROFILE_MAIN, &options);
    if (ret != 0) {
        std::cerr << "Failed to create RIST sender context: " << ret << std::endl;
        return false;
    }
    
    // Set stats callback
    struct rist_stats_callback_object stats_cb = {0};
    stats_cb.callback = &RistOutput::stats_callback;
    stats_cb.arg = this;
    rist_stats_callback_set(m_ctx, &stats_cb, 1000); // 1 second interval
    
    // Setup peer connection
    struct rist_peer_config peer_config = {0};
    char url[512];
    snprintf(url, sizeof(url), "rist://%s:%d", m_dst_ip.c_str(), m_dst_port);
    peer_config.address = url;
    
    ret = rist_peer_create(m_ctx, &m_peer, &peer_config);
    if (ret != 0 || !m_peer) {
        std::cerr << "Failed to create RIST peer: " << ret << std::endl;
        rist_destroy(m_ctx);
        m_ctx = nullptr;
        return false;
    }
    
    // Start RIST event loop
    m_running = true;
    m_event_thread = std::thread(&RistOutput::rist_event_loop, this);
    
    std::cout << "RIST output initialized to " << url << std::endl;
    return true;
}

void RistOutput::rist_event_loop() {
    while (m_running) {
        int ret = rist_auth_handler(m_ctx);
        if (ret != 0) {
            std::cerr << "RIST auth handler error: " << ret << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool RistOutput::send_data(const char* data, size_t size) {
    if (!m_ctx || !m_peer) {
        return false;
    }
    
    // Protect with mutex for thread safety
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Use first stream ID
    uint16_t stream_id = 0;
    
    // Send data over RIST
    int ret = rist_sender_data_write(m_ctx, data, size, stream_id);
    if (ret < 0) {
        std::cerr << "Failed to send data over RIST: " << ret << std::endl;
        return false;
    }
    
    return true;
}

void RistOutput::set_feedback_callback(std::shared_ptr<Feedback> feedback) {
    m_feedback = feedback;
}

int RistOutput::stats_callback(void* arg, const struct rist_stats *stats) {
    RistOutput* output = static_cast<RistOutput*>(arg);
    if (!output || !output->m_feedback) {
        return 0;
    }
    
    // Process stats based on type
    switch (stats->stats_type) {
        case RIST_STATS_SENDER_PEER: {
            // Extract relevant stats
            uint32_t bitrate_avg = stats->stats.sender_peer.bitrate_avg;
            float packet_loss = stats->stats.sender_peer.quality;
            uint32_t rtt = stats->stats.sender_peer.rtt;
            
            // Report to feedback
            output->m_feedback->process_stats(bitrate_avg, packet_loss, rtt);
            break;
        }
        default:
            // Other stats types not used
            break;
    }
    
    return 0;
}
