#include "srt_input.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// Buffer size for SRT data
#define SRT_BUFFER_SIZE 1456 * 100  // Should be multiple of SRT packet size

SRTInput::SRTInput(const std::string& srt_url, std::shared_ptr<RistOutput> output)
    : m_mode(Mode::CALLER), m_srt_url(srt_url), m_listen_port(0) {
    m_outputs.push_back(output);
}

SRTInput::SRTInput(int listen_port, std::shared_ptr<RistOutput> output)
    : m_mode(Mode::LISTENER), m_listen_port(listen_port) {
    m_outputs.push_back(output);
}

SRTInput::SRTInput(int listen_port)
    : m_mode(Mode::MULTI), m_listen_port(listen_port) {
}

SRTInput::~SRTInput() {
    stop();
}

void SRTInput::add_binding(const std::string& interface_ip, std::shared_ptr<RistOutput> output) {
    if (m_mode == Mode::MULTI) {
        m_ip_to_output[interface_ip] = output;
        m_outputs.push_back(output);
        if (!m_active_output) {
            m_active_output = output;
        }
    }
}

void SRTInput::set_active_output(std::shared_ptr<RistOutput> output) {
    if (m_mode != Mode::MULTI || !output) return;
    m_active_output = output;
    for (auto &pair : m_socket_to_output) {
        pair.second = output;
    }
}

bool SRTInput::init_srt() {
    if (m_initialized) {
        return true;
    }
    
    // Initialize SRT library
    if (srt_startup() < 0) {
        report_srt_error("SRT startup failed");
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool SRTInput::start() {
    if (!init_srt()) {
        return false;
    }
    
    // Setup based on mode
    bool success = false;
    switch (m_mode) {
        case Mode::CALLER:
            success = setup_caller();
            break;
        case Mode::LISTENER:
            success = setup_listener();
            break;
        case Mode::MULTI:
            success = setup_multi_listener();
            break;
    }
    
    if (success) {
        m_running = true;
        std::cout << "SRT input started successfully" << std::endl;
    } else {
        std::cerr << "Failed to start SRT input" << std::endl;
    }
    
    return success;
}

bool SRTInput::setup_caller() {
    // Create socket
    m_caller_socket = srt_create_socket();
    if (m_caller_socket == SRT_INVALID_SOCK) {
        report_srt_error("Failed to create SRT socket");
        return false;
    }
    
    // Parse URL
    std::string host;
    int port;
    size_t pos = m_srt_url.find("://");
    if (pos != std::string::npos) {
        host = m_srt_url.substr(pos + 3);
    } else {
        host = m_srt_url;
    }
    
    pos = host.find(":");
    if (pos != std::string::npos) {
        port = std::stoi(host.substr(pos + 1));
        host = host.substr(0, pos);
    } else {
        port = 1234;  // Default SRT port
    }
    
    // Set SRT options
    int latency = 200;  // ms
    srt_setsockopt(m_caller_socket, 0, SRTO_LATENCY, &latency, sizeof(latency));
    
    // Connect to remote SRT server
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sa.sin_addr);
    
    if (srt_connect(m_caller_socket, (sockaddr*)&sa, sizeof(sa)) < 0) {
        report_srt_error("Failed to connect to SRT server");
        srt_close(m_caller_socket);
        m_caller_socket = SRT_INVALID_SOCK;
        return false;
    }
    
    m_poll_sockets.push_back(m_caller_socket);
    return true;
}

bool SRTInput::setup_listener() {
    // Create socket
    m_listen_socket = srt_create_socket();
    if (m_listen_socket == SRT_INVALID_SOCK) {
        report_srt_error("Failed to create SRT socket");
        return false;
    }
    
    // Set SRT options
    int latency = 200;  // ms
    srt_setsockopt(m_listen_socket, 0, SRTO_LATENCY, &latency, sizeof(latency));
    
    int reuse = 1;
    srt_setsockopt(m_listen_socket, 0, SRTO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind to local address
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(m_listen_port);
    sa.sin_addr.s_addr = INADDR_ANY;
    
    if (srt_bind(m_listen_socket, (sockaddr*)&sa, sizeof(sa)) < 0) {
        report_srt_error("Failed to bind SRT socket");
        srt_close(m_listen_socket);
        m_listen_socket = SRT_INVALID_SOCK;
        return false;
    }
    
    // Start listening
    if (srt_listen(m_listen_socket, 5) < 0) {
        report_srt_error("Failed to listen on SRT socket");
        srt_close(m_listen_socket);
        m_listen_socket = SRT_INVALID_SOCK;
        return false;
    }
    
    m_poll_sockets.push_back(m_listen_socket);
    return true;
}

bool SRTInput::setup_multi_listener() {
    // Make sure we have interface bindings
    if (m_ip_to_output.empty()) {
        std::cerr << "No interface bindings specified for multi-interface mode" << std::endl;
        return false;
    }
    
    // Create listener socket
    return setup_listener();
}

void SRTInput::process() {
    if (!m_running) {
        return;
    }
    
    // Poll for events
    std::vector<SRTSOCKET> readfds = m_poll_sockets;
    int rlen = readfds.size();
    
    int ret = srt_epoll_wait(SRT_INVALID_SOCK, &readfds[0], &rlen, nullptr, nullptr, 10, nullptr, nullptr, nullptr, nullptr);
    
    if (ret < 0) {
        if (srt_getlasterror(nullptr) == SRT_ETIMEOUT) {
            // Timeout is normal
            return;
        }
        report_srt_error("SRT poll error");
        return;
    }
    
    // Process ready sockets
    for (int i = 0; i < rlen; i++) {
        SRTSOCKET s = readfds[i];
        
        if (s == m_listen_socket) {
            // Handle new connections
            handle_connections();
        } else {
            // Handle data from existing connection
            if (m_mode == Mode::MULTI) {
                auto it = m_socket_to_output.find(s);
                if (it != m_socket_to_output.end()) {
                    process_socket(s, it->second);
                }
            } else {
                process_socket(s, m_outputs[0]);
            }
        }
    }
}

void SRTInput::handle_connections() {
    sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);
    
    SRTSOCKET client_sock = srt_accept(m_listen_socket, (sockaddr*)&client_addr, &addrlen);
    if (client_sock == SRT_INVALID_SOCK) {
        report_srt_error("SRT accept failed");
        return;
    }
    
    // Convert client address to string
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);
    std::string client_ip(ipstr);
    
    std::cout << "New SRT connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
    
    // Add to poll list
    m_poll_sockets.push_back(client_sock);
    
    if (m_mode == Mode::MULTI) {
        if (m_active_output) {
            m_socket_to_output[client_sock] = m_active_output;
        }
    }
}

void SRTInput::process_socket(SRTSOCKET s, std::shared_ptr<RistOutput> output) {
    std::vector<char> buffer(SRT_BUFFER_SIZE);
    
    // Receive data
    int ret = srt_recvmsg(s, buffer.data(), buffer.size());
    if (ret < 0) {
        if (srt_getlasterror(nullptr) == SRT_ECONNLOST) {
            std::cout << "SRT connection lost" << std::endl;
            
            // Remove from poll list
            auto it = std::find(m_poll_sockets.begin(), m_poll_sockets.end(), s);
            if (it != m_poll_sockets.end()) {
                m_poll_sockets.erase(it);
            }
            
            // Remove from mapping
            if (m_mode == Mode::MULTI) {
                m_socket_to_output.erase(s);
            }
            
            srt_close(s);
        } else {
            report_srt_error("SRT receive error");
        }
        return;
    }
    
    if (ret > 0) {
        std::shared_ptr<RistOutput> out = output;
        if (m_mode == Mode::MULTI && m_active_output) {
            out = m_active_output;
            m_socket_to_output[s] = m_active_output;
        }
        if (out) {
            out->send_data(buffer.data(), ret);
        }
    }
}

void SRTInput::stop() {
    m_running = false;
    
    // Close all sockets
    for (auto s : m_poll_sockets) {
        srt_close(s);
    }
    m_poll_sockets.clear();
    m_socket_to_output.clear();
    
    m_caller_socket = SRT_INVALID_SOCK;
    m_listen_socket = SRT_INVALID_SOCK;
    
    if (m_initialized) {
        srt_cleanup();
        m_initialized = false;
    }
}

void SRTInput::report_srt_error(const std::string& context) {
    int errcode = srt_getlasterror(nullptr);
    std::cerr << context << ": " << srt_getlasterror_str() << " (" << errcode << ")" << std::endl;
}