// This explicit test-only fallback does not implement the teammate's room service.
#include "gommanna/contracts.hpp"

namespace gommanna {
namespace {
class UnavailableRoomService final : public IRoomService {
    static Error unavailable() {
        return {ErrorCode::not_implemented, "Room service is not available in this test build"};
    }
public:
    Result<RoomMembership> create_room(const Id&, const std::string&,
                                      const std::string&, TimeMs) override {
        return unavailable();
    }
    Result<RoomMembership> join_room(const Id&, const std::string&,
                                    const std::string&, TimeMs) override {
        return unavailable();
    }
    Result<Unit> leave_room(const Id&, const Id&, TimeMs) override { return unavailable(); }
    Result<Unit> end_room(const Id&, const Id&, TimeMs) override { return unavailable(); }
    Result<Invite> get_invite(const Id&, const Id&) override { return unavailable(); }
};
} // namespace

std::unique_ptr<IRoomService> make_room_service(IRepository&) {
    return std::make_unique<UnavailableRoomService>();
}
} // namespace gommanna
