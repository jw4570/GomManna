#pragma once

#include "gommanna/contracts.hpp"
#include "gommanna/photo_storage.hpp"

#include <string_view>

namespace gommanna {
struct AttachedPhoto {
    Note note;
    Id member_id;
};

class PhotoService {
public:
    PhotoService(IRepository& repository, PhotoStorage& storage)
        : repository_(repository), storage_(storage) {}

    Result<AttachedPhoto> upload(const Id& actor, const Id& room, const Id& note_id,
        TimeMs expected_written_at_ms, Epoch epoch, std::string_view data, TimeMs now);
    Result<SanitizedPhoto> download(const Id& actor, const Id& room,
        const Id& member_id, TimeMs now);
    void collect_garbage(TimeMs now);

private:
    IRepository& repository_;
    PhotoStorage& storage_;
};
} // namespace gommanna
