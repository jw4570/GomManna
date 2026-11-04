#include "gommanna/contracts.hpp"
#include "gommanna/room_invite.h"

#include <memory>
#include <string>
#include <utility>

namespace gommanna {
namespace {

constexpr int kMaxInviteAttempts = 5;

// The human-entered code can have surrounding whitespace and lowercase letters.
// Interior spaces and all characters outside the shared alphabet are invalid.
std::string normalized_code(const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const auto end = input.find_last_not_of(" \t\r\n");
    std::string code = input.substr(start, end - start + 1);
    for (char& c : code) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return code;
}

class RoomService final : public IRoomService {
public:
    explicit RoomService(IRepository& repository) : repository_(repository) {}

    Result<RoomMembership> create_room(const Id& actor, const std::string& title,
                                       const std::string& nickname, TimeMs now) override {
        for (int attempt = 0; attempt < kMaxInviteAttempts; ++attempt) {
            char code[GM_ROOM_CODE_CAPACITY]{};
            if (gm_room_code_generate(code, sizeof(code)) != GM_ROOM_OK) {
                return Error{ErrorCode::internal, "secure invite generation failed"};
            }
            auto result = repository_.create_room(actor, title, nickname, code, now);
            const auto* error = std::get_if<Error>(&result);
            if (!error || error->code != ErrorCode::invite_code_conflict) return result;
        }
        return Error{ErrorCode::invite_code_conflict, "could not allocate an invite code"};
    }

    Result<RoomMembership> join_room(const Id& actor, const std::string& invite,
                                     const std::string& nickname, TimeMs now) override {
        const auto code = normalized_code(invite);
        if (!gm_room_code_is_valid(code.c_str())) {
            return Error{ErrorCode::invalid_argument, "invalid invite code"};
        }
        auto found = repository_.find_room_by_code(code);
        if (const auto* error = std::get_if<Error>(&found)) return *error;
        const auto room = std::get<Room>(std::move(found));
        if (!room.active) return Error{ErrorCode::room_closed, "room is closed"};
        auto joined = repository_.join_room(actor, room.id, nickname, now);
        if (const auto* error = std::get_if<Error>(&joined)) return *error;
        return RoomMembership{room, std::get<Member>(std::move(joined))};
    }

    Result<Unit> leave_room(const Id& actor, const Id& room, TimeMs now) override {
        return repository_.leave_room(actor, room, now);
    }

    Result<Unit> end_room(const Id& actor, const Id& room, TimeMs now) override {
        return repository_.end_room(actor, room, now);
    }

    Result<Invite> get_invite(const Id& actor, const Id& room_id) override {
        auto found = repository_.get_room(actor, room_id);
        if (const auto* error = std::get_if<Error>(&found)) return *error;
        const auto& code = std::get<Room>(found).invite_code;
        char payload[GM_ROOM_PAYLOAD_CAPACITY]{};
        size_t required = 0;
        if (gm_room_invite_payload(code.c_str(), payload, sizeof(payload)) != GM_ROOM_OK ||
            gm_room_qr_svg(code.c_str(), nullptr, 0, &required) != GM_ROOM_OK ||
            required < 2 || required > GM_ROOM_QR_SVG_CAPACITY) {
            return Error{ErrorCode::internal, "could not generate invite"};
        }
        std::string svg(required, '\0');
        if (gm_room_qr_svg(code.c_str(), svg.data(), svg.size(), &required) != GM_ROOM_OK) {
            return Error{ErrorCode::internal, "could not generate invite"};
        }
        svg.resize(required - 1);
        return Invite{code, payload, std::move(svg)};
    }

private:
    IRepository& repository_;
};
} // namespace

std::unique_ptr<IRoomService> make_room_service(IRepository& repository) {
    return std::make_unique<RoomService>(repository);
}
} // namespace gommanna
