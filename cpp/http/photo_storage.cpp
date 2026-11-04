#include "gommanna/photo_storage.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <regex>
#include <string>
#include <utility>

namespace gommanna {
namespace {
bool valid_key(const std::string& key) {
    static const std::regex pattern(R"(^v1/[0-9a-f]{64}[.](jpg|png)$)");
    return std::regex_match(key, pattern);
}
Error unavailable() {
    return {ErrorCode::storage_unavailable, "photo storage unavailable"};
}

class SupabasePhotoStorage final : public PhotoStorage {
public:
    explicit SupabasePhotoStorage(PhotoStorageConfig config) : config_(std::move(config)) {}

    Result<Unit> upload(const std::string& key, const SanitizedPhoto& photo) override {
        if (!valid_key(key) || photo.bytes.empty() || photo.bytes.size() > kMaximumPhotoBytes ||
            (photo.content_type != "image/png" && photo.content_type != "image/jpeg"))
            return Error{ErrorCode::invalid_argument, "invalid photo"};
        auto client = make_client();
        const auto response = client->Post("/storage/v1/object/" + config_.bucket + "/" + key,
                                            headers(), photo.bytes, photo.content_type);
        if (!response || response->status < 200 || response->status >= 300) return unavailable();
        return Unit{};
    }

    Result<SanitizedPhoto> download(const std::string& key) override {
        if (!valid_key(key)) return Error{ErrorCode::invalid_argument, "invalid photo key"};
        auto client = make_client();
        const auto response = client->Get("/storage/v1/object/authenticated/" + config_.bucket + "/" + key,
                                          headers());
        if (!response || response->status != 200 || response->body.empty() ||
            response->body.size() > kMaximumPhotoBytes) return unavailable();
        return SanitizedPhoto{response->body, key.ends_with(".png") ? "image/png" : "image/jpeg"};
    }

    Result<Unit> remove(const std::string& key) override {
        if (!valid_key(key)) return Error{ErrorCode::invalid_argument, "invalid photo key"};
        auto client = make_client();
        const auto body = nlohmann::json{{"prefixes", nlohmann::json::array({key})}}.dump();
        const auto response = client->Delete("/storage/v1/object/" + config_.bucket, headers(),
                                              body, "application/json");
        if (!response || (response->status != 404 && (response->status < 200 || response->status >= 300)))
            return unavailable();
        return Unit{};
    }

private:
    std::unique_ptr<httplib::Client> make_client() const {
        auto client = std::make_unique<httplib::Client>(config_.project_url);
        client->set_connection_timeout(10, 0);
        client->set_read_timeout(20, 0);
        client->set_write_timeout(20, 0);
        client->set_follow_location(false);
        client->enable_server_certificate_verification(true);
        if (!config_.ca_file.empty()) client->set_ca_cert_path(config_.ca_file);
        return client;
    }
    httplib::Headers headers() const {
        httplib::Headers result{{"apikey", config_.server_secret}, {"x-upsert", "false"}};
        // New sb_secret keys are opaque, not JWTs. Only legacy service_role
        // JWTs belong in the Authorization header.
        if (!config_.server_secret.starts_with("sb_secret_"))
            result.emplace("Authorization", "Bearer " + config_.server_secret);
        return result;
    }
    PhotoStorageConfig config_;
};
} // namespace

Result<std::unique_ptr<PhotoStorage>> make_supabase_photo_storage(PhotoStorageConfig config) {
    while (!config.project_url.empty() && config.project_url.back() == '/') config.project_url.pop_back();
    static const std::regex url(R"(^https://[A-Za-z0-9.-]+(:[0-9]{1,5})?$)");
    static const std::regex bucket(R"(^[a-z0-9][a-z0-9-]{1,61}[a-z0-9]$)");
    if (!std::regex_match(config.project_url, url) || !std::regex_match(config.bucket, bucket) ||
        config.server_secret.empty() || config.server_secret.find_first_of("\r\n") != std::string::npos) {
        return Error{ErrorCode::invalid_argument, "invalid photo storage configuration"};
    }
    return std::unique_ptr<PhotoStorage>(std::make_unique<SupabasePhotoStorage>(std::move(config)));
}
} // namespace gommanna
