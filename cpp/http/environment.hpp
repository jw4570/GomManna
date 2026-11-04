#pragma once

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace gommanna::http_config {
using Values = std::map<std::string, std::string>;

inline std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Values are never included in errors or output. Explicit process variables win.
inline Values load_file() {
    const char* configured = std::getenv("GOMMANNA_ENV_FILE");
    const bool explicit_path = configured && *configured;
    const std::filesystem::path path = explicit_path ? configured : ".env";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (explicit_path || std::filesystem::exists(path))
            throw std::runtime_error("Could not read environment file");
        return {};
    }
    Values values;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number == 1 && line.rfind("\xef\xbb\xbf", 0) == 0) line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error("Invalid environment file line " + std::to_string(line_number));
        const auto key = trim(line.substr(0, separator));
        if (key.empty() || !(std::isalpha(static_cast<unsigned char>(key.front())) || key.front() == '_'))
            throw std::runtime_error("Invalid environment file key on line " + std::to_string(line_number));
        for (const auto ch : key) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_'))
                throw std::runtime_error("Invalid environment file key on line " + std::to_string(line_number));
        }
        auto value = trim(line.substr(separator + 1));
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            if (value.size() < 2 || value.back() != value.front())
                throw std::runtime_error("Unclosed environment file value on line " + std::to_string(line_number));
            value = value.substr(1, value.size() - 2);
        }
        if (value.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid environment file value on line " + std::to_string(line_number));
        values[key] = std::move(value);
    }
    if (input.bad()) throw std::runtime_error("Could not read environment file");
    return values;
}

inline std::string get(const Values& file_values, const char* name, const char* fallback = "") {
    if (const auto* from_process = std::getenv(name)) return from_process;
    const auto found = file_values.find(name);
    return found == file_values.end() ? fallback : found->second;
}
} // namespace gommanna::http_config
