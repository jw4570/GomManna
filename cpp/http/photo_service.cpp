#include "photo_service.hpp"

#include <openssl/rand.h>

#include <algorithm>
#include <string>
#include <utility>

namespace gommanna {
namespace {
Result<MemberView> visible_member(IRepository& repository, const Id& actor,
                                  const Id& room, const Id& member_id, TimeMs now) {
    auto result = repository.list_visible_members(actor, room, now);
    if (const auto* error = std::get_if<Error>(&result)) return *error;
    for (auto& view : std::get<std::vector<MemberView>>(result)) {
        if (view.member.id == member_id) return std::move(view);
    }
    return Error{ErrorCode::not_found, "member not found"};
}

Result<MemberView> own_member(IRepository& repository, const Id& actor,
                              const Id& room, TimeMs now) {
    auto result = repository.list_visible_members(actor, room, now);
    if (const auto* error = std::get_if<Error>(&result)) return *error;
    for (auto& view : std::get<std::vector<MemberView>>(result)) {
        if (view.member.user_id == actor) return std::move(view);
    }
    return Error{ErrorCode::forbidden, "not a room member"};
}

std::string random_key(const std::string& content_type) {
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof bytes) != 1) return {};
    static constexpr char digits[] = "0123456789abcdef";
    std::string key = "v1/";
    for (const auto byte : bytes) {
        key += digits[byte >> 4];
        key += digits[byte & 15];
    }
    return key + (content_type == "image/png" ? ".png" : ".jpg");
}
}

Result<AttachedPhoto> PhotoService::upload(const Id& actor, const Id& room,
    const Id& note_id, TimeMs written_at, Epoch epoch, std::string_view data, TimeMs now) {
    auto own = own_member(repository_, actor, room, now);
    if (const auto* error = std::get_if<Error>(&own)) return *error;
    const auto& view = std::get<MemberView>(own);
    if (!view.member.is_sharing) return Error{ErrorCode::sharing_off, "sharing is off"};
    if (view.member.sharing_epoch != epoch) return Error{ErrorCode::stale_update, "outdated sharing session"};
    if (!view.note) return Error{ErrorCode::not_found, "note not found"};
    if (view.note->id != note_id || view.note->written_at_ms != written_at ||
        view.note->sharing_epoch != epoch)
        return Error{ErrorCode::stale_update, "note changed before photo upload"};

    SanitizedPhoto image;
    const auto image_status = sanitize_photo(data, image);
    if (image_status == PhotoImageStatus::too_large)
        return Error{ErrorCode::photo_too_large, "photo exceeds 5 MiB"};
    if (image_status != PhotoImageStatus::ok)
        return Error{ErrorCode::invalid_argument, "invalid JPEG or PNG photo"};
    const auto key = random_key(image.content_type);
    if (key.empty()) return Error{ErrorCode::internal, "secure photo key generation failed"};

    // A delayed cleanup reservation exists before any remote object is written.
    // Upload timeout is much shorter than this grace period.
    auto reserved = repository_.reserve_photo_cleanup(key, now + 10LL * 60 * 1000);
    if (const auto* error = std::get_if<Error>(&reserved)) return *error;
    auto stored = storage_.upload(key, image);
    if (const auto* error = std::get_if<Error>(&stored)) return *error;
    auto attached = repository_.attach_photo(actor, room, note_id, written_at,
                                              PhotoRef{key, now}, epoch, now);
    if (const auto* error = std::get_if<Error>(&attached)) return *error;
    return AttachedPhoto{std::get<Note>(std::move(attached)), view.member.id};
}

Result<SanitizedPhoto> PhotoService::download(const Id& actor, const Id& room,
                                              const Id& member_id, TimeMs now) {
    auto first = visible_member(repository_, actor, room, member_id, now);
    if (const auto* error = std::get_if<Error>(&first)) return *error;
    const auto& before = std::get<MemberView>(first);
    if (!before.member.is_sharing || !before.note || !before.note->photo)
        return Error{ErrorCode::not_found, "photo not found"};
    const auto key = before.note->photo->key;
    auto image = storage_.download(key);
    if (const auto* error = std::get_if<Error>(&image)) return *error;

    // A share may have been disabled or the note replaced during download.
    auto again = visible_member(repository_, actor, room, member_id, now);
    if (const auto* error = std::get_if<Error>(&again)) return *error;
    const auto& after = std::get<MemberView>(again);
    if (!after.member.is_sharing || !after.note || !after.note->photo ||
        after.note->photo->key != key ||
        after.member.sharing_epoch != before.member.sharing_epoch)
        return Error{ErrorCode::not_found, "photo not found"};
    return std::get<SanitizedPhoto>(std::move(image));
}

void PhotoService::collect_garbage(TimeMs now) {
    auto due = repository_.due_photo_cleanup(now);
    if (auto* keys = std::get_if<std::vector<std::string>>(&due)) {
        for (const auto& key : *keys) {
            if (std::holds_alternative<Unit>(storage_.remove(key)))
                (void)repository_.finish_photo_cleanup(key);
        }
    }
}
} // namespace gommanna
