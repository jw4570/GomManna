#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace gommanna {
inline constexpr std::size_t kMaximumPhotoBytes = 5 * 1024 * 1024;

struct SanitizedPhoto {
    std::string bytes;
    std::string content_type;
};

enum class PhotoImageStatus { ok, too_large, invalid };

// Accept JPEG/PNG up to 5 MiB, limit decoded dimensions and remove metadata.
// The output is owned by the caller and never contains the input's EXIF chunks.
PhotoImageStatus sanitize_photo(std::string_view input, SanitizedPhoto& output);
} // namespace gommanna
