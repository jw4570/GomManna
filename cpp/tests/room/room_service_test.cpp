#include "gommanna/contracts.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <variant>

using namespace gommanna;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "FAIL at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

// Wrap the in-memory DB so collision handling is deterministic and independent
// of the quality or state of the operating system's random number generator.
class CollisionRepository final : public IRepository {
public:
    explicit CollisionRepository(IRepository& target) : target_(target) {}
    int conflicts_left = 0;
    int create_attempts = 0;
    bool fail_database = false;

    Result<User> create_guest(const std::string& hash, TimeMs a, TimeMs b) override {
        return target_.create_guest(hash, a, b);
    }
    Result<User> authenticate_guest(const std::string& hash, TimeMs now) override {
        return target_.authenticate_guest(hash, now);
    }
    Result<Room> get_room(const Id& actor, const Id& room) override {
        return target_.get_room(actor, room);
    }
    Result<Room> find_room_by_code(const std::string& code) override {
        return target_.find_room_by_code(code);
    }
    Result<RoomMembership> create_room(const Id& actor, const std::string& title,
        const std::string& nickname, const std::string& code, TimeMs now) override {
        ++create_attempts;
        if (fail_database) return Error{ErrorCode::db_unavailable, "database unavailable"};
        if (conflicts_left-- > 0) return Error{ErrorCode::invite_code_conflict, "collision"};
        return target_.create_room(actor, title, nickname, code, now);
    }
    Result<Member> join_room(const Id& actor, const Id& room,
                             const std::string& nickname, TimeMs now) override {
        return target_.join_room(actor, room, nickname, now);
    }
    Result<Unit> leave_room(const Id& actor, const Id& room, TimeMs now) override {
        return target_.leave_room(actor, room, now);
    }
    Result<Unit> end_room(const Id& actor, const Id& room, TimeMs now) override {
        return target_.end_room(actor, room, now);
    }
    Result<std::vector<MemberView>> list_visible_members(const Id& actor,
                                                          const Id& room, TimeMs now) override {
        return target_.list_visible_members(actor, room, now);
    }
    Result<Member> set_sharing(const Id& actor, const Id& room, bool enabled, TimeMs now) override {
        return target_.set_sharing(actor, room, enabled, now);
    }
    Result<Location> update_location(const Id& actor, const Id& room,
                                      const LocationUpdate& update, TimeMs now) override {
        return target_.update_location(actor, room, update, now);
    }
    Result<Note> save_note(const Id& actor, const Id& room,
                            const NoteDraft& draft, TimeMs now) override {
        return target_.save_note(actor, room, draft, now);
    }
    Result<Unit> delete_note(const Id& actor, const Id& room, TimeMs now) override {
        return target_.delete_note(actor, room, now);
    }
    Result<Note> attach_photo(const Id& actor, const Id& room, const Id& note,
        TimeMs written, const PhotoRef& photo, Epoch epoch, TimeMs now) override {
        return target_.attach_photo(actor, room, note, written, photo, epoch, now);
    }
    Result<Unit> reserve_photo_cleanup(const std::string& key, TimeMs now) override {
        return target_.reserve_photo_cleanup(key, now);
    }
    Result<std::vector<std::string>> due_photo_cleanup(TimeMs now) override {
        return target_.due_photo_cleanup(now);
    }
    Result<Unit> finish_photo_cleanup(const std::string& key) override {
        return target_.finish_photo_cleanup(key);
    }
private:
    IRepository& target_;
};

int main() {
    auto fake = make_fake_repository();
    CollisionRepository db(*fake);
    auto service = make_room_service(db);
    const auto host = std::get<User>(db.create_guest("host", 1000, 999999));
    const auto guest = std::get<User>(db.create_guest("guest", 1000, 999999));
    const auto outsider = std::get<User>(db.create_guest("outsider", 1000, 999999));

    db.conflicts_left = 2;
    auto created = service->create_room(host.id, "Travel", "Host", 1000);
    CHECK(std::holds_alternative<RoomMembership>(created));
    CHECK(db.create_attempts == 3);
    auto room = std::get<RoomMembership>(created);
    CHECK(room.room.invite_code.size() == 6);
    auto invite = service->get_invite(host.id, room.room.id);
    CHECK(std::holds_alternative<Invite>(invite));
    const auto& data = std::get<Invite>(invite);
    CHECK(data.invite_code == room.room.invite_code);
    CHECK(data.qr_payload == "gommanna://join?code=" + data.invite_code);
    CHECK(data.qr_svg.rfind("<svg ", 0) == 0);
    auto forbidden = service->get_invite(outsider.id, room.room.id);
    CHECK(std::get<Error>(forbidden).code == ErrorCode::forbidden);

    auto invalid = service->join_room(guest.id, "ABCD2345", "Guest", 1001);
    CHECK(std::get<Error>(invalid).code == ErrorCode::invalid_argument);
    std::string lower = room.room.invite_code;
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    auto joined = service->join_room(guest.id, "  " + lower + "\t", "Guest", 1001);
    CHECK(std::holds_alternative<RoomMembership>(joined));
    CHECK(std::get<RoomMembership>(joined).room.id == room.room.id);

    db.create_attempts = 0;
    db.conflicts_left = 6;
    auto exhausted = service->create_room(host.id, "Travel", "Host", 1002);
    CHECK(std::get<Error>(exhausted).code == ErrorCode::invite_code_conflict);
    CHECK(db.create_attempts == 5);
    db.create_attempts = 0;
    db.fail_database = true;
    auto db_error = service->create_room(host.id, "Travel", "Host", 1002);
    CHECK(std::get<Error>(db_error).code == ErrorCode::db_unavailable);
    CHECK(db.create_attempts == 1);
    db.fail_database = false;
    CHECK(std::holds_alternative<Unit>(service->end_room(host.id, room.room.id, 1003)));
    auto closed = service->join_room(guest.id, room.room.invite_code, "Guest", 1004);
    CHECK(std::get<Error>(closed).code == ErrorCode::room_closed);
    std::cout << "PASS: room service, invite, QR, permissions and collision handling\n";
}
