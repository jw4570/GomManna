#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace gm {
using Json = nlohmann::json;
struct ApiError : std::runtime_error {
    int status;
    std::string code;
    ApiError(int status, std::string code, std::string message)
        : std::runtime_error(std::move(message)), status(status), code(std::move(code)) {}
};
struct Config {
    std::string database_url, auth_secret, host = "127.0.0.1";
    int port = 8080, pool_size = 4, token_ttl_hours = 720, room_ttl_hours = 24;
    static Config load();
};
std::string text(const Json& body, const std::string& key, size_t max_length);
std::string uuid(const std::string& value);
double number(const Json& body, const std::string& key, double min, double max);
std::string random_code();
class Auth {
    std::string secret_;
    int ttl_;
public:
    explicit Auth(const Config& config) : secret_(config.auth_secret), ttl_(config.token_ttl_hours) {}
    std::string issue(const std::string& user_id) const;
    std::string verify(const std::string& token) const;
};
class RateLimiter {
    struct Entry { int count = 0; std::chrono::steady_clock::time_point start; };
    std::mutex mutex_;
    std::map<std::string, Entry> entries_;
public:
    void check(const std::string& key, int limit, std::chrono::seconds window);
};
}
