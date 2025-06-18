#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include "config.h"

Config parse_config(const std::string& config_path);

#endif // CONFIG_PARSER_H
