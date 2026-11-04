#include "gommanna/photo_image.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace gommanna {
namespace {
std::mutex image_mutex;
constexpr int kMaximumDimension = 4096;
constexpr std::int64_t kMaximumPixels = 16LL * 1024 * 1024;

struct Output {
    std::string bytes;
    bool too_large = false;
};
void write_bytes(void* context, void* data, int size) {
    auto& output = *static_cast<Output*>(context);
    if (size < 0 || static_cast<size_t>(size) > kMaximumPhotoBytes - output.bytes.size()) {
        output.too_large = true;
        return;
    }
    if (!output.too_large) output.bytes.append(static_cast<const char*>(data), static_cast<size_t>(size));
}
}

PhotoImageStatus sanitize_photo(std::string_view input, SanitizedPhoto& output) {
    output = {};
    if (input.size() > kMaximumPhotoBytes) return PhotoImageStatus::too_large;
    if (input.size() < 8 || input.size() > INT_MAX) return PhotoImageStatus::invalid;

    static constexpr unsigned char png[] = {137,80,78,71,13,10,26,10};
    const auto* data = reinterpret_cast<const unsigned char*>(input.data());
    const bool is_png = std::equal(std::begin(png), std::end(png), data);
    const bool is_jpeg = data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff;
    if (!is_png && !is_jpeg) return PhotoImageStatus::invalid;

    // Bound both memory and simultaneous decoder work for untrusted uploads.
    std::lock_guard<std::mutex> guard(image_mutex);
    int width = 0, height = 0, original_channels = 0;
    if (!stbi_info_from_memory(data, static_cast<int>(input.size()), &width, &height, &original_channels) ||
        width < 1 || height < 1 || width > kMaximumDimension || height > kMaximumDimension ||
        static_cast<std::int64_t>(width) * height > kMaximumPixels) {
        return PhotoImageStatus::invalid;
    }
    const int channels = is_png ? 4 : 3;
    using Pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    Pixels pixels(stbi_load_from_memory(data, static_cast<int>(input.size()), &width, &height,
                                       &original_channels, channels), &stbi_image_free);
    if (!pixels) return PhotoImageStatus::invalid;
    Output encoded;
    const int success = is_png
        ? stbi_write_png_to_func(write_bytes, &encoded, width, height, channels, pixels.get(), width * channels)
        : stbi_write_jpg_to_func(write_bytes, &encoded, width, height, channels, pixels.get(), 90);
    if (encoded.too_large) return PhotoImageStatus::too_large;
    if (!success || encoded.bytes.empty()) return PhotoImageStatus::invalid;
    output.bytes = std::move(encoded.bytes);
    output.content_type = is_png ? "image/png" : "image/jpeg";
    return PhotoImageStatus::ok;
}
} // namespace gommanna
