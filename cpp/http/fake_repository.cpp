// Local in-memory development adapter. NEVER use as evidence of Supabase integration.
#include "gommanna/contracts.hpp"
#include <algorithm>
#include <openssl/rand.h>
#include <cctype>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
using namespace gommanna;
namespace {
Id uid(){static std::atomic<unsigned long long> n{1};auto v=n++;std::ostringstream o;o<<"00000000-0000-4000-8000-"<<std::hex<<std::setw(12)<<std::setfill('0')<<v;return o.str();}
template<class T> Result<T> err(ErrorCode c,const char* m){return Error{c,m};}
struct Store {
 std::mutex mu;
 std::map<std::string,std::pair<User,TimeMs>> sessions;
 std::map<Id,Room> rooms;
 std::map<Id,Member> members;
 std::map<Id,Location> locations;
 std::map<Id,Note> notes;
};
class Fake final:public IRepository {
 Store s;
 Member* lookup(const Id& actor,const Id& room){for(auto& [id,m]:s.members)if(m.user_id==actor&&m.room_id==room)return &m;return nullptr;}
 Error room_check(const Id& actor,const Id& room){auto r=s.rooms.find(room);if(r==s.rooms.end())return {ErrorCode::not_found,"Room not found"};if(!lookup(actor,room))return {ErrorCode::forbidden,"Not a room member"};if(!r->second.active)return {ErrorCode::room_closed,"Room closed"};return {ErrorCode::internal,""};}
 public:
 Result<User> create_guest(const std::string& hash,TimeMs now,TimeMs expiry) override {std::lock_guard l(s.mu);User u{uid(),now};s.sessions[hash]={u,expiry};return u;}
 Result<User> authenticate_guest(const std::string& hash,TimeMs now) override {std::lock_guard l(s.mu);auto i=s.sessions.find(hash);if(i==s.sessions.end()||i->second.second<=now)return err<User>(ErrorCode::unauthenticated,"Invalid or expired token");return i->second.first;}
 Result<Room> get_room(const Id& actor,const Id& room) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;return s.rooms.at(room);}
 Result<Room> find_room_by_code(const std::string& code) override {std::lock_guard l(s.mu);for(auto& [id,r]:s.rooms)if(r.invite_code==code)return r;return err<Room>(ErrorCode::not_found,"Invite code not found");}
 Result<RoomMembership> create_room(const Id& actor,const std::string& title,const std::string& nick,const std::string& code,TimeMs now) override {std::lock_guard l(s.mu);for(auto& [id,r]:s.rooms)if(r.invite_code==code)return err<RoomMembership>(ErrorCode::invite_code_conflict,"Invite code collision");Room r{uid(),title,actor,code,true,now,{}};Member m{uid(),r.id,actor,nick,false,0,now};s.rooms[r.id]=r;s.members[m.id]=m;return RoomMembership{r,m};}
 Result<Member> join_room(const Id& actor,const Id& room,const std::string& nick,TimeMs now) override {std::lock_guard l(s.mu);auto it=s.rooms.find(room);if(it==s.rooms.end())return err<Member>(ErrorCode::not_found,"Room not found");if(!it->second.active)return err<Member>(ErrorCode::room_closed,"Room closed");if(auto* m=lookup(actor,room))return *m;int count=0;for(auto& [id,m]:s.members)if(m.room_id==room)++count;if(count>=kMaxRoomMembers)return err<Member>(ErrorCode::conflict,"Room full");Member m{uid(),room,actor,nick,false,0,now};s.members[m.id]=m;return m;}
 Result<Unit> leave_room(const Id& actor,const Id& room,TimeMs) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;if(s.rooms.at(room).host_id==actor)return err<Unit>(ErrorCode::forbidden,"Host must end room");auto m=lookup(actor,room);s.locations.erase(m->id);s.notes.erase(m->id);s.members.erase(m->id);return Unit{};}
 Result<Unit> end_room(const Id& actor,const Id& room,TimeMs now) override {std::lock_guard l(s.mu);auto it=s.rooms.find(room);if(it==s.rooms.end())return err<Unit>(ErrorCode::not_found,"Room not found");if(it->second.host_id!=actor)return err<Unit>(ErrorCode::forbidden,"Host only");if(!it->second.active)return Unit{};it->second.active=false;it->second.ended_at_ms=now;for(auto i=s.members.begin();i!=s.members.end();){if(i->second.room_id==room){s.locations.erase(i->first);s.notes.erase(i->first);i=s.members.erase(i);}else ++i;}return Unit{};}
 Result<std::vector<MemberView>> list_visible_members(const Id& actor,const Id& room,TimeMs now) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;std::vector<MemberView> out;for(auto& [id,m]:s.members){if(m.room_id!=room)continue;MemberView v;v.member=m;if(!m.is_sharing){v.location_status=Freshness::off;}else{auto it=s.locations.find(id);if(it!=s.locations.end()&&it->second.sharing_epoch==m.sharing_epoch){auto& p=it->second;auto age=std::max<TimeMs>(0,now-p.captured_at_ms);v.location_status=age<kDelayedAfterMs?Freshness::fresh:age<kStaleAfterMs?Freshness::delayed:Freshness::stale;v.location=p;}auto nt=s.notes.find(id);if(nt!=s.notes.end()&&nt->second.sharing_epoch==m.sharing_epoch)v.note=nt->second;}out.push_back(v);}return out;}
 Result<Member> set_sharing(const Id& actor,const Id& room,bool enabled,TimeMs) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;auto* m=lookup(actor,room);if(enabled&&!m->is_sharing)++m->sharing_epoch;m->is_sharing=enabled;return *m;}
 Result<Location> update_location(const Id& actor,const Id& room,const LocationUpdate& u,TimeMs now) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;auto* m=lookup(actor,room);if(!m->is_sharing)return err<Location>(ErrorCode::sharing_off,"Sharing is off");if(u.sharing_epoch!=m->sharing_epoch)return err<Location>(ErrorCode::stale_update,"Old sharing epoch");if(u.captured_at_ms>now+kFutureToleranceMs||u.sequence<1||!std::isfinite(u.coordinate.latitude)||!std::isfinite(u.coordinate.longitude)||std::abs(u.coordinate.latitude)>90||std::abs(u.coordinate.longitude)>180)return err<Location>(ErrorCode::invalid_argument,"Invalid location");auto it=s.locations.find(m->id);if(it!=s.locations.end()&&it->second.sharing_epoch==u.sharing_epoch&&(u.sequence<=it->second.sequence||u.captured_at_ms<it->second.captured_at_ms))return err<Location>(ErrorCode::stale_update,"Out-of-order location");Location p{u.coordinate,u.accuracy_m,u.captured_at_ms,now,u.sharing_epoch,u.sequence};s.locations[m->id]=p;return p;}
 Result<Note> save_note(const Id& actor,const Id& room,const NoteDraft& d,TimeMs now) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;auto* m=lookup(actor,room);if(!m->is_sharing)return err<Note>(ErrorCode::sharing_off,"Sharing is off");if(d.sharing_epoch!=m->sharing_epoch)return err<Note>(ErrorCode::stale_update,"Old sharing epoch");Note n;n.id=s.notes.contains(m->id)?s.notes.at(m->id).id:uid();n.text=d.text;n.written_at_ms=now;n.sharing_epoch=d.sharing_epoch;auto p=s.locations.find(m->id);if(p!=s.locations.end()&&p->second.sharing_epoch==m->sharing_epoch&&now-p->second.captured_at_ms>=-kFutureToleranceMs&&now-p->second.captured_at_ms<kDelayedAfterMs){n.origin=p->second.coordinate;n.origin_captured_at_ms=p->second.captured_at_ms;}s.notes[m->id]=n;return n;}
 Result<Unit> delete_note(const Id& actor,const Id& room,TimeMs) override {std::lock_guard l(s.mu);auto e=room_check(actor,room);if(e.message.size())return e;auto* m=lookup(actor,room);s.notes.erase(m->id);return Unit{};}
 Result<Note> attach_photo(const Id&,const Id&,const Id&,TimeMs,const PhotoRef&,Epoch,TimeMs) override {return err<Note>(ErrorCode::not_implemented,"Photo not implemented");}
};
// Only a development room service; teammate's gm_room is used in real mode.
class FakeRoom final:public IRoomService {
 IRepository& db;
 public:explicit FakeRoom(IRepository& r):db(r){}
 Result<RoomMembership> create_room(const Id& actor,const std::string& title,const std::string& nick,TimeMs now) override {static constexpr char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";for(int retry=0;retry<5;retry++){unsigned char bytes[8];if(RAND_bytes(bytes,8)!=1)return Error{ErrorCode::internal,"Random generator failed"};std::string code;for(auto b:bytes)code+=alphabet[b%32];auto result=db.create_room(actor,title,nick,code,now);auto e=std::get_if<Error>(&result);if(!e||e->code!=ErrorCode::invite_code_conflict)return result;}return Error{ErrorCode::invite_code_conflict,"Invite collision"};}
 Result<RoomMembership> join_room(const Id& actor,const std::string& invite,const std::string& nick,TimeMs now) override {std::string code;for(char c:invite)if(c!=' '&&c!='\t')code+=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));auto found=db.find_room_by_code(code);if(auto e=std::get_if<Error>(&found))return *e;auto r=std::get<Room>(found);if(!r.active)return Error{ErrorCode::room_closed,"Room closed"};auto m=db.join_room(actor,r.id,nick,now);if(auto e=std::get_if<Error>(&m))return *e;return RoomMembership{r,std::get<Member>(m)};}
 Result<Unit> leave_room(const Id& a,const Id& r,TimeMs t) override{return db.leave_room(a,r,t);}
 Result<Unit> end_room(const Id& a,const Id& r,TimeMs t) override{return db.end_room(a,r,t);}
 Result<Invite> get_invite(const Id& a,const Id& r) override {auto result=db.get_room(a,r);if(auto e=std::get_if<Error>(&result))return *e;auto code=std::get<Room>(result).invite_code;return Invite{code,"gommanna://join?code="+code,""};} // QR SVG intentionally unavailable in dev stub
};
}
namespace gommanna {
std::unique_ptr<IRepository> make_fake_repository(){return std::make_unique<Fake>();}
std::unique_ptr<IRoomService> make_fake_room_service(IRepository& repo){return std::make_unique<FakeRoom>(repo);}
}
