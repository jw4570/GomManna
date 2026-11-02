#include "gommanna/core.hpp"
#include <jwt-cpp/jwt.h>
#include <openssl/rand.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <regex>

namespace gm {
namespace {
std::string trim(std::string s) {
    auto white = [](unsigned char c) { return std::isspace(c); };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), white));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), white).base(), s.end());
    return s;
}
}
Config Config::load() {
    std::map<std::string, std::string> file;
    std::ifstream input(".env");
    for (std::string line; std::getline(input, line);) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto at = line.find('=');
        if (at == std::string::npos) continue;
        auto value = trim(line.substr(at + 1));
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1, value.size() - 2);
        file[trim(line.substr(0, at))] = value;
    }
    auto get = [&](const std::string& key, const std::string& fallback) {
        if (const char* v = std::getenv(key.c_str())) return std::string(v);
        auto it = file.find(key);
        return it == file.end() ? fallback : it->second;
    };
    auto integer = [&](const char* key, int fallback, int min, int max) {
        const auto raw = get(key, std::to_string(fallback));
        size_t used = 0;
        int n;
        try { n = std::stoi(raw, &used); }
        catch (...) { throw std::runtime_error(std::string("Invalid configuration: ") + key); }
        if (used != raw.size() || n < min || n > max)
            throw std::runtime_error(std::string("Invalid configuration: ") + key);
        return n;
    };
    Config c;
    c.database_url = get("DATABASE_URL", "");
    c.auth_secret = get("AUTH_SECRET", "");
    c.host = get("HOST", "127.0.0.1");
    c.port = integer("PORT", 8080, 1, 65535);
    c.pool_size = integer("DB_POOL_SIZE", 4, 1, 32);
    c.token_ttl_hours = integer("TOKEN_TTL_HOURS", 720, 1, 8760);
    c.room_ttl_hours = integer("ROOM_TTL_HOURS", 24, 1, 168);
    if (c.database_url.empty()) throw std::runtime_error("DATABASE_URL is required");
    if (c.auth_secret.size() < 32 || c.auth_secret.find("REPLACE_") != std::string::npos)
        throw std::runtime_error("Set AUTH_SECRET to a random secret of at least 32 characters");
    return c;
}
std::string text(const Json& body, const std::string& key, size_t max_length) {
    if (!body.contains(key) || !body[key].is_string())
        throw ApiError(400, "invalid_input", key + " must be a string");
    auto value = trim(body[key].get<std::string>());
    size_t length = 0;
    for (unsigned char c : value) {
        if (c == 0) throw ApiError(400, "invalid_input", key + " contains an invalid character");
        if ((c & 0xc0) != 0x80) ++length;
    }
    if (length == 0 || length > max_length)
        throw ApiError(400, "invalid_input", key + " has an invalid length");
    return value;
}
std::string uuid(const std::string& value) {
    static const std::regex pattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    if (!std::regex_match(value, pattern)) throw ApiError(400, "invalid_id", "Invalid UUID");
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}
double number(const Json& body, const std::string& key, double min, double max) {
    if (!body.contains(key) || !body[key].is_number())
        throw ApiError(400, "invalid_input", key + " must be a number");
    auto value = body[key].get<double>();
    if (!std::isfinite(value) || value < min || value > max)
        throw ApiError(400, "invalid_input", key + " is out of range");
    return value;
}
std::string random_code() {
    unsigned char bytes[8];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Random generation failed");
    // 문자 32개를 사용해 무작위 바이트 변환 시 분포를 균등하게 유지합니다.
    constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string result;
    for (auto b : bytes) result += alphabet[b & 31];
    return result;
}
std::string Auth::issue(const std::string& user_id) const {
    const auto now = std::chrono::system_clock::now();
    return jwt::create().set_type("JWT").set_issuer("gommanna-server")
        .set_audience("gommanna-ios").set_subject(uuid(user_id))
        .set_issued_at(now).set_expires_at(now + std::chrono::hours(ttl_))
        .sign(jwt::algorithm::hs256{secret_});
}
std::string Auth::verify(const std::string& token) const {
    if (token.empty() || token.size() > 4096) throw ApiError(401, "unauthorized", "Invalid session");
    try {
        auto decoded = jwt::decode(token);
        jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret_})
            .with_issuer("gommanna-server").with_audience("gommanna-ios").verify(decoded);
        if (!decoded.has_expires_at() || !decoded.has_issued_at())
            throw std::runtime_error("Missing token claims");
        return uuid(decoded.get_subject());
    } catch (...) { throw ApiError(401, "unauthorized", "Session expired or invalid"); }
}
void RateLimiter::check(const std::string& key, int limit, std::chrono::seconds window) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);
    if (entries_.size() >= 4096) {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (now - it->second.start >= std::chrono::hours(1)) it = entries_.erase(it);
            else ++it;
        }
        if (entries_.size() >= 4096 && !entries_.contains(key))
            throw ApiError(429, "rate_limited", "Please try again later");
    }
    auto& entry = entries_[key];
    if (entry.count == 0 || now - entry.start >= window) entry = {0, now};
    if (++entry.count > limit) throw ApiError(429, "rate_limited", "Please try again later");
}
}
