#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <signal.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "config.h"
#include "srt_input.h"
#include "rtsp_input.h"
#include "rist_output.h"
#include "feedback.h"
#include "network_utils.h"
#include "config_parser.h"

using json = nlohmann::json;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int signal) {
    std::cout << "Caught signal " << signal << ", shutting down..." << std::endl;
    running = 0;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>" << std::endl;
        return 1;
    }
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // Parse config
        Config config = parse_config(argv[1]);
        
        // Setup feedback handler
        auto feedback = std::make_shared<Feedback>(
            config.min_bitrate, config.max_bitrate,
            config.feedback_ip, config.feedback_port);
        
        // Setup input and output based on config
        std::unique_ptr<InputBase> input;
        std::vector<std::shared_ptr<RistOutput>> outputs;
        
        if (config.mode == InputMode::SRT) {
            if (config.srt_mode == SRTMode::MULTI) {
                // Get available WAN interfaces if using "auto"
                if (config.filter_to_wan) {
                    auto wan_ips = NetworkUtils::get_wan_interface_ips();
                    if (wan_ips.empty()) {
                        throw std::runtime_error("No WAN interfaces found");
                    }
                    
                    // Assign IPs to multi-route config
                    size_t ip_index = 0;
                    for (auto& route : config.multi_routes) {
                        if (route.interface_ip == "auto") {
                            if (ip_index < wan_ips.size()) {
                                route.interface_ip = wan_ips[ip_index++];
                                std::cout << "Assigned WAN IP " << route.interface_ip 
                                          << " to route " << ip_index << std::endl;
                            } else {
                                throw std::runtime_error("Not enough WAN interfaces for configured routes");
                            }
                        }
                    }
                }
                
                // Create multiple RIST outputs
                for (const auto& route : config.multi_routes) {
                    auto rist = std::make_shared<RistOutput>(route.rist_dst, route.rist_port);
                    rist->set_feedback_callback(feedback);
                    if (!rist->init()) {
                        throw std::runtime_error("Failed to initialize RIST output");
                    }
                    outputs.push_back(rist);
                }
                
                // Create multi-interface SRT input
                auto srt_input = std::make_unique<SRTInput>(config.listen_port);
                for (size_t i = 0; i < config.multi_routes.size(); i++) {
                    srt_input->add_binding(config.multi_routes[i].interface_ip, outputs[i]);
                }
                input = std::move(srt_input);
                
            } else if (config.srt_mode == SRTMode::CALLER) {
                // Create SRT caller input
                auto rist = std::make_shared<RistOutput>(config.rist_dst, config.rist_port);
                rist->set_feedback_callback(feedback);
                if (!rist->init()) {
                    throw std::runtime_error("Failed to initialize RIST output");
                }
                outputs.push_back(rist);
                input = std::make_unique<SRTInput>(config.input_url, outputs[0]);
                
            } else if (config.srt_mode == SRTMode::LISTENER) {
                // Create SRT listener input
                auto rist = std::make_shared<RistOutput>(config.rist_dst, config.rist_port);
                rist->set_feedback_callback(feedback);
                if (!rist->init()) {
                    throw std::runtime_error("Failed to initialize RIST output");
                }
                outputs.push_back(rist);
                input = std::make_unique<SRTInput>(config.listen_port, outputs[0]);
            }
        } else if (config.mode == InputMode::RTSP) {
            // Create RTSP input
            auto rist = std::make_shared<RistOutput>(config.rist_dst, config.rist_port);
            rist->set_feedback_callback(feedback);
            if (!rist->init()) {
                throw std::runtime_error("Failed to initialize RIST output");
            }
            outputs.push_back(rist);
            input = std::make_unique<RTSPInput>(config.input_url, outputs[0]);
        }
        
        if (!input || outputs.empty()) {
            throw std::runtime_error("Failed to initialize input or output");
        }
        
        std::cout << "Stream relay initialized successfully" << std::endl;
        
        // Start the stream relay
        input->start();
        
        // Main loop
        while (running) {
            input->process();
            
            // Sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Stop and cleanup
        input->stop();
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Stream relay terminated" << std::endl;
    return 0;
}
