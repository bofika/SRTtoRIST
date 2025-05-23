#ifndef RIST_OUTPUT_H
#define RIST_OUTPUT_H

#include <string>
#include <memory>
#include <librist/librist.h>
#include <thread>
#include <atomic>
#include <mutex>

class Feedback;

class RistOutput {
public:
    RistOutput(const std::string& dst_ip, int dst_port);
    ~RistOutput();
    
    // Initialize RIST output
    bool init();
    
    // Send data to RIST destination
    bool send_data(const char* data, size_t size);
    
    // Set feedback callback
    void set_feedback_callback(std::shared_ptr<Feedback> feedback);
    
private:
    // RIST stats callback
    static int stats_callback(void* arg, const struct rist_stats *stats);
    
    // Thread function to run RIST event loop
    void rist_event_loop();
    
    std::string m_dst_ip;
    int m_dst_port;
    
    struct rist_ctx *m_ctx = nullptr;
    struct rist_peer *m_peer = nullptr;
    
    std::shared_ptr<Feedback> m_feedback;
    std::thread m_event_thread;
    std::atomic<bool> m_running{false};
    
    // Mutex for thread safety
    std::mutex m_mutex;
};

#endif // RIST_OUTPUT_H