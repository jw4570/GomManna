#include "environment.hpp"
#include "photo_service.hpp"
#include "stb_image_write.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace gommanna;
namespace {
TimeMs now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
template<class T> T value(Result<T> result, const char* msg) {
    if (std::holds_alternative<Error>(result)) throw std::runtime_error(msg);
    return std::get<T>(std::move(result));
}
std::string hash_for(std::uint64_t number) {
    std::ostringstream s; s << std::hex << std::setfill('0') << std::setw(64) << number; return s.str();
}
void append(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
}
class MemoryStorage final : public PhotoStorage {
public:
    std::map<std::string, SanitizedPhoto> objects;
    std::function<void()> after_upload;
    Result<Unit> upload(const std::string& key, const SanitizedPhoto& data) override {
        objects[key]=data; if(after_upload) after_upload(); return Unit{};
    }
    Result<SanitizedPhoto> download(const std::string& key) override {
        if(!objects.contains(key)) return Error{ErrorCode::storage_unavailable,"missing"};
        return objects.at(key);
    }
    Result<Unit> remove(const std::string& key) override { objects.erase(key); return Unit{}; }
};
struct RoomCleanup {
    IRepository* repo{};
    Id host, room;
    ~RoomCleanup() { if(repo && !host.empty() && !room.empty()) (void)repo->end_room(host, room, now_ms()); }
};
}

int main() {
    const auto* run = std::getenv("GOMMANNA_RUN_HTTP_DB_INTEGRATION");
    if (!run || std::string(run)!="1" || !std::getenv("GOMMANNA_ENV_FILE")) return 77;
    try {
        const auto env=http_config::load_file();
        const auto url=http_config::get(env,"GOMMANNA_DATABASE_URL");
        auto db=value(make_postgres_repository({url}),"DB connection");
        auto rooms=make_room_service(*db);
        RoomCleanup cleanup{db.get()};
        auto now=now_ms();
        const auto host=value(db->create_guest(hash_for(static_cast<std::uint64_t>(now)*100+31),
            now,now+kGuestSessionTtlMs),"host");
        const auto guest=value(db->create_guest(hash_for(static_cast<std::uint64_t>(now)*100+32),
            now,now+kGuestSessionTtlMs),"guest");
        auto room=value(rooms->create_room(host.id,"photo DB test","host",now),"room");
        cleanup.host=host.id; cleanup.room=room.room.id;
        value(rooms->join_room(guest.id,room.room.invite_code,"guest",now),"join");
        value(db->set_sharing(host.id,room.room.id,true,now),"sharing");
        auto note=value(db->save_note(host.id,room.room.id,{"synthetic",1},now),"note");
        const unsigned char pixels[]={255,0,0, 0,255,0, 0,0,255, 255,255,255};
        std::string png;
        check(stbi_write_png_to_func(append,&png,2,2,3,pixels,6)!=0,"PNG fixture");
        MemoryStorage storage;
        PhotoService photos(*db,storage);
        auto attached=value(photos.upload(host.id,room.room.id,note.id,note.written_at_ms,1,png,now),"photo attach");
        check(attached.note.photo.has_value() && storage.objects.size()==1,"photo stored");
        value(photos.download(guest.id,room.room.id,room.member.id,now),"authorized photo");
        value(db->set_sharing(host.id,room.room.id,false,now),"disable");
        check(std::holds_alternative<Error>(photos.download(guest.id,room.room.id,room.member.id,now)),"OFF hides photo");
        value(db->set_sharing(host.id,room.room.id,true,now),"resume");
        check(std::holds_alternative<Error>(photos.download(guest.id,room.room.id,room.member.id,now)),"old epoch hidden");
        note=value(db->save_note(host.id,room.room.id,{"new",2},now+10),"replace note");
        photos.collect_garbage(now_ms()+1000);
        check(storage.objects.empty(),"database trigger queued old photo");
        storage.after_upload=[&]{ (void)db->set_sharing(host.id,room.room.id,false,now+20); };
        auto raced=photos.upload(host.id,room.room.id,note.id,note.written_at_ms,2,png,now+20);
        check(std::holds_alternative<Error>(raced) && std::get<Error>(raced).code==ErrorCode::sharing_off,
              "OFF during upload must reject attachment");
        check(storage.objects.size()==1,"unattached object retained for cleanup");
        photos.collect_garbage(now+20+10LL*60*1000);
        check(storage.objects.empty(),"reserved object cleaned after grace period");
        std::cout << "PASS: real DB photo attachment, visibility, trigger and cleanup; in-memory object storage\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
