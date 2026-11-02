#include "gommanna/app.hpp"
#include <algorithm>
#include <iostream>
#include <set>

namespace gm {
namespace {
Json body(const httplib::Request& request) {
    if (request.get_header_value("Content-Type").find("application/json") != 0)
        throw ApiError(415, "json_required", "Use Content-Type: application/json");
    auto value = Json::parse(request.body, nullptr, false);
    if (value.is_discarded() || !value.is_object()) throw ApiError(400, "invalid_json", "Expected a JSON object");
    return value;
}
void fields(const Json& body, std::initializer_list<const char*> allowed) {
    std::set<std::string> keys(allowed.begin(), allowed.end());
    for (auto it = body.begin(); it != body.end(); ++it)
        if (!keys.contains(it.key())) throw ApiError(400, "invalid_input", "Unexpected field: " + it.key());
}
struct Current { std::string room, member, host; };
std::optional<Current> current(Db& db, const std::string& user, bool required = true) {
    auto membership = db.query("SELECT room_id FROM public.room_members WHERE user_id=$1::uuid", {user});
    if (!membership.empty()) {
        auto room = db.query("SELECT id,host_id FROM public.rooms WHERE id=$1::uuid AND expires_at>now() FOR UPDATE",
                             {membership[0]["room_id"].get<std::string>()});
        if (!room.empty()) {
            auto m = db.query("SELECT id FROM public.room_members WHERE user_id=$1::uuid AND room_id=$2::uuid",
                              {user, room[0]["id"].get<std::string>()});
            if (!m.empty()) return Current{room[0]["id"], m[0]["id"], room[0]["host_id"]};
        }
    }
    if (required) throw ApiError(404, "no_active_room", "No active room");
    return std::nullopt;
}
void clear_expired(Db& db, const std::string& user) {
    db.query("DELETE FROM public.rooms WHERE expires_at<=now() AND id IN "
             "(SELECT room_id FROM public.room_members WHERE user_id=$1::uuid)", {user});
}
Json snapshot(Db& db, const Current& c) {
    auto room = db.query("SELECT id,title,host_id,invite_code,created_at,expires_at FROM public.rooms WHERE id=$1::uuid", {c.room})[0];
    room["my_member_id"] = c.member;
    room["members"] = db.query(R"SQL(
        SELECT m.id,m.user_id,m.nickname,m.transport_mode,m.is_sharing,m.joined_at,u.avatar,
               (m.user_id=r.host_id) AS is_host,l.latitude,l.longitude,l.accuracy_m,l.recorded_at,l.received_at
        FROM public.room_members m JOIN public.users u ON u.id=m.user_id
        JOIN public.rooms r ON r.id=m.room_id
        LEFT JOIN public.member_locations l ON l.member_id=m.id AND m.is_sharing
        WHERE m.room_id=$1::uuid ORDER BY m.joined_at,m.id
    )SQL", {c.room});
    room["marks"] = db.query("SELECT id,created_by,name,latitude,longitude,created_at FROM public.map_marks "
                              "WHERE room_id=$1::uuid ORDER BY created_at,id", {c.room});
    return {{"room", room}};
}
void require_new_room(Db& db, const std::string& user) {
    clear_expired(db, user);
    if (!db.query("SELECT id FROM public.room_members WHERE user_id=$1::uuid", {user}).empty())
        throw ApiError(409, "already_in_room", "Leave the current room before joining another");
}
Json mark_coordinates(const Json& b) {
    return {{"latitude", number(b,"latitude",-90,90)}, {"longitude", number(b,"longitude",-180,180)}};
}
}
App::App(Config config) : config_(std::move(config)), auth_(config_), pool_(config_.database_url, config_.pool_size) {
    http_.new_task_queue = [] { return new httplib::ThreadPool(8); };
    http_.set_payload_max_length(16 * 1024);
    http_.set_read_timeout(10, 0);
    http_.set_write_timeout(10, 0);
    http_.set_keep_alive_max_count(100);
    http_.set_default_headers({{"Cache-Control", "no-store"}, {"X-Content-Type-Options", "nosniff"}});
    http_.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.body.empty()) response.set_content(
            Json{{"error", {{"code", "http_error"}, {"message", "Request could not be processed"}}}}.dump(), "application/json; charset=utf-8");
    });
    http_.set_exception_handler([](const httplib::Request&, httplib::Response& response, std::exception_ptr) {
        response.status = 500;
        response.set_content(Json{{"error", {{"code", "internal_error"}, {"message", "Internal server error"}}}}.dump(), "application/json; charset=utf-8");
    });
    routes();
}
void App::respond(const httplib::Request& req, httplib::Response& res, const Action& action, int status, bool authenticated) {
    auto fail = [&](int code, const std::string& key, const std::string& message) {
        res.status = code;
        res.set_content(Json{{"error", {{"code", key}, {"message", message}}}}.dump(), "application/json; charset=utf-8");
        if (code == 401) res.set_header("WWW-Authenticate", "Bearer");
        if (code == 429) res.set_header("Retry-After", "60");
    };
    try {
        limits_.check("request:" + req.remote_addr, 600, std::chrono::minutes(1));
        std::string user;
        if (authenticated) {
            auto header = req.get_header_value("Authorization");
            if (!header.starts_with("Bearer ")) throw ApiError(401, "unauthorized", "Bearer token required");
            user = auth_.verify(header.substr(7));
            limits_.check("api:" + user, 600, std::chrono::minutes(1));
        }
        auto lease = pool_.acquire();
        Db& db = *lease;
        Transaction tx(db);
        if (authenticated && db.query("SELECT id FROM public.users WHERE id=$1::uuid FOR UPDATE", {user}).empty())
            throw ApiError(401, "unauthorized", "User no longer exists");
        auto payload = action(db, user);
        tx.commit();
        res.status = status;
        res.set_content(payload.dump(), "application/json; charset=utf-8");
    } catch (const ApiError& e) { fail(e.status, e.code, e.what()); }
    catch (const DbError& e) {
        std::cerr << "Database error SQLSTATE=" << e.state << '\n';
        if (e.state == "23505") fail(409, "conflict", "A conflicting record already exists");
        else if (e.state == "23503") fail(409, "conflict", "The related record is no longer available");
        else if (e.state == "23514" || e.state == "23502" || e.state.starts_with("22"))
            fail(400, "invalid_input", "Invalid field value");
        else if (e.state == "40P01" || e.state == "40001" || e.state == "55P03" || e.state == "57014")
            fail(503, "retry_request", "Please retry the request");
        else fail(500, "database_error", "Database request failed");
    } catch (...) { fail(500, "internal_error", "Internal server error"); }
}
void App::routes() {
    http_.Get("/health", [](const auto&, auto& res) {
        res.set_content(Json{{"status", "ok"}}.dump(), "application/json");
    });
    http_.Get("/ready", [this](const auto& req, auto& res) {
        respond(req, res, [](Db& db, const std::string&) -> Json {
            auto tables = db.query("SELECT count(*)::int AS count FROM information_schema.tables "
                "WHERE table_schema='public' AND table_name IN "
                "('users','rooms','room_members','member_locations','messages','map_marks')");
            if (tables[0]["count"] != 6) throw ApiError(503, "schema_missing", "Apply the database migration first");
            return {{"status", "ready"}};
        }, 200, false);
    });
    http_.Post("/v1/auth/guest", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string&) -> Json {
            limits_.check("guest:" + req.remote_addr, 20, std::chrono::hours(1));
            auto user = db.query("INSERT INTO public.users DEFAULT VALUES RETURNING id,avatar,created_at")[0];
            return {{"access_token", auth_.issue(user["id"])}, {"token_type", "Bearer"},
                    {"expires_in", config_.token_ttl_hours * 3600}, {"user", user}};
        }, 201, false);
    });
    http_.Post("/v1/auth/renew", [this](const auto& req, auto& res) {
        respond(req, res, [this](Db&, const std::string& user) -> Json {
            return {{"access_token", auth_.issue(user)}, {"token_type", "Bearer"},
                    {"expires_in", config_.token_ttl_hours * 3600}};
        });
    });
    http_.Get("/v1/me", [this](const auto& req, auto& res) {
        respond(req, res, [](Db& db, const std::string& user) -> Json {
            return {{"user", db.query("SELECT id,avatar,created_at FROM public.users WHERE id=$1::uuid", {user})[0]}};
        });
    });
    http_.Patch("/v1/me", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b, {"avatar"});
            const auto avatar = text(b, "avatar", 16);
            static const std::set<std::string> animals = {"bear","rabbit","fox","cat","dog","panda","koala","tiger","penguin"};
            if (!animals.contains(avatar)) throw ApiError(400, "invalid_avatar", "Unknown animal character");
            return {{"user", db.query("UPDATE public.users SET avatar=$2 WHERE id=$1::uuid RETURNING id,avatar,created_at", {user,avatar})[0]}};
        });
    });
    http_.Get("/v1/rooms/current", [this](const auto& req, auto& res) {
        respond(req, res, [](Db& db, const std::string& user) -> Json {
            auto c = current(db, user, false);
            return c ? snapshot(db, *c) : Json{{"room", nullptr}};
        });
    });
    http_.Post("/v1/rooms", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b, {"title","nickname"});
            auto title = text(b,"title",60), nickname = text(b,"nickname",20);
            require_new_room(db,user);
            std::string room;
            for (int attempt = 0; attempt < 5; ++attempt) {
                db.query("SAVEPOINT invite_code");
                try {
                    room = db.query("INSERT INTO public.rooms(title,host_id,invite_code,expires_at) "
                        "VALUES($1,$2::uuid,$3,now()+$4::int*interval '1 hour') RETURNING id",
                        {title,user,random_code(),std::to_string(config_.room_ttl_hours)})[0]["id"];
                    db.query("RELEASE SAVEPOINT invite_code");
                    break;
                } catch (const DbError& e) {
                    if (e.state != "23505") throw;
                    db.query("ROLLBACK TO SAVEPOINT invite_code");
                    db.query("RELEASE SAVEPOINT invite_code");
                }
            }
            if (room.empty()) throw ApiError(503,"code_unavailable","Please retry room creation");
            db.query("INSERT INTO public.room_members(room_id,user_id,nickname) VALUES($1::uuid,$2::uuid,$3)", {room,user,nickname});
            return snapshot(db,*current(db,user));
        }, 201);
    });
    http_.Post("/v1/rooms/join", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            limits_.check("join:" + req.remote_addr, 30, std::chrono::minutes(10));
            auto b = body(req); fields(b, {"invite_code","nickname"});
            auto code = text(b,"invite_code",8), nickname = text(b,"nickname",20);
            std::transform(code.begin(),code.end(),code.begin(),[](unsigned char c){return std::toupper(c);});
            if (code.size() != 8) throw ApiError(400,"invalid_code","Invite code must be 8 characters");
            clear_expired(db,user);
            auto existing = db.query("SELECT room_id FROM public.room_members WHERE user_id=$1::uuid", {user});
            auto rooms = db.query("SELECT id FROM public.rooms WHERE invite_code=$1 AND expires_at>now() FOR UPDATE", {code});
            if (rooms.empty()) throw ApiError(404,"room_not_found","Invite code is invalid or the room has ended");
            const std::string room = rooms[0]["id"];
            if (!existing.empty()) {
                if (existing[0]["room_id"] == room) return snapshot(db,*current(db,user));
                throw ApiError(409,"already_in_room","Leave the current room first");
            }
            db.query("INSERT INTO public.room_members(room_id,user_id,nickname) VALUES($1::uuid,$2::uuid,$3)", {room,user,nickname});
            return snapshot(db,*current(db,user));
        });
    });
    http_.Delete("/v1/rooms/current/membership", [this](const auto& req, auto& res) {
        respond(req, res, [](Db& db, const std::string& user) -> Json {
            auto c = current(db,user);
            if (c->host == user) throw ApiError(409,"host_must_end_room","The host must end the room instead");
            db.query("DELETE FROM public.room_members WHERE id=$1::uuid", {c->member});
            return {{"left",true}};
        });
    });
    http_.Delete("/v1/rooms/current", [this](const auto& req, auto& res) {
        respond(req, res, [](Db& db, const std::string& user) -> Json {
            auto c = current(db,user);
            if (c->host != user) throw ApiError(403,"host_only","Only the host can end this room");
            db.query("DELETE FROM public.rooms WHERE id=$1::uuid", {c->room});
            return {{"ended",true}};
        });
    });
    http_.Patch("/v1/rooms/current/membership", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b, {"nickname","transport_mode","is_sharing"});
            if (b.empty()) throw ApiError(400,"invalid_input","Provide at least one field");
            auto c = current(db,user);
            if (b.contains("nickname")) db.query("UPDATE public.room_members SET nickname=$2 WHERE id=$1::uuid", {c->member,text(b,"nickname",20)});
            if (b.contains("transport_mode")) {
                auto mode = text(b,"transport_mode",16);
                if (mode != "walking" && mode != "driving" && mode != "transit") throw ApiError(400,"invalid_input","Unknown transport mode");
                db.query("UPDATE public.room_members SET transport_mode=$2 WHERE id=$1::uuid", {c->member,mode});
            }
            if (b.contains("is_sharing")) {
                if (!b["is_sharing"].is_boolean()) throw ApiError(400,"invalid_input","is_sharing must be boolean");
                bool enabled = b["is_sharing"];
                db.query("UPDATE public.room_members SET is_sharing=$2::boolean WHERE id=$1::uuid", {c->member,enabled ? "true" : "false"});
                if (!enabled) db.query("DELETE FROM public.member_locations WHERE member_id=$1::uuid", {c->member});
            }
            return snapshot(db,*c);
        });
    });
    http_.Put("/v1/rooms/current/location", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b, {"latitude","longitude","accuracy_m","recorded_at"});
            auto coords = mark_coordinates(b);
            auto c = current(db,user);
            auto sharing = db.query("SELECT is_sharing FROM public.room_members WHERE id=$1::uuid", {c->member});
            if (!sharing[0]["is_sharing"].get<bool>()) throw ApiError(409,"sharing_disabled","Enable location sharing first");
            std::optional<std::string> accuracy, recorded;
            if (b.contains("accuracy_m") && !b["accuracy_m"].is_null()) accuracy = Json(number(b,"accuracy_m",0,1000000)).dump();
            if (b.contains("recorded_at")) recorded = text(b,"recorded_at",64);
            auto clock = db.query("SELECT coalesce($1::timestamptz,now()) AS measured, "
                                  "coalesce($1::timestamptz,now()) BETWEEN now()-interval '5 minutes' AND now()+interval '1 minute' AS valid", {recorded});
            if (!clock[0]["valid"].get<bool>()) throw ApiError(400,"invalid_time","Location timestamp must be recent");
            auto rows = db.query(R"SQL(
                INSERT INTO public.member_locations(member_id,room_id,latitude,longitude,accuracy_m,recorded_at)
                VALUES($1::uuid,$2::uuid,$3::float8,$4::float8,$5::float8,$6::timestamptz)
                ON CONFLICT(member_id) DO UPDATE SET latitude=excluded.latitude,longitude=excluded.longitude,
                  accuracy_m=excluded.accuracy_m,recorded_at=excluded.recorded_at,received_at=now()
                WHERE excluded.recorded_at >= public.member_locations.recorded_at RETURNING member_id
            )SQL", {c->member,c->room,coords["latitude"].dump(),coords["longitude"].dump(),accuracy,clock[0]["measured"].get<std::string>()});
            return {{"updated",!rows.empty()}};
        });
    });
    http_.Get("/v1/rooms/current/messages", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto c = current(db,user);
            std::string condition;
            Params params{c->room};
            if (req.has_param("before")) {
                auto before = uuid(req.get_param_value("before"));
                if (db.query("SELECT id FROM public.messages WHERE id=$1::uuid AND room_id=$2::uuid", {before,c->room}).empty())
                    throw ApiError(404,"invalid_cursor","Message cursor not found");
                params.push_back(before);
                condition = " AND (m.created_at,m.id)<(SELECT created_at,id FROM public.messages WHERE id=$2::uuid AND room_id=$1::uuid)";
            }
            auto rows = db.query("SELECT m.id,m.sender_id,m.body,m.created_at,rm.nickname,u.avatar FROM public.messages m "
                "LEFT JOIN public.room_members rm ON rm.user_id=m.sender_id AND rm.room_id=m.room_id "
                "LEFT JOIN public.users u ON u.id=rm.user_id WHERE m.room_id=$1::uuid" + condition +
                " ORDER BY m.created_at DESC,m.id DESC LIMIT 50", params);
            std::reverse(rows.begin(),rows.end());
            Json cursor = rows.size() == 50 ? rows[0]["id"] : Json(nullptr);
            return {{"messages",rows},{"next_before",cursor}};
        });
    });
    http_.Post("/v1/rooms/current/messages", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b,{"body"});
            auto message = text(b,"body",2000);
            auto c = current(db,user);
            return {{"message",db.query("INSERT INTO public.messages(room_id,sender_id,body) VALUES($1::uuid,$2::uuid,$3) "
                "RETURNING id,room_id,sender_id,body,created_at", {c->room,user,message})[0]}};
        },201);
    });
    http_.Post("/v1/rooms/current/marks", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b,{"name","latitude","longitude"});
            auto name = text(b,"name",40); auto coords = mark_coordinates(b);
            auto c = current(db,user);
            return {{"mark",db.query("INSERT INTO public.map_marks(room_id,created_by,name,latitude,longitude) "
                "VALUES($1::uuid,$2::uuid,$3,$4::float8,$5::float8) RETURNING *",
                {c->room,user,name,coords["latitude"].dump(),coords["longitude"].dump()})[0]}};
        },201);
    });
    http_.Patch(R"(/v1/rooms/current/marks/([0-9a-fA-F-]+))", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto b = body(req); fields(b,{"name"});
            auto id = uuid(req.matches[1].str()), name = text(b,"name",40);
            auto c = current(db,user);
            auto rows = db.query("UPDATE public.map_marks SET name=$4 WHERE id=$1::uuid AND room_id=$2::uuid AND created_by=$3::uuid RETURNING *", {id,c->room,user,name});
            if (rows.empty()) throw ApiError(404,"mark_not_found","Mark not found or not editable");
            return {{"mark",rows[0]}};
        });
    });
    http_.Delete(R"(/v1/rooms/current/marks/([0-9a-fA-F-]+))", [this](const auto& req, auto& res) {
        respond(req, res, [&](Db& db, const std::string& user) -> Json {
            auto c = current(db,user);
            auto rows = db.query("DELETE FROM public.map_marks WHERE id=$1::uuid AND room_id=$2::uuid AND created_by=$3::uuid RETURNING id",
                {uuid(req.matches[1].str()),c->room,user});
            if (rows.empty()) throw ApiError(404,"mark_not_found","Mark not found or not editable");
            return {{"deleted",true}};
        });
    });
}
bool App::listen() { return http_.listen(config_.host,config_.port); }
void App::cleanup() {
    auto lease = pool_.acquire();
    lease.db->query("DELETE FROM public.rooms WHERE expires_at<=now()");
}
}
