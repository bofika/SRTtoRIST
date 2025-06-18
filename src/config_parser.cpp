#include "config_parser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>

using json = nlohmann::json;

Config parse_config(const std::string& config_path) {
    Config config;

    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    try {
        json j;
        config_file >> j;

        auto require = [&](const json &obj, const std::string &key) -> const json & {
            if (!obj.contains(key)) {
                throw std::runtime_error("Missing required key '" + key + "'");
            }
            return obj.at(key);
        };

        // Parse mode
        std::string mode = require(j, "mode").get<std::string>();
        if (mode == "srt") {
            config.mode = InputMode::SRT;

            // Parse SRT mode
            std::string srt_mode = require(j, "srt_mode").get<std::string>();
            if (srt_mode == "caller") {
                config.srt_mode = SRTMode::CALLER;
                config.input_url = require(j, "input_url").get<std::string>();
            } else if (srt_mode == "listener") {
                config.srt_mode = SRTMode::LISTENER;
                config.listen_port = require(j, "listen_port").get<int>();
            } else if (srt_mode == "multi") {
                config.srt_mode = SRTMode::MULTI;
                config.listen_port = require(j, "listen_port").get<int>();
                config.filter_to_wan = j.value("filter_to_wan", true);

                // Parse multi-route configuration
                auto routes = require(j, "multi_route");
                for (const auto& route : routes) {
                    MultiRouteConfig mrc;
                    mrc.interface_ip = require(route, "interface_ip").get<std::string>();
                    mrc.rist_dst = require(route, "rist_dst").get<std::string>();
                    mrc.rist_port = require(route, "rist_port").get<int>();
                    config.multi_routes.push_back(mrc);
                }
            } else {
                throw std::runtime_error("Invalid SRT mode: " + srt_mode);
            }
        } else if (mode == "rtsp") {
            config.mode = InputMode::RTSP;
            config.input_url = require(j, "input_url").get<std::string>();
        } else {
            throw std::runtime_error("Invalid mode: " + mode);
        }

        // Parse common parameters
        if (config.mode != InputMode::SRT || config.srt_mode != SRTMode::MULTI) {
            config.rist_dst = require(j, "rist_dst").get<std::string>();
            config.rist_port = require(j, "rist_port").get<int>();
        }

        config.min_bitrate = require(j, "min_bitrate").get<int>();
        config.max_bitrate = require(j, "max_bitrate").get<int>();

        // Parse feedback settings
        config.feedback_ip = j.value("feedback_ip", config.feedback_ip);
        config.feedback_port = j.value("feedback_port", config.feedback_port);

    } catch (json::exception& e) {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    }

    return config;
}

