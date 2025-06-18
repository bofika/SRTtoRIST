#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <thread>

#include "config.h"
#include "srt_input.h"
#include "rtsp_input.h"
#include "rist_output.h"
#include "feedback.h"
#include "network_utils.h"

using json = nlohmann::json;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int signal) {
    spdlog::info("Caught signal {}, shutting down...", signal);
    running = 0;
}

// Parse config file and return Config object
Config parse_config(const std::string& config_path) {
    Config config;
    
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }
    
    try {
        json j;
        config_file >> j;
        
        // Parse mode
        std::string mode = j.at("mode");
        if (mode == "srt") {
            config.mode = InputMode::SRT;
            
            // Parse SRT mode
            std::string srt_mode = j.at("srt_mode");
            if (srt_mode == "caller") {
                config.srt_mode = SRTMode::CALLER;
                config.input_url = j.at("input_url");
            } else if (srt_mode == "listener") {
                config.srt_mode = SRTMode::LISTENER;
                config.listen_port = j.at("listen_port");
            } else if (srt_mode == "multi") {
                config.srt_mode = SRTMode::MULTI;
                config.listen_port = j.at("listen_port");
                config.filter_to_wan = j.value("filter_to_wan", true);
                
                // Parse multi-route configuration
                auto routes = j.at("multi_route");
                for (const auto& route : routes) {
                    MultiRouteConfig mrc;
                    mrc.interface_ip = route.at("interface_ip");
                    mrc.rist_dst = route.at("rist_dst");
                    mrc.rist_port = route.at("rist_port");
                    config.multi_routes.push_back(mrc);
                }
            } else {
                throw std::runtime_error("Invalid SRT mode: " + srt_mode);
            }
        } else if (mode == "rtsp") {
            config.mode = InputMode::RTSP;
            config.input_url = j.at("input_url");
        } else {
            throw std::runtime_error("Invalid mode: " + mode);
        }
        
        // Parse common parameters
        if (config.srt_mode != SRTMode::MULTI) {
            config.rist_dst = j.at("rist_dst");
            config.rist_port = j.at("rist_port");
        }
        
        config.min_bitrate = j.at("min_bitrate");
        config.max_bitrate = j.at("max_bitrate");

        if (j.contains("path_switch")) {
            auto ps = j["path_switch"];
            config.path_switch.enable = ps.value("enable", false);
            config.path_switch.max_packet_loss = ps.value("max_packet_loss", 0.0);
            config.path_switch.max_rtt_ms = ps.value("max_rtt_ms", 0u);
        }
        
    } catch (json::exception& e) {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        spdlog::error("Usage: {} <config.json>", argv[0]);
        return 1;
    }

    auto logger = spdlog::rotating_logger_mt("srttorist", "/var/log/srttorist.log", 1024 * 1024, 3);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S %^%l%$ %v");
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // Parse config
        Config config = parse_config(argv[1]);
        
        // Setup feedback handler
        auto feedback = std::make_shared<Feedback>(config.min_bitrate, config.max_bitrate);
        
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
                                spdlog::info("Assigned WAN IP {} to route {}", route.interface_ip, ip_index);
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
                    rist->init();
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
                rist->init();
                outputs.push_back(rist);
                input = std::make_unique<SRTInput>(config.input_url, outputs[0]);
                
            } else if (config.srt_mode == SRTMode::LISTENER) {
                // Create SRT listener input
                auto rist = std::make_shared<RistOutput>(config.rist_dst, config.rist_port);
                rist->set_feedback_callback(feedback);
                rist->init();
                outputs.push_back(rist);
                input = std::make_unique<SRTInput>(config.listen_port, outputs[0]);
            }
        } else if (config.mode == InputMode::RTSP) {
            // Create RTSP input
            auto rist = std::make_shared<RistOutput>(config.rist_dst, config.rist_port);
            rist->set_feedback_callback(feedback);
            rist->init();
            outputs.push_back(rist);
            input = std::make_unique<RTSPInput>(config.input_url, outputs[0]);
        }
        
        if (!input || outputs.empty()) {
            throw std::runtime_error("Failed to initialize input or output");
        }
        
        spdlog::info("Stream relay initialized successfully");
        
        // Start the stream relay
        input->start();

        size_t active_index = 0;
        if (config.srt_mode == SRTMode::MULTI && !outputs.empty()) {
            dynamic_cast<SRTInput*>(input.get())->set_active_output(outputs[0]);
        }

        auto last_check = std::chrono::steady_clock::now();

        // Main loop
        while (running) {
            input->process();

            if (config.srt_mode == SRTMode::MULTI && config.path_switch.enable) {
                auto now = std::chrono::steady_clock::now();
                if (now - last_check >= std::chrono::seconds(1)) {
                    last_check = now;
                    size_t best = active_index;
                    float best_loss = outputs[active_index]->get_stats().packet_loss;
                    uint32_t best_rtt = outputs[active_index]->get_stats().rtt;
                    for (size_t i = 0; i < outputs.size(); ++i) {
                        auto st = outputs[i]->get_stats();
                        if (st.packet_loss <= config.path_switch.max_packet_loss && st.rtt <= config.path_switch.max_rtt_ms) {
                            best = i;
                            break;
                        }
                        if (st.packet_loss < best_loss) {
                            best = i;
                            best_loss = st.packet_loss;
                            best_rtt = st.rtt;
                        }
                    }
                    if (best != active_index) {
                        active_index = best;
                        dynamic_cast<SRTInput*>(input.get())->set_active_output(outputs[active_index]);
                        spdlog::info("Switched RIST peer to index {}", active_index);
                    }
                }
            }

            // Sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Stop and cleanup
        input->stop();
        
    } catch (std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    spdlog::info("Stream relay terminated");
    return 0;
}