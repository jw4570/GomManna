#pragma once
// GomManna shared contract v1.1 - declarations only.
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace gommanna {
using Id = std::string;                 // UUID string
using TimeMs = std::int64_t;            // UTC Unix milliseconds
using Epoch = std::int64_t;
using Unit = std::monostate;
inline constexpr int kContractVersion = 1;
inline constexpr double kWalkingSpeedMPerMin = 80.0;
inline constexpr double kEarthRadiusM = 6371000.0;
inline constexpr TimeMs kDelayedAfterMs = 30000;
inline constexpr TimeMs kStaleAfterMs = 120000;
inline constexpr TimeMs kFutureToleranceMs = 5000;
// Compatibility duration for fixtures. Production uses six calendar months (UTC).
inline constexpr TimeMs kGuestSessionTtlMs = 180LL * 24 * 60 * 60 * 1000;
inline constexpr TimeMs kRetentionMs = 48LL * 60 * 60 * 1000;
inline constexpr TimeMs kRoomInactivityMs = 10LL * 60 * 1000;
inline constexpr TimeMs kRouteHeartbeatMs = 60LL * 1000;
inline constexpr double kRouteMinimumDistanceM = 20.0;
inline constexpr int kMaxRoomMembers = 8;

enum class ErrorCode {
    invalid_argument, unauthenticated, forbidden, not_found, conflict,
    invite_code_conflict,
    room_closed, sharing_off, stale_update, db_unavailable, storage_unavailable,
    photo_too_large,
    internal, not_implemented
};
struct Error { ErrorCode code; std::string message; };
template<class T> using Result = std::variant<T, Error>;
enum class Freshness { off, no_data, fresh, delayed, stale };

struct Coordinate { double latitude{}; double longitude{}; };
struct User {
    Id id; TimeMs created_at_ms{};
    std::string animal_icon{"bear"};
    TimeMs expires_at_ms{};
};
struct Room {
    Id id; std::string title; Id host_id; std::string invite_code;
    bool active{true}; TimeMs created_at_ms{};
    std::optional<TimeMs> ended_at_ms;
};
struct Member {
    Id id; Id room_id; Id user_id; std::string nickname;
    bool is_sharing{false}; Epoch sharing_epoch{}; TimeMs joined_at_ms{};
    std::optional<TimeMs> left_at_ms;
    std::string animal_icon{"bear"};
};
struct DeviceStatus {
    std::optional<int> battery_percent;
    std::optional<bool> is_charging;
    TimeMs updated_at_ms{};
    Epoch sharing_epoch{};
};
struct DeviceStatusDraft {
    std::optional<int> battery_percent;
    std::optional<bool> is_charging;
    Epoch sharing_epoch{};
};
struct Location {
    Coordinate coordinate;
    std::optional<double> accuracy_m;
    TimeMs captured_at_ms{}; TimeMs received_at_ms{};
    Epoch sharing_epoch{}; std::int64_t sequence{};
};
struct LocationUpdate {
    Coordinate coordinate;
    std::optional<double> accuracy_m;
    TimeMs captured_at_ms{}; Epoch sharing_epoch{};
    std::int64_t sequence{};
};
struct PhotoRef { std::string key; TimeMs uploaded_at_ms{}; };
struct Note {
    Id id; std::string text; TimeMs written_at_ms{};
    std::optional<Coordinate> origin;
    std::optional<TimeMs> origin_captured_at_ms;
    Epoch sharing_epoch{}; std::optional<PhotoRef> photo;
};
struct NoteDraft { std::string text; Epoch sharing_epoch{}; };
struct MemberView {
    Member member;
    Freshness location_status{Freshness::no_data};
    std::optional<Location> location;
    std::optional<Note> note;
    std::optional<DeviceStatus> device_status;
};
struct MapMark {
    Id id; Id room_id; Id creator_id; std::string name;
    Coordinate coordinate;
    TimeMs created_at_ms{}; TimeMs updated_at_ms{};
};
struct MapMarkDraft { std::string name; Coordinate coordinate; };
struct RoutePoint {
    std::int64_t id{}; Id member_id;
    Location location;
};
struct RoutePage {
    std::vector<RoutePoint> points;
    std::optional<std::int64_t> next_after_id;
};
struct RoomMembership { Room room; Member member; };
struct Invite { std::string invite_code; std::string qr_payload; std::string qr_svg; };
struct TravelEstimate {
    double distance_m{}; std::int64_t walk_minutes{};
    double walking_speed_m_per_min{kWalkingSpeedMPerMin};
};

// Ian: database adapter; no HTTP, JSON, PGconn or PGresult in this interface.
// actor_id is resolved from a verified guest token, not trusted request JSON.
// All data-changing methods must enforce access/state atomically in the DB.
class IRepository {
public:
    virtual ~IRepository() = default;
    virtual Result<User> create_guest(const std::string& token_sha256,
        TimeMs now_ms, TimeMs expires_at_ms) = 0;
    virtual Result<User> authenticate_guest(const std::string& token_sha256,
        TimeMs now_ms) = 0;
    virtual Result<Room> get_room(const Id& actor_id, const Id& room_id) = 0;
    virtual Result<Room> find_room_by_code(const std::string& invite_code) = 0;
    virtual Result<RoomMembership> create_room(const Id& actor_id,
        const std::string& title, const std::string& nickname,
        const std::string& invite_code, TimeMs now_ms) = 0;
    virtual Result<Member> join_room(const Id& actor_id, const Id& room_id,
        const std::string& nickname, TimeMs now_ms) = 0;
    virtual Result<Unit> leave_room(const Id& actor_id, const Id& room_id,
        TimeMs now_ms) = 0;
    virtual Result<Unit> end_room(const Id& actor_id, const Id& room_id,
        TimeMs now_ms) = 0;
    virtual Result<std::vector<MemberView>> list_visible_members(
        const Id& actor_id, const Id& room_id, TimeMs now_ms) = 0;
    virtual Result<Member> set_sharing(const Id& actor_id, const Id& room_id,
        bool enabled, TimeMs now_ms) = 0;
    virtual Result<Location> update_location(const Id& actor_id,
        const Id& room_id, const LocationUpdate& update, TimeMs now_ms) = 0;
    virtual Result<Note> save_note(const Id& actor_id, const Id& room_id,
        const NoteDraft& draft, TimeMs now_ms) = 0;
    virtual Result<Unit> delete_note(const Id& actor_id, const Id& room_id,
        TimeMs now_ms) = 0;
    // Keys are generated by the server and must never be exposed to clients.
    // reserve -> upload -> attach atomically removes the new key from cleanup.
    virtual Result<Note> attach_photo(const Id& actor_id, const Id& room_id,
        const Id& note_id, TimeMs expected_written_at_ms,
        const PhotoRef& photo, Epoch sharing_epoch, TimeMs now_ms) = 0;
    virtual Result<Unit> reserve_photo_cleanup(const std::string& key,
        TimeMs not_before_ms) = 0;
    virtual Result<std::vector<std::string>> due_photo_cleanup(TimeMs now_ms) = 0;
    virtual Result<Unit> finish_photo_cleanup(const std::string& key) = 0;
    // Optional defaults keep third-party test adapters source-compatible.
    virtual Result<User> get_profile(const Id&) { return Error{ErrorCode::not_implemented,"profile unavailable"}; }
    virtual Result<User> update_profile(const Id&, const std::string&) { return Error{ErrorCode::not_implemented,"profile unavailable"}; }
    virtual Result<Unit> delete_user(const Id&, TimeMs) { return Error{ErrorCode::not_implemented,"deletion unavailable"}; }
    virtual Result<Unit> delete_member_data(const Id&, const Id&, bool, TimeMs) { return Error{ErrorCode::not_implemented,"deletion unavailable"}; }
    virtual Result<DeviceStatus> update_device_status(const Id&, const Id&, const DeviceStatusDraft&, TimeMs) { return Error{ErrorCode::not_implemented,"device status unavailable"}; }
    virtual Result<std::vector<MapMark>> list_marks(const Id&, const Id&) { return Error{ErrorCode::not_implemented,"marks unavailable"}; }
    virtual Result<MapMark> create_mark(const Id&, const Id&, const MapMarkDraft&, TimeMs) { return Error{ErrorCode::not_implemented,"marks unavailable"}; }
    virtual Result<MapMark> rename_mark(const Id&, const Id&, const Id&, const std::string&, TimeMs) { return Error{ErrorCode::not_implemented,"marks unavailable"}; }
    virtual Result<Unit> delete_mark(const Id&, const Id&, const Id&) { return Error{ErrorCode::not_implemented,"marks unavailable"}; }
    virtual Result<RoutePage> get_route(const Id&, const Id&, const Id&, std::int64_t, int, TimeMs) { return Error{ErrorCode::not_implemented,"route unavailable"}; }
    virtual Result<Unit> collect_expired_data(TimeMs) { return Unit{}; }
};

// Seoyeong: room rules + invite/QR; uses IRepository, never writes SQL here.
class IRoomService {
public:
    virtual ~IRoomService() = default;
    virtual Result<RoomMembership> create_room(const Id& actor_id,
        const std::string& title, const std::string& nickname,
        TimeMs now_ms) = 0;
    virtual Result<RoomMembership> join_room(const Id& actor_id,
        const std::string& invite_code, const std::string& nickname,
        TimeMs now_ms) = 0;
    virtual Result<Unit> leave_room(const Id& actor_id, const Id& room_id,
        TimeMs now_ms) = 0;
    virtual Result<Unit> end_room(const Id& actor_id, const Id& room_id,
        TimeMs now_ms) = 0;
    virtual Result<Invite> get_invite(const Id& actor_id, const Id& room_id) = 0;
};

// Composition entry points. Own repository before creating room service.
struct RepositoryConfig { std::string connection_string; };
Result<std::unique_ptr<IRepository>> make_postgres_repository(
    const RepositoryConfig& config);                       // Ian
std::unique_ptr<IRepository> make_fake_repository();       // Wongyu
std::unique_ptr<IRoomService> make_room_service(
    IRepository& repository);                            // Seoyeong

// Juwon: pure calculations; no DB, HTTP, Swift or external library required.
Result<TravelEstimate> estimate_walk(Coordinate from, Coordinate to,
    double walking_speed_m_per_min = kWalkingSpeedMPerMin);
Freshness classify_location(const std::optional<Location>& location,
    bool is_sharing, Epoch current_epoch, TimeMs now_ms);
} // namespace gommanna
