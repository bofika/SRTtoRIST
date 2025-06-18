#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <string>
#include <vector>

class NetworkUtils {
public:
    // Get list of all interface IPs
    static std::vector<std::string> get_interface_ips();
    
    // Get list of WAN interface IPs
    static std::vector<std::string> get_wan_interface_ips();
    
    // Check if interface is a WAN interface
    static bool is_wan_interface(const std::string& interface_name);
    
private:
    // Check interface properties using ubus/uci (OpenWRT specific)
    static bool check_uci_interface(const std::string& interface_name);
};

#endif // NETWORK_UTILS_H
