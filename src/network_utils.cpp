#include "network_utils.h"
#include <spdlog/spdlog.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

std::vector<std::string> NetworkUtils::get_interface_ips() {
    std::vector<std::string> ips;
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("getifaddrs failed: {}", strerror(errno));
        return ips;
    }
    
    // Walk through linked list
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        
        // Filter only IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            void* addr_ptr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            
            // Convert IP to string
            inet_ntop(AF_INET, addr_ptr, ip, INET_ADDRSTRLEN);
            
            // Skip loopback
            if (strcmp(ip, "127.0.0.1") != 0) {
                ips.push_back(std::string(ip));
                spdlog::info("Found interface {} with IP {}", ifa->ifa_name, ip);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return ips;
}

std::vector<std::string> NetworkUtils::get_wan_interface_ips() {
    std::vector<std::string> wan_ips;
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        spdlog::error("getifaddrs failed: {}", strerror(errno));
        return wan_ips;
    }
    
    // Walk through linked list
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        
        // Filter only IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            void* addr_ptr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            
            // Convert IP to string
            inet_ntop(AF_INET, addr_ptr, ip, INET_ADDRSTRLEN);
            
            // Skip loopback
            if (strcmp(ip, "127.0.0.1") != 0) {
                // Check if this is a WAN interface
                if (is_wan_interface(ifa->ifa_name)) {
                    wan_ips.push_back(std::string(ip));
                    spdlog::info("Found WAN interface {} with IP {}", ifa->ifa_name, ip);
                }
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    // If no WAN interfaces found, fallback to all non-loopback interfaces
    if (wan_ips.empty()) {
        spdlog::info("No WAN interfaces found, falling back to all interfaces");
        return get_interface_ips();
    }
    
    return wan_ips;
}

bool NetworkUtils::is_wan_interface(const std::string& interface_name) {
    // Skip loopback
    if (interface_name == "lo") {
        return false;
    }
    
    // Skip Docker and bridge interfaces (usually LAN)
    if (interface_name.substr(0, 4) == "docker" || 
        interface_name.substr(0, 2) == "br" ||
        interface_name.substr(0, 4) == "virbr") {
        return false;
    }
    
    // Common WAN interface names
    if (interface_name == "eth0" || 
        interface_name == "ppp0" || 
        interface_name == "wwan0" ||
        interface_name == "wan" ||
        interface_name.substr(0, 3) == "wan") {
        return true;
    }
    
    // For OpenWRT, try to use UCI to determine if WAN
    return check_uci_interface(interface_name);
}

bool NetworkUtils::check_uci_interface(const std::string& interface_name) {
    // On OpenWRT, we can use uci to check if an interface is WAN
    // Create child process for shell command
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        spdlog::error("pipe failed: {}", strerror(errno));
        return false;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        spdlog::error("fork failed: {}", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close read end
        
        // Redirect stdout to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Execute uci command
        execlp("uci", "uci", "show", "network.wan.ifname", nullptr);
        
        // If execlp returns, it failed
        spdlog::error("execlp failed: {}", strerror(errno));
        exit(1);
    } else {
        // Parent process
        close(pipefd[1]);  // Close write end
        
        // Read output
        char buffer[1024];
        std::string output;
        ssize_t count;
        
        while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            output.append(buffer, count);
        }
        
        close(pipefd[0]);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        
        // Check if interface name appears in output
        return output.find(interface_name) != std::string::npos;
    }
    
    return false;
}