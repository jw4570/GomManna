#include "photo_service.hpp"
#include "stb_image_write.h"

#include <functional>
#include <iostream>
#include <map>
#include <string>

using namespace gommanna;
#define CHECK(test) do { if (!(test)) { std::cerr << "FAIL line " << __LINE__ << ": " #test "\n"; return 1; } } while (false)

class MemoryStorage final : public PhotoStorage {
public:
    std::map<std::string, SanitizedPhoto> objects;
    std::function<void()> after_upload, after_download;
    Result<Unit> upload(const std::string& key, const SanitizedPhoto& photo) override {
        objects[key] = photo;
        if (after_upload) after_upload();
        return Unit{};
    }
    Result<SanitizedPhoto> download(const std::string& key) override {
        if (!objects.contains(key)) return Error{ErrorCode::storage_unavailable, "missing"};
        auto result = objects.at(key);
        if (after_download) after_download();
        return result;
    }
    Result<Unit> remove(const std::string& key) override {
        objects.erase(key);
        return Unit{};
    }
};

void append(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
}

int main() {
    auto db = make_fake_repository();
    auto rooms = make_room_service(*db);
    const auto host = std::get<User>(db->create_guest("host", 1000, 9999999));
    const auto guest = std::get<User>(db->create_guest("guest", 1000, 9999999));
    const auto outsider = std::get<User>(db->create_guest("outsider", 1000, 9999999));
    const auto created = std::get<RoomMembership>(rooms->create_room(host.id, "Travel", "Host", 1000));
    const auto room = created.room.id;
    const auto member_id = created.member.id;
    CHECK(std::holds_alternative<RoomMembership>(rooms->join_room(guest.id, created.room.invite_code, "Guest", 1001)));
    CHECK(std::holds_alternative<Member>(db->set_sharing(host.id, room, true, 1002)));
    auto note = std::get<Note>(db->save_note(host.id, room, {"entrance", 1}, 1003));
    const unsigned char pixels[] = {255,0,0, 0,255,0, 0,0,255, 255,255,255};
    std::string png;
    CHECK(stbi_write_png_to_func(append, &png, 2, 2, 3, pixels, 6) != 0);
    MemoryStorage storage;
    PhotoService photos(*db, storage);

    auto bad = photos.upload(host.id, room, note.id, note.written_at_ms, 1, "not a photo", 1004);
    CHECK(std::get<Error>(bad).code == ErrorCode::invalid_argument);
    auto uploaded = photos.upload(host.id, room, note.id, note.written_at_ms, 1, png, 1004);
    CHECK(std::holds_alternative<AttachedPhoto>(uploaded));
    CHECK(storage.objects.size() == 1);
    auto downloaded = photos.download(guest.id, room, member_id, 1005);
    CHECK(std::holds_alternative<SanitizedPhoto>(downloaded));
    CHECK(std::get<SanitizedPhoto>(downloaded).content_type == "image/png");
    auto blocked = photos.download(outsider.id, room, member_id, 1005);
    CHECK(std::get<Error>(blocked).code == ErrorCode::forbidden);

    // Switch OFF while the storage read is in progress; no image may escape.
    storage.after_download = [&] { (void)db->set_sharing(host.id, room, false, 1006); };
    blocked = photos.download(guest.id, room, member_id, 1006);
    CHECK(std::get<Error>(blocked).code == ErrorCode::not_found);
    storage.after_download = {};
    CHECK(std::holds_alternative<Member>(db->set_sharing(host.id, room, true, 1007)));
    blocked = photos.download(guest.id, room, member_id, 1007);
    CHECK(std::get<Error>(blocked).code == ErrorCode::not_found);
    note = std::get<Note>(db->save_note(host.id, room, {"new note", 2}, 1008));
    photos.collect_garbage(1009);
    CHECK(storage.objects.empty());

    // Switch OFF after upload but before DB attachment. The object must stay
    // inaccessible and its cleanup reservation must be collected after the grace period.
    storage.after_upload = [&] { (void)db->set_sharing(host.id, room, false, 1010); };
    auto raced = photos.upload(host.id, room, note.id, note.written_at_ms, 2, png, 1010);
    CHECK(std::get<Error>(raced).code == ErrorCode::sharing_off);
    CHECK(storage.objects.size() == 1);
    photos.collect_garbage(1011);
    CHECK(storage.objects.size() == 1);
    photos.collect_garbage(1010 + 10 * 60 * 1000);
    CHECK(storage.objects.empty());
    std::cout << "PASS: photo upload, access control, sharing races and cleanup\n";
}
