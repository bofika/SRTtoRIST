#ifndef SRT_INPUT_H
#define SRT_INPUT_H

#include <string>
#include <map>
#include <srt/srt.h>
#include "input_base.h"

class SRTInput : public InputBase {
public:
    // Constructor for caller mode
    SRTInput(const std::string& srt_url, std::shared_ptr<RistOutput> output);
    
    // Constructor for listener mode
    SRTInput(int listen_port, std::shared_ptr<RistOutput> output);
    
    // Constructor for multi-interface mode
    SRTInput(int listen_port);
    
    // Add a binding for multi-interface mode
    void add_binding(const std::string& interface_ip, std::shared_ptr<RistOutput> output);

    // Set active RIST output (multi mode)
    void set_active_output(std::shared_ptr<RistOutput> output);
    
    // Virtual functions from InputBase
    bool start() override;
    void process() override;
    void stop() override;
    
    ~SRTInput();
    
private:
    enum class Mode {
        CALLER,
        LISTENER,
        MULTI
    };
    
    struct ConnectionInfo {
        SRTSOCKET socket;
        std::shared_ptr<RistOutput> output;
    };
    
    // Initialize SRT library
    bool init_srt();
    
    // Setup caller connection
    bool setup_caller();
    
    // Setup listener connection
    bool setup_listener();
    
    // Setup multi-interface listener
    bool setup_multi_listener();
    
    // Process data from a specific socket
    void process_socket(SRTSOCKET s, std::shared_ptr<RistOutput> output);
    
    // Handle new connections
    void handle_connections();
    
    // SRT error reporting
    void report_srt_error(const std::string& context);
    
    Mode m_mode;
    std::string m_srt_url;
    int m_listen_port;
    
    // SRT socket management
    SRTSOCKET m_caller_socket = SRT_INVALID_SOCK;
    SRTSOCKET m_listen_socket = SRT_INVALID_SOCK;
    
    // Multi-interface mode mappings
    std::map<std::string, std::shared_ptr<RistOutput>> m_ip_to_output;
    std::map<SRTSOCKET, std::shared_ptr<RistOutput>> m_socket_to_output;
    std::shared_ptr<RistOutput> m_active_output;
    
    bool m_initialized = false;
    bool m_running = false;
    
    // Polling structures
    std::vector<SRTSOCKET> m_poll_sockets;
};

#endif // SRT_INPUT_H