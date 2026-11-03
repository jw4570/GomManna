#include "gommanna/contracts.hpp"

#include <libpq-fe.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gommanna {
namespace {

using PgConnection = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using PgResult = std::unique_ptr<PGresult, decltype(&PQclear)>;
using Param = std::optional<std::string>;

struct DatabaseFailure {
    std::string state;
    std::string constraint;
    bool disconnected{};
};
struct DomainFailure { Error error; };

[[noreturn]] void reject(ErrorCode code, const char* message) {
    throw DomainFailure{Error{code, message}};
}

bool has_nul(const std::string& text) { return text.find('\0') != std::string::npos; }
void require_text(const std::string& text) {
    if (has_nul(text)) reject(ErrorCode::invalid_argument, "invalid text");
}
void require_coordinate(Coordinate c) {
    if (!std::isfinite(c.latitude) || !std::isfinite(c.longitude) ||
        c.latitude < -90.0 || c.latitude > 90.0 ||
        c.longitude < -180.0 || c.longitude > 180.0)
        reject(ErrorCode::invalid_argument, "invalid coordinate");
}
void require_now(TimeMs now_ms) {
    // Supported through year 9999; prevents invalid timestamp arithmetic.
    if (now_ms < 0 || now_ms > 253402300799999LL)
        reject(ErrorCode::invalid_argument, "invalid time");
}

PgResult exec(PGconn* conn, const std::string& sql, const std::vector<Param>& params = {}) {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params) values.push_back(p ? p->c_str() : nullptr);
    PgResult result(PQexecParams(conn, sql.c_str(), static_cast<int>(values.size()),
                                nullptr, values.data(), nullptr, nullptr, 0), &PQclear);
    if (!result || (PQresultStatus(result.get()) != PGRES_COMMAND_OK &&
                    PQresultStatus(result.get()) != PGRES_TUPLES_OK)) {
        const char* state = result ? PQresultErrorField(result.get(), PG_DIAG_SQLSTATE) : nullptr;
        const char* constraint = result ? PQresultErrorField(result.get(), PG_DIAG_CONSTRAINT_NAME) : nullptr;
        throw DatabaseFailure{state ? state : "", constraint ? constraint : "",
                              PQstatus(conn) != CONNECTION_OK};
    }
    return result;
}

class Transaction {
public:
    explicit Transaction(PGconn* conn) : conn_(conn) { exec(conn_, "begin"); }
    ~Transaction() {
        if (!finished_) {
            PgResult result(PQexec(conn_, "rollback"), &PQclear);
        }
    }
    void commit() { exec(conn_, "commit"); finished_ = true; }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
private:
    PGconn* conn_;
    bool finished_ = false;
};

int col(PGresult* r, const char* name) {
    const int index = PQfnumber(r, name);
    if (index < 0) throw std::runtime_error("missing database result column");
    return index;
}
bool is_null(PGresult* r, int row, const char* name) { return PQgetisnull(r, row, col(r, name)) != 0; }
std::string string_at(PGresult* r, int row, const char* name) {
    if (is_null(r, row, name)) throw std::runtime_error("unexpected database null");
    return PQgetvalue(r, row, col(r, name));
}
std::int64_t integer_at(PGresult* r, int row, const char* name) { return std::stoll(string_at(r, row, name)); }
double double_at(PGresult* r, int row, const char* name) { return std::stod(string_at(r, row, name)); }
bool bool_at(PGresult* r, int row, const char* name) { return string_at(r, row, name) == "t"; }
std::string number(double value) {
    std::array<char,128> buffer{};
    auto [end, ec]=std::to_chars(buffer.data(),buffer.data()+buffer.size(),value,
                                 std::chars_format::general,std::numeric_limits<double>::max_digits10);
    if(ec!=std::errc{}) throw std::runtime_error("number conversion failed");
    return std::string(buffer.data(),end);
}

const std::string kRoomColumns =
    "r.id::text r_id, r.title r_title, r.host_id::text r_host, r.invite_code r_code, "
    "r.status r_status, round(extract(epoch from r.created_at)*1000)::bigint r_created, "
    "round(extract(epoch from r.ended_at)*1000)::bigint r_ended ";
const std::string kMemberColumns =
    "m.id::text m_id, m.room_id::text m_room, m.user_id::text m_user, m.nickname m_name, "
    "m.is_sharing m_sharing, m.sharing_epoch m_epoch, "
    "round(extract(epoch from m.joined_at)*1000)::bigint m_joined ";
const std::string kLocationColumns =
    "l.latitude l_lat, l.longitude l_lon, l.accuracy_m l_accuracy, "
    "round(extract(epoch from l.captured_at)*1000)::bigint l_captured, "
    "round(extract(epoch from l.received_at)*1000)::bigint l_received, "
    "l.sharing_epoch l_epoch, l.sequence l_sequence ";
const std::string kNoteColumns =
    "n.id::text n_id, n.text n_text, round(extract(epoch from n.written_at)*1000)::bigint n_written, "
    "n.origin_latitude n_lat, n.origin_longitude n_lon, "
    "round(extract(epoch from n.origin_captured_at)*1000)::bigint n_captured, "
    "n.sharing_epoch n_epoch, n.photo_key n_photo_key, "
    "round(extract(epoch from n.photo_uploaded_at)*1000)::bigint n_photo_uploaded ";

Room room_at(PGresult* r, int row) {
    Room x;
    x.id = string_at(r,row,"r_id"); x.title = string_at(r,row,"r_title");
    x.host_id = string_at(r,row,"r_host"); x.invite_code = string_at(r,row,"r_code");
    x.active = string_at(r,row,"r_status") == "active";
    x.created_at_ms = integer_at(r,row,"r_created");
    if (!is_null(r,row,"r_ended")) x.ended_at_ms = integer_at(r,row,"r_ended");
    return x;
}
Member member_at(PGresult* r, int row) {
    Member x;
    x.id=string_at(r,row,"m_id"); x.room_id=string_at(r,row,"m_room");
    x.user_id=string_at(r,row,"m_user"); x.nickname=string_at(r,row,"m_name");
    x.is_sharing=bool_at(r,row,"m_sharing"); x.sharing_epoch=integer_at(r,row,"m_epoch");
    x.joined_at_ms=integer_at(r,row,"m_joined");
    return x;
}
Location location_at(PGresult* r, int row) {
    Location x;
    x.coordinate={double_at(r,row,"l_lat"),double_at(r,row,"l_lon")};
    if (!is_null(r,row,"l_accuracy")) x.accuracy_m=double_at(r,row,"l_accuracy");
    x.captured_at_ms=integer_at(r,row,"l_captured"); x.received_at_ms=integer_at(r,row,"l_received");
    x.sharing_epoch=integer_at(r,row,"l_epoch"); x.sequence=integer_at(r,row,"l_sequence");
    return x;
}
Note note_at(PGresult* r, int row) {
    Note x;
    x.id=string_at(r,row,"n_id"); x.text=string_at(r,row,"n_text");
    x.written_at_ms=integer_at(r,row,"n_written"); x.sharing_epoch=integer_at(r,row,"n_epoch");
    if (!is_null(r,row,"n_lat")) {
        x.origin=Coordinate{double_at(r,row,"n_lat"),double_at(r,row,"n_lon")};
        x.origin_captured_at_ms=integer_at(r,row,"n_captured");
    }
    if (!is_null(r,row,"n_photo_key"))
        x.photo=PhotoRef{string_at(r,row,"n_photo_key"),integer_at(r,row,"n_photo_uploaded")};
    return x;
}

Freshness freshness(const std::optional<Location>& loc, bool sharing, Epoch epoch, TimeMs now_ms) {
    if (!sharing) return Freshness::off;
    if (!loc || loc->sharing_epoch != epoch || loc->captured_at_ms < 0 ||
        static_cast<long double>(loc->captured_at_ms) > static_cast<long double>(now_ms) + kFutureToleranceMs ||
        !std::isfinite(loc->coordinate.latitude) || !std::isfinite(loc->coordinate.longitude) ||
        std::abs(loc->coordinate.latitude) > 90.0 || std::abs(loc->coordinate.longitude) > 180.0)
        return Freshness::no_data;
    const auto age = std::max<TimeMs>(0, now_ms - loc->captured_at_ms);
    if (age < kDelayedAfterMs) return Freshness::fresh;
    if (age < kStaleAfterMs) return Freshness::delayed;
    return Freshness::stale;
}

class PostgresRepository final : public IRepository {
public:
    explicit PostgresRepository(PgConnection conn) : conn_(std::move(conn)) {}
    Result<User> create_guest(const std::string&, TimeMs, TimeMs) override;
    Result<User> authenticate_guest(const std::string&, TimeMs) override;
    Result<Room> get_room(const Id&, const Id&) override;
    Result<Room> find_room_by_code(const std::string&) override;
    Result<RoomMembership> create_room(const Id&, const std::string&, const std::string&, const std::string&, TimeMs) override;
    Result<Member> join_room(const Id&, const Id&, const std::string&, TimeMs) override;
    Result<Unit> leave_room(const Id&, const Id&, TimeMs) override;
    Result<Unit> end_room(const Id&, const Id&, TimeMs) override;
    Result<std::vector<MemberView>> list_visible_members(const Id&, const Id&, TimeMs) override;
    Result<Member> set_sharing(const Id&, const Id&, bool, TimeMs) override;
    Result<Location> update_location(const Id&, const Id&, const LocationUpdate&, TimeMs) override;
    Result<Note> save_note(const Id&, const Id&, const NoteDraft&, TimeMs) override;
    Result<Unit> delete_note(const Id&, const Id&, TimeMs) override;
    Result<Note> attach_photo(const Id&, const Id&, const Id&, TimeMs, const PhotoRef&, Epoch, TimeMs) override {
        return Error{ErrorCode::not_implemented,"photo attachment is not implemented"};
    }
private:
    template<class T, class F> Result<T> guarded(F&& function) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (PQstatus(conn_.get()) != CONNECTION_OK)
                return Error{ErrorCode::db_unavailable,"database unavailable"};
            return function();
        } catch (const DomainFailure& e) {
            return e.error;
        } catch (const DatabaseFailure& e) {
            if (e.disconnected || e.state.rfind("08",0)==0 || e.state.empty())
                return Error{ErrorCode::db_unavailable,"database unavailable"};
            if (e.state=="23505") {
                if (e.constraint=="rooms_invite_code_key")
                    return Error{ErrorCode::invite_code_conflict,"invite code already exists"};
                return Error{ErrorCode::conflict,"data already exists"};
            }
            if (e.state.rfind("22",0)==0 || e.state=="23514" || e.state=="23502")
                return Error{ErrorCode::invalid_argument,"invalid input"};
            if (e.state=="23503") return Error{ErrorCode::conflict,"related data missing"};
            return Error{ErrorCode::internal,"database operation failed"};
        } catch (const std::exception&) {
            return Error{ErrorCode::internal,"database operation failed"};
        }
    }
    Room locked_room(const Id& room_id, bool allow_ended = false) {
        require_text(room_id);
        auto r=exec(conn_.get(),"select "+kRoomColumns+"from gommanna.rooms r where r.id=$1::uuid for update",{room_id});
        if (PQntuples(r.get())==0) reject(ErrorCode::not_found,"room not found");
        auto room=room_at(r.get(),0);
        if (!room.active && !allow_ended) reject(ErrorCode::room_closed,"room is closed");
        return room;
    }
    Member locked_member(const Id& actor, const Id& room) {
        require_text(actor);
        auto r=exec(conn_.get(),"select "+kMemberColumns+
            "from gommanna.room_members m where m.room_id=$1::uuid and m.user_id=$2::uuid for update",{room,actor});
        if (PQntuples(r.get())==0) reject(ErrorCode::forbidden,"not a room member");
        return member_at(r.get(),0);
    }
    void require_user(const Id& user) {
        require_text(user);
        auto r=exec(conn_.get(),"select 1 from gommanna.users where id=$1::uuid",{user});
        if (PQntuples(r.get())==0) reject(ErrorCode::unauthenticated,"guest not found");
    }
    PgConnection conn_;
    std::mutex mutex_;
};

bool valid_token_hash(const std::string& hash) {
    return hash.size()==64 && std::all_of(hash.begin(),hash.end(),[](char c) {
        return (c>='0' && c<='9') || (c>='a' && c<='f');
    });
}

Result<User> PostgresRepository::create_guest(const std::string& hash, TimeMs now, TimeMs expires) {
    return guarded<User>([&] {
        require_now(now); require_now(expires);
        if (!valid_token_hash(hash) || expires<=now) reject(ErrorCode::invalid_argument,"invalid guest session");
        Transaction tx(conn_.get());
        auto user=exec(conn_.get(),
            "insert into gommanna.users(created_at) values(to_timestamp($1::double precision/1000)) "
            "returning id::text user_id,round(extract(epoch from created_at)*1000)::bigint created_ms",{std::to_string(now)});
        User result{string_at(user.get(),0,"user_id"),integer_at(user.get(),0,"created_ms")};
        exec(conn_.get(),"insert into gommanna.guest_sessions(user_id,token_sha256,created_at,expires_at) "
            "values($1::uuid,$2,to_timestamp($3::double precision/1000),to_timestamp($4::double precision/1000))",
            {result.id,hash,std::to_string(now),std::to_string(expires)});
        tx.commit();
        return result;
    });
}

Result<User> PostgresRepository::authenticate_guest(const std::string& hash, TimeMs now) {
    return guarded<User>([&] {
        require_now(now);
        if (!valid_token_hash(hash)) reject(ErrorCode::unauthenticated,"invalid guest session");
        auto r=exec(conn_.get(),"select u.id::text user_id,round(extract(epoch from u.created_at)*1000)::bigint created_ms "
            "from gommanna.guest_sessions s join gommanna.users u on u.id=s.user_id "
            "where s.token_sha256=$1 and s.revoked_at is null and s.expires_at>to_timestamp($2::double precision/1000)",
            {hash,std::to_string(now)});
        if (PQntuples(r.get())==0) reject(ErrorCode::unauthenticated,"invalid guest session");
        return User{string_at(r.get(),0,"user_id"),integer_at(r.get(),0,"created_ms")};
    });
}

Result<Room> PostgresRepository::get_room(const Id& actor, const Id& room_id) {
    return guarded<Room>([&] {
        Transaction tx(conn_.get());
        Room room=locked_room(room_id);
        locked_member(actor,room_id);
        tx.commit();
        return room;
    });
}

Result<Room> PostgresRepository::find_room_by_code(const std::string& code) {
    return guarded<Room>([&] {
        require_text(code);
        auto r=exec(conn_.get(),"select "+kRoomColumns+"from gommanna.rooms r where r.invite_code=$1",{code});
        if(PQntuples(r.get())==0) reject(ErrorCode::not_found,"invite code not found");
        auto room=room_at(r.get(),0);
        if(!room.active) reject(ErrorCode::room_closed,"room is closed");
        return room;
    });
}

Result<RoomMembership> PostgresRepository::create_room(const Id& actor, const std::string& title,
                                                        const std::string& name, const std::string& code, TimeMs now) {
    return guarded<RoomMembership>([&] {
        require_now(now); require_text(title); require_text(name); require_text(code);
        Transaction tx(conn_.get());
        require_user(actor);
        auto r=exec(conn_.get(),"with added as (insert into gommanna.rooms(title,host_id,invite_code,created_at) "
            "values(btrim($1),$2::uuid,$3,to_timestamp($4::double precision/1000)) returning *) "
            "select "+kRoomColumns+"from added r",{title,actor,code,std::to_string(now)});
        Room room=room_at(r.get(),0);
        auto m=exec(conn_.get(),"with added as (insert into gommanna.room_members(room_id,user_id,nickname,joined_at) "
            "values($1::uuid,$2::uuid,btrim($3),to_timestamp($4::double precision/1000)) returning *) "
            "select "+kMemberColumns+"from added m",{room.id,actor,name,std::to_string(now)});
        RoomMembership result{room,member_at(m.get(),0)};
        tx.commit();
        return result;
    });
}

Result<Member> PostgresRepository::join_room(const Id& actor, const Id& room_id, const std::string& name, TimeMs now) {
    return guarded<Member>([&] {
        require_now(now); require_text(name);
        Transaction tx(conn_.get());
        locked_room(room_id);
        require_user(actor);
        auto existing=exec(conn_.get(),"select "+kMemberColumns+
            "from gommanna.room_members m where m.room_id=$1::uuid and m.user_id=$2::uuid",{room_id,actor});
        if(PQntuples(existing.get())!=0) {
            Member result=member_at(existing.get(),0);
            tx.commit();
            return result;
        }
        auto count=exec(conn_.get(),"select count(*)::text count from gommanna.room_members where room_id=$1::uuid",{room_id});
        if(integer_at(count.get(),0,"count")>=kMaxRoomMembers) reject(ErrorCode::conflict,"room is full");
        auto r=exec(conn_.get(),"with added as (insert into gommanna.room_members(room_id,user_id,nickname,joined_at) "
            "values($1::uuid,$2::uuid,btrim($3),to_timestamp($4::double precision/1000)) returning *) "
            "select "+kMemberColumns+"from added m",{room_id,actor,name,std::to_string(now)});
        Member result=member_at(r.get(),0);
        tx.commit();
        return result;
    });
}

Result<Unit> PostgresRepository::leave_room(const Id& actor, const Id& room_id, TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        Room room=locked_room(room_id);
        Member member=locked_member(actor,room_id);
        if(room.host_id==actor) reject(ErrorCode::forbidden,"host must end the room");
        exec(conn_.get(),"delete from gommanna.room_members where id=$1::uuid",{member.id});
        tx.commit();
        return Unit{};
    });
}

Result<Unit> PostgresRepository::end_room(const Id& actor, const Id& room_id, TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now); require_text(actor);
        Transaction tx(conn_.get());
        Room room=locked_room(room_id,true);
        if(room.host_id!=actor) reject(ErrorCode::forbidden,"only the host can end the room");
        if(room.active) {
            if(now<room.created_at_ms) reject(ErrorCode::invalid_argument,"invalid time");
            exec(conn_.get(),"update gommanna.rooms set status='ended',ended_at=to_timestamp($2::double precision/1000) "
                "where id=$1::uuid",{room_id,std::to_string(now)});
            exec(conn_.get(),"delete from gommanna.room_members where room_id=$1::uuid",{room_id});
        }
        tx.commit();
        return Unit{};
    });
}

Result<std::vector<MemberView>> PostgresRepository::list_visible_members(const Id& actor, const Id& room_id, TimeMs now) {
    return guarded<std::vector<MemberView>>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        locked_room(room_id);
        locked_member(actor,room_id);
        auto r=exec(conn_.get(),"select "+kMemberColumns+", "+kLocationColumns+", "+kNoteColumns+
            "from gommanna.room_members m "
            "left join gommanna.member_locations l on l.member_id=m.id and l.room_id=m.room_id "
                "and m.is_sharing and l.sharing_epoch=m.sharing_epoch "
            "left join gommanna.member_notes n on n.member_id=m.id and n.room_id=m.room_id "
                "and m.is_sharing and n.sharing_epoch=m.sharing_epoch "
            "where m.room_id=$1::uuid order by m.joined_at,m.id",{room_id});
        std::vector<MemberView> members;
        for(int i=0;i<PQntuples(r.get());++i) {
            MemberView view;
            view.member=member_at(r.get(),i);
            if(!is_null(r.get(),i,"l_lat")) view.location=location_at(r.get(),i);
            if(!is_null(r.get(),i,"n_id")) view.note=note_at(r.get(),i);
            view.location_status=freshness(view.location,view.member.is_sharing,view.member.sharing_epoch,now);
            if(view.location_status==Freshness::off || view.location_status==Freshness::no_data)
                view.location.reset();
            // SQL filters notes/photos before they leave the database.
            if(!view.member.is_sharing) view.note.reset();
            members.push_back(std::move(view));
        }
        tx.commit();
        return members;
    });
}

Result<Member> PostgresRepository::set_sharing(const Id& actor, const Id& room_id, bool enabled, TimeMs now) {
    return guarded<Member>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        locked_room(room_id);
        Member member=locked_member(actor,room_id);
        auto r=exec(conn_.get(),"with changed as (update gommanna.room_members set "
            "sharing_epoch=case when not is_sharing and $2::boolean then sharing_epoch+1 else sharing_epoch end, "
            "is_sharing=$2::boolean where id=$1::uuid returning *) select "+kMemberColumns+"from changed m",
            {member.id,enabled?"true":"false"});
        Member result=member_at(r.get(),0);
        tx.commit();
        return result;
    });
}

Result<Location> PostgresRepository::update_location(const Id& actor, const Id& room_id,
                                                      const LocationUpdate& update, TimeMs now) {
    return guarded<Location>([&] {
        require_now(now); require_now(update.captured_at_ms); require_coordinate(update.coordinate);
        if(update.accuracy_m && (!std::isfinite(*update.accuracy_m) || *update.accuracy_m<0))
            reject(ErrorCode::invalid_argument,"invalid accuracy");
        if(update.sequence<1 || update.sharing_epoch<1 ||
           static_cast<long double>(update.captured_at_ms)>static_cast<long double>(now)+kFutureToleranceMs)
            reject(ErrorCode::invalid_argument,"invalid location update");
        Transaction tx(conn_.get());
        locked_room(room_id);
        Member member=locked_member(actor,room_id);
        if(!member.is_sharing) reject(ErrorCode::sharing_off,"sharing is off");
        if(member.sharing_epoch!=update.sharing_epoch) reject(ErrorCode::stale_update,"outdated sharing session");
        auto previous=exec(conn_.get(),"select "+kLocationColumns+
            "from gommanna.member_locations l where l.member_id=$1::uuid for update",{member.id});
        if(PQntuples(previous.get())>0) {
            Location old=location_at(previous.get(),0);
            if(old.sharing_epoch==update.sharing_epoch &&
               (update.sequence<=old.sequence || update.captured_at_ms<old.captured_at_ms))
                reject(ErrorCode::stale_update,"outdated location update");
        }
        Param accuracy=update.accuracy_m ? Param{number(*update.accuracy_m)} : std::nullopt;
        auto r=exec(conn_.get(),"with stored as (insert into gommanna.member_locations "
            "(member_id,room_id,latitude,longitude,accuracy_m,captured_at,received_at,sharing_epoch,sequence) "
            "values($1::uuid,$2::uuid,$3::float8,$4::float8,$5::float8,"
              "to_timestamp($6::double precision/1000),to_timestamp($7::double precision/1000),$8::bigint,$9::bigint) "
            "on conflict(member_id) do update set latitude=excluded.latitude,longitude=excluded.longitude,"
              "accuracy_m=excluded.accuracy_m,captured_at=excluded.captured_at,received_at=excluded.received_at,"
              "sharing_epoch=excluded.sharing_epoch,sequence=excluded.sequence returning *) "
            "select "+kLocationColumns+"from stored l",
            {member.id,room_id,number(update.coordinate.latitude),number(update.coordinate.longitude),accuracy,
             std::to_string(update.captured_at_ms),std::to_string(now),std::to_string(update.sharing_epoch),
             std::to_string(update.sequence)});
        Location result=location_at(r.get(),0);
        tx.commit();
        return result;
    });
}

Result<Note> PostgresRepository::save_note(const Id& actor, const Id& room_id, const NoteDraft& draft, TimeMs now) {
    return guarded<Note>([&] {
        require_now(now); require_text(draft.text);
        if(draft.sharing_epoch<1) reject(ErrorCode::invalid_argument,"invalid sharing session");
        Transaction tx(conn_.get());
        locked_room(room_id);
        Member member=locked_member(actor,room_id);
        if(!member.is_sharing) reject(ErrorCode::sharing_off,"sharing is off");
        if(member.sharing_epoch!=draft.sharing_epoch) reject(ErrorCode::stale_update,"outdated sharing session");
        auto previous=exec(conn_.get(),"select "+kLocationColumns+
            "from gommanna.member_locations l where l.member_id=$1::uuid",{member.id});
        Param latitude,longitude,captured;
        if(PQntuples(previous.get())>0) {
            Location loc=location_at(previous.get(),0);
            if(freshness(loc,true,member.sharing_epoch,now)==Freshness::fresh) {
                latitude=number(loc.coordinate.latitude); longitude=number(loc.coordinate.longitude);
                captured=std::to_string(loc.captured_at_ms);
            }
        }
        auto r=exec(conn_.get(),"with stored as (insert into gommanna.member_notes "
            "(member_id,room_id,text,written_at,origin_latitude,origin_longitude,origin_captured_at,sharing_epoch) "
            "values($1::uuid,$2::uuid,btrim($3),to_timestamp($4::double precision/1000),$5::float8,$6::float8,"
              "to_timestamp($7::double precision/1000),$8::bigint) "
            "on conflict(member_id) do update set text=excluded.text,written_at=excluded.written_at,"
              "origin_latitude=excluded.origin_latitude,origin_longitude=excluded.origin_longitude,"
              "origin_captured_at=excluded.origin_captured_at,sharing_epoch=excluded.sharing_epoch,"
              "photo_key=null,photo_uploaded_at=null returning *) select "+kNoteColumns+"from stored n",
            {member.id,room_id,draft.text,std::to_string(now),latitude,longitude,captured,std::to_string(draft.sharing_epoch)});
        Note result=note_at(r.get(),0);
        tx.commit();
        return result;
    });
}

Result<Unit> PostgresRepository::delete_note(const Id& actor, const Id& room_id, TimeMs now) {
    return guarded<Unit>([&] {
        require_now(now);
        Transaction tx(conn_.get());
        locked_room(room_id);
        Member member=locked_member(actor,room_id);
        if(!member.is_sharing) reject(ErrorCode::sharing_off,"sharing is off");
        exec(conn_.get(),"delete from gommanna.member_notes where member_id=$1::uuid",{member.id});
        tx.commit();
        return Unit{};
    });
}

} // namespace

Result<std::unique_ptr<IRepository>> make_postgres_repository(const RepositoryConfig& config) {
    if(config.connection_string.empty() || has_nul(config.connection_string))
        return Error{ErrorCode::invalid_argument,"database configuration is missing"};
    const char* keywords[]={"dbname","sslmode","connect_timeout",nullptr};
    const char* values[]={config.connection_string.c_str(),"verify-full","10",nullptr};
    PgConnection connection(PQconnectdbParams(keywords,values,1),&PQfinish);
    if(!connection) return Error{ErrorCode::db_unavailable,"database connection failed"};
    if(PQstatus(connection.get())!=CONNECTION_OK) {
        // Categorize locally for troubleshooting without exposing connection strings,
        // credentials, raw server messages, or SQL in API responses or logs.
        const std::string diagnostic=PQerrorMessage(connection.get());
        if(diagnostic.find("certificate")!=std::string::npos || diagnostic.find("SSL")!=std::string::npos)
            return Error{ErrorCode::db_unavailable,"database TLS verification failed"};
        if(diagnostic.find("password authentication failed")!=std::string::npos)
            return Error{ErrorCode::db_unavailable,"database authentication failed"};
        return Error{ErrorCode::db_unavailable,"database connection failed"};
    }
    if(!PQsslInUse(connection.get()))
        return Error{ErrorCode::db_unavailable,"database TLS is required"};
    try {
        exec(connection.get(),"set role gommanna_server");
        exec(connection.get(),"set client_encoding to 'UTF8'");
        exec(connection.get(),"set time zone 'UTC'");
        std::unique_ptr<IRepository> repository=std::make_unique<PostgresRepository>(std::move(connection));
        return repository;
    } catch(...) {
        return Error{ErrorCode::db_unavailable,"database setup failed"};
    }
}
} // namespace gommanna
