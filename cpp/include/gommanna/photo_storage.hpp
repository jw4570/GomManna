#pragma once

#include "gommanna/contracts.hpp"
#include "gommanna/photo_image.hpp"

#include <memory>
#include <string>

namespace gommanna {
class PhotoStorage {
public:
    virtual ~PhotoStorage() = default;
    virtual Result<Unit> upload(const std::string& key, const SanitizedPhoto& photo) = 0;
    virtual Result<SanitizedPhoto> download(const std::string& key) = 0;
    virtual Result<Unit> remove(const std::string& key) = 0;
};

struct PhotoStorageConfig {
    std::string project_url;
    std::string server_secret;
    std::string bucket;
    std::string ca_file;
};

Result<std::unique_ptr<PhotoStorage>> make_supabase_photo_storage(PhotoStorageConfig config);
} // namespace gommanna
