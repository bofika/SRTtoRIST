#include "srt_input.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
        // Create epoll instance
        m_epoll_id = srt_epoll_create();
        if (m_epoll_id < 0) {
            report_srt_error("Failed to create SRT epoll");
            return false;
        }

        int events = SRT_EPOLL_IN;
        for (auto s : m_poll_sockets) {
            if (srt_epoll_add_usock(m_epoll_id, s, &events) < 0) {
                report_srt_error("Failed to add socket to epoll");
            }
        }

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
    std::string port_str;
    std::string url = m_srt_url;

    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        url = url.substr(pos + 3);
    }

    if (!url.empty() && url[0] == '[') {
        // IPv6 address in brackets
        size_t end = url.find(']');
        if (end != std::string::npos) {
            host = url.substr(1, end - 1);
            if (end + 1 < url.size() && url[end + 1] == ':') {
                port_str = url.substr(end + 2);
            }
        }
    }

    if (host.empty()) {
        // IPv4 or hostname
        pos = url.rfind(':');
        if (pos != std::string::npos && url.find(':') == pos) {
            host = url.substr(0, pos);
            port_str = url.substr(pos + 1);
        } else {
            host = url;
        }
    }

    if (port_str.empty()) {
        port_str = "1234"; // Default SRT port
    }

    // Set SRT options
    int latency = 200;  // ms
    srt_setsockopt(m_caller_socket, 0, SRTO_LATENCY, &latency, sizeof(latency));

    // Resolve host using getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* res = nullptr;
    int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(ret) << std::endl;
        srt_close(m_caller_socket);
        m_caller_socket = SRT_INVALID_SOCK;
        return false;
    }

    bool connected = false;
    for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        if (srt_connect(m_caller_socket, ai->ai_addr, ai->ai_addrlen) == 0) {
            connected = true;
            break;
        }
    }

    freeaddrinfo(res);

    if (!connected) {
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
    
    int ret = srt_epoll_wait(m_epoll_id, &readfds[0], &rlen, nullptr, nullptr, 10, nullptr, nullptr, nullptr, nullptr);
    
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
    
    SRTSOCKET client_sock = srt_accept(m_listen_socket,
                                       reinterpret_cast<sockaddr*>(&client_addr),
                                       &addrlen);
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

    int events = SRT_EPOLL_IN;
    if (m_epoll_id >= 0) {
        if (srt_epoll_add_usock(m_epoll_id, client_sock, &events) < 0) {
            report_srt_error("Failed to add client socket to epoll");
        }
    }
    
    // For multi mode, map to appropriate output
    if (m_mode == Mode::MULTI) {
        std::shared_ptr<RistOutput> output = nullptr;
        
        // Find the output for this IP
        auto it = m_ip_to_output.find(client_ip);
        if (it != m_ip_to_output.end()) {
            output = it->second;
        } else {
            // If no exact match, use the first output
            if (!m_outputs.empty()) {
                output = m_outputs[0];
            }
        }
        
        if (output) {
            m_socket_to_output[client_sock] = output;
        } else {
            std::cerr << "No output found for client IP " << client_ip << std::endl;
            srt_close(client_sock);
            // Remove from poll list
            auto poll_it = std::find(m_poll_sockets.begin(), m_poll_sockets.end(), client_sock);
            if (poll_it != m_poll_sockets.end()) {
                m_poll_sockets.erase(poll_it);
            }
            if (m_epoll_id >= 0) {
                srt_epoll_remove_usock(m_epoll_id, client_sock);
            }
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
            auto poll_it =
                std::find(m_poll_sockets.begin(), m_poll_sockets.end(), s);
            if (poll_it != m_poll_sockets.end()) {
                m_poll_sockets.erase(poll_it);
            }
            if (m_epoll_id >= 0) {
                srt_epoll_remove_usock(m_epoll_id, s);
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
    
    if (ret > 0 && output) {
        // Forward data to RIST output
        output->send_data(buffer.data(), ret);
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

    if (m_epoll_id >= 0) {
        srt_epoll_release(m_epoll_id);
        m_epoll_id = -1;
    }
    
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
