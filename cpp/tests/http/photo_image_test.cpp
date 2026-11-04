#include "gommanna/photo_image.hpp"
#include "stb_image_write.h"

#include <iostream>
#include <string>

using namespace gommanna;
using namespace std::string_literals;
#define CHECK(test) do { if (!(test)) { std::cerr << "FAIL line " << __LINE__ << ": " #test "\n"; return 1; } } while (false)

void append(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
}
int main() {
    const unsigned char pixels[] = {255,0,0, 0,255,0, 0,0,255, 255,255,255};
    std::string jpeg, png;
    CHECK(stbi_write_jpg_to_func(append, &jpeg, 2, 2, 3, pixels, 90) != 0);
    CHECK(stbi_write_png_to_func(append, &png, 2, 2, 3, pixels, 6) != 0);
    // Add a synthetic APP1/EXIF segment containing a marker to the valid JPEG.
    const std::string marker = "Exif\0\0TEST-GPS-SECRET"s;
    const unsigned length = static_cast<unsigned>(marker.size()) + 2;
    const std::string segment = std::string("\xFF\xE1", 2) +
        static_cast<char>(length >> 8) + static_cast<char>(length & 0xff) + marker;
    jpeg.insert(2, segment);
    CHECK(jpeg.find("TEST-GPS-SECRET") != std::string::npos);
    SanitizedPhoto output;
    CHECK(sanitize_photo(jpeg, output) == PhotoImageStatus::ok);
    CHECK(output.content_type == "image/jpeg");
    CHECK(output.bytes.find("TEST-GPS-SECRET") == std::string::npos);
    CHECK(sanitize_photo(png, output) == PhotoImageStatus::ok);
    CHECK(output.content_type == "image/png");
    CHECK(sanitize_photo("not an image", output) == PhotoImageStatus::invalid);
    CHECK(sanitize_photo(jpeg.substr(0, 10), output) == PhotoImageStatus::invalid);
    CHECK(sanitize_photo(std::string(kMaximumPhotoBytes + 1, 'x'), output) == PhotoImageStatus::too_large);
    std::cout << "PASS: JPEG/PNG validation, limits and EXIF removal\n";
}
