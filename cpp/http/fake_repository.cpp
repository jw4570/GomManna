// In-memory development adapter. Verify production behavior with PostgreSQL too.
#include "gommanna/contracts.hpp"
#include "gommanna/data_policy.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <tuple>
using namespace gommanna;
namespace {
Id uid(){static std::atomic<unsigned long long> n{1};std::ostringstream s;s<<"00000000-0000-4000-8000-"<<std::hex<<std::setw(12)<<std::setfill('0')<<n++;return s.str();}
struct Failure { Error error; };
[[noreturn]] void reject(ErrorCode code,const char* text){throw Failure{{code,text}};}
struct Retention { TimeMs activity{};std::optional<TimeMs> stopped;bool purged{}; };
class Fake final:public IRepository {
 std::mutex mutex;
 std::map<Id,User> users;
 std::map<std::string,std::pair<Id,TimeMs>> sessions;
 std::map<Id,Room> rooms;
 std::map<Id,Member> members;
 std::map<Id,Location> locations;
 std::map<Id,Note> notes;
 std::map<Id,DeviceStatus> devices;
 std::map<Id,MapMark> marks;
 std::map<Id,std::vector<RoutePoint>> routes;
 std::map<Id,Retention> retention;
 std::map<std::string,TimeMs> photo_cleanup;
 std::int64_t route_id{};
 template<class T,class F> Result<T> run(F&& f){
   std::lock_guard lock(mutex);try{return f();}catch(const Failure& e){return e.error;}
 }
 Member* lookup(const Id& actor,const Id& room,bool left=false){
   for(auto& [id,m]:members)if(m.user_id==actor&&m.room_id==room&&(left||!m.left_at_ms))return &m;
   return nullptr;
 }
 Member& own(const Id& actor,const Id& room,bool left=false){
   auto* m=lookup(actor,room,left);if(!m)reject(ErrorCode::forbidden,"not a room member");return *m;
 }
 Room& room_check(const Id& room,bool ended=false){
   auto i=rooms.find(room);if(i==rooms.end())reject(ErrorCode::not_found,"room not found");
   if(!ended&&!i->second.active)reject(ErrorCode::room_closed,"room closed");return i->second;
 }
 User& user(const Id& actor){auto i=users.find(actor);if(i==users.end())reject(ErrorCode::unauthenticated,"guest not found");return i->second;}
 void refresh(const Id& room,TimeMs now,bool activity=false){
   auto& state=retention[room];bool sharing=false;
   for(const auto& [id,m]:members)if(m.room_id==room&&!m.left_at_ms&&m.is_sharing)sharing=true;
   if(sharing)state.stopped.reset();else if(!state.stopped||(state.purged&&activity))state.stopped=now;
   if(activity)state.activity=now;state.purged=false;
 }
 void erase_note(const Id& member,TimeMs now){
   auto n=notes.find(member);if(n!=notes.end()&&n->second.photo)photo_cleanup[n->second.photo->key]=now;
   notes.erase(member);
 }
 void erase_content(const Member& m,TimeMs now){
   locations.erase(m.id);erase_note(m.id,now);devices.erase(m.id);routes.erase(m.id);
   std::erase_if(marks,[&](const auto& item){return item.second.room_id==m.room_id&&item.second.creator_id==m.user_id;});
 }
 void check_sharing(const Member& m,Epoch epoch){
   if(!m.is_sharing)reject(ErrorCode::sharing_off,"sharing is off");
   if(m.sharing_epoch!=epoch)reject(ErrorCode::stale_update,"old sharing epoch");
 }
 void coordinate(Coordinate c){
   if(!std::isfinite(c.latitude)||!std::isfinite(c.longitude)||std::abs(c.latitude)>90||std::abs(c.longitude)>180)
     reject(ErrorCode::invalid_argument,"invalid coordinate");
 }
 public:
 Result<User> create_guest(const std::string& hash,TimeMs now,TimeMs expiry) override{return run<User>([&]{
   if(sessions.contains(hash))reject(ErrorCode::conflict,"duplicate token");
   User u{uid(),now,"bear",expiry};users[u.id]=u;sessions[hash]={u.id,expiry};return u;
 });}
 Result<User> authenticate_guest(const std::string& hash,TimeMs now) override{return run<User>([&]{
   auto s=sessions.find(hash);if(s==sessions.end()||s->second.second<=now)reject(ErrorCode::unauthenticated,"invalid or expired token");
   s->second.second=std::max(s->second.second,guest_session_expiry(now));
   auto result=user(s->second.first);result.expires_at_ms=s->second.second;return result;
 });}
 Result<User> get_profile(const Id& actor) override{return run<User>([&]{return user(actor);});}
 Result<User> update_profile(const Id& actor,const std::string& icon) override{return run<User>([&]{
   if(!valid_animal_icon(icon))reject(ErrorCode::invalid_argument,"unknown animal icon");
   auto& u=user(actor);u.animal_icon=icon;return u;
 });}
 Result<Room> get_room(const Id& actor,const Id& id) override{return run<Room>([&]{auto r=room_check(id);own(actor,id);return r;});}
 Result<Room> find_room_by_code(const std::string& code) override{return run<Room>([&]{
   for(const auto& [id,r]:rooms)if(r.invite_code==code)return room_check(id);
   reject(ErrorCode::not_found,"invite code not found");
 });}
 Result<RoomMembership> create_room(const Id& actor,const std::string& title,const std::string& name,const std::string& code,TimeMs now) override{
   return run<RoomMembership>([&]{user(actor);
     for(const auto& [id,r]:rooms)if(r.invite_code==code)reject(ErrorCode::invite_code_conflict,"invite code collision");
     Room r{uid(),title,actor,code,true,now,{}};Member m{uid(),r.id,actor,name,false,0,now};
     m.animal_icon=user(actor).animal_icon;rooms[r.id]=r;members[m.id]=m;retention[r.id]={now,now,false};
     return RoomMembership{r,m};
   });
 }
 Result<Member> join_room(const Id& actor,const Id& room,const std::string& name,TimeMs now) override{return run<Member>([&]{
   room_check(room);user(actor);if(auto* m=lookup(actor,room))return *m;
   int count=0;for(const auto& [id,m]:members)if(m.room_id==room&&!m.left_at_ms)++count;
   if(count>=kMaxRoomMembers)reject(ErrorCode::conflict,"room full");
   if(auto* m=lookup(actor,room,true)){m->left_at_ms.reset();m->is_sharing=false;++m->sharing_epoch;m->nickname=name;m->joined_at_ms=now;devices.erase(m->id);return *m;}
   Member m{uid(),room,actor,name,false,0,now};m.animal_icon=user(actor).animal_icon;members[m.id]=m;return m;
 });}
 Result<Unit> leave_room(const Id& actor,const Id& room,TimeMs now) override{return run<Unit>([&]{
   auto& r=room_check(room);auto& m=own(actor,room);
   if(r.host_id==actor)reject(ErrorCode::forbidden,"host must end room");
   m.is_sharing=false;++m.sharing_epoch;m.left_at_ms=now;refresh(room,now);return Unit{};
 });}
 Result<Unit> end_room(const Id& actor,const Id& room,TimeMs now) override{return run<Unit>([&]{
   auto& r=room_check(room,true);if(r.host_id!=actor)reject(ErrorCode::forbidden,"host only");
   if(r.active){r.active=false;r.ended_at_ms=now;for(auto& [id,m]:members)if(m.room_id==room){m.is_sharing=false;++m.sharing_epoch;}}
   return Unit{};
 });}
 Result<std::vector<MemberView>> list_visible_members(const Id& actor,const Id& room,TimeMs now) override{
   return run<std::vector<MemberView>>([&]{room_check(room);own(actor,room);std::vector<MemberView> result;
     for(const auto& [id,m]:members){if(m.room_id!=room||m.left_at_ms)continue;
       MemberView v;v.member=m;v.member.animal_icon=user(m.user_id).animal_icon;
       if(m.is_sharing){
         if(locations.contains(id)&&locations.at(id).sharing_epoch==m.sharing_epoch)v.location=locations.at(id);
         if(notes.contains(id)&&notes.at(id).sharing_epoch==m.sharing_epoch)v.note=notes.at(id);
         if(devices.contains(id)&&devices.at(id).sharing_epoch==m.sharing_epoch)v.device_status=devices.at(id);
       }
       v.location_status=classify_location(v.location,m.is_sharing,m.sharing_epoch,now);result.push_back(v);
     }return result;
   });
 }
 Result<Member> set_sharing(const Id& actor,const Id& room,bool enabled,TimeMs now) override{return run<Member>([&]{
   room_check(room);auto& m=own(actor,room);if(enabled&&!m.is_sharing)++m.sharing_epoch;m.is_sharing=enabled;
   refresh(room,now,enabled);return m;
 });}
 Result<Location> update_location(const Id& actor,const Id& room,const LocationUpdate& u,TimeMs now) override{return run<Location>([&]{
   room_check(room);auto& m=own(actor,room);check_sharing(m,u.sharing_epoch);coordinate(u.coordinate);
   if(u.captured_at_ms<0||u.captured_at_ms>now+kFutureToleranceMs||u.sequence<1||
      (u.accuracy_m&&(!std::isfinite(*u.accuracy_m)||*u.accuracy_m<0)))reject(ErrorCode::invalid_argument,"invalid location");
   auto previous=locations.find(m.id);
   if(previous!=locations.end()&&previous->second.sharing_epoch==u.sharing_epoch&&
      (u.sequence<=previous->second.sequence||u.captured_at_ms<previous->second.captured_at_ms))reject(ErrorCode::stale_update,"out of order");
   Location value{u.coordinate,u.accuracy_m,u.captured_at_ms,now,u.sharing_epoch,u.sequence};locations[m.id]=value;
   auto& history=routes[m.id];std::optional<Location> last;if(!history.empty())last=history.back().location;
   if(should_store_route(last,value))history.push_back({++route_id,m.id,value});
   refresh(room,now,true);return value;
 });}
 Result<Note> save_note(const Id& actor,const Id& room,const NoteDraft& draft,TimeMs now) override{return run<Note>([&]{
   room_check(room);auto& m=own(actor,room);check_sharing(m,draft.sharing_epoch);
   Note n;n.id=notes.contains(m.id)?notes.at(m.id).id:uid();erase_note(m.id,now);
   n.text=draft.text;n.written_at_ms=now;n.sharing_epoch=draft.sharing_epoch;
   if(locations.contains(m.id)){auto& p=locations.at(m.id);if(classify_location(p,true,m.sharing_epoch,now)==Freshness::fresh){n.origin=p.coordinate;n.origin_captured_at_ms=p.captured_at_ms;}}
   notes[m.id]=n;refresh(room,now);return n;
 });}
 Result<Unit> delete_note(const Id& actor,const Id& room,TimeMs now) override{return run<Unit>([&]{
   room_check(room);auto& m=own(actor,room);if(!m.is_sharing)reject(ErrorCode::sharing_off,"sharing is off");erase_note(m.id,now);return Unit{};
 });}
 Result<Note> attach_photo(const Id& actor,const Id& room,const Id& id,TimeMs written,const PhotoRef& photo,Epoch epoch,TimeMs now) override{return run<Note>([&]{
   room_check(room);auto& m=own(actor,room);check_sharing(m,epoch);auto i=notes.find(m.id);
   if(i==notes.end())reject(ErrorCode::not_found,"note not found");auto& n=i->second;
   if(n.id!=id||n.written_at_ms!=written||n.sharing_epoch!=epoch)reject(ErrorCode::stale_update,"note changed");
   if(n.photo)photo_cleanup[n.photo->key]=now;n.photo=photo;photo_cleanup.erase(photo.key);refresh(room,now);return n;
 });}
 Result<Unit> reserve_photo_cleanup(const std::string& key,TimeMs due) override{return run<Unit>([&]{photo_cleanup.emplace(key,due);return Unit{};});}
 Result<std::vector<std::string>> due_photo_cleanup(TimeMs now) override{return run<std::vector<std::string>>([&]{
   std::vector<std::string> out;for(const auto& [key,due]:photo_cleanup){
     if(due>now)continue;bool used=false;for(const auto& [id,n]:notes)if(n.photo&&n.photo->key==key)used=true;
     if(!used)out.push_back(key);if(out.size()==20)break;
   }return out;
 });}
 Result<Unit> finish_photo_cleanup(const std::string& key) override{return run<Unit>([&]{
   for(const auto& [id,n]:notes)if(n.photo&&n.photo->key==key)return Unit{};photo_cleanup.erase(key);return Unit{};
 });}
 Result<Unit> delete_user(const Id& actor,TimeMs now) override{return run<Unit>([&]{
   user(actor);
   for(auto& [id,r]:rooms)if(r.host_id==actor){
     Member* successor=nullptr;
     if(r.active)for(auto& [mid,m]:members)if(m.room_id==id&&m.user_id!=actor&&!m.left_at_ms&&
       (!successor||std::tie(m.joined_at_ms,m.id)<std::tie(successor->joined_at_ms,successor->id)))successor=&m;
     if(successor)r.host_id=successor->user_id;
     else{r.host_id.clear();if(r.active){r.active=false;r.ended_at_ms=now;for(auto& [mid,m]:members)if(m.room_id==id){m.is_sharing=false;++m.sharing_epoch;}}}
   }
   for(auto i=members.begin();i!=members.end();)if(i->second.user_id==actor){auto room=i->second.room_id;erase_content(i->second,now);i=members.erase(i);refresh(room,now);}else ++i;
   std::erase_if(sessions,[&](const auto& s){return s.second.first==actor;});users.erase(actor);return Unit{};
 });}
 Result<Unit> delete_member_data(const Id& actor,const Id& room,bool route_only,TimeMs now) override{return run<Unit>([&]{
   room_check(room,true);auto& m=own(actor,room,true);routes.erase(m.id);
   if(!route_only){erase_content(m,now);m.is_sharing=false;++m.sharing_epoch;refresh(room,now);}return Unit{};
 });}
 Result<DeviceStatus> update_device_status(const Id& actor,const Id& room,const DeviceStatusDraft& draft,TimeMs now) override{return run<DeviceStatus>([&]{
   room_check(room);auto& m=own(actor,room);check_sharing(m,draft.sharing_epoch);
   if(draft.battery_percent&&(*draft.battery_percent<0||*draft.battery_percent>100))reject(ErrorCode::invalid_argument,"invalid battery percentage");
   DeviceStatus d{draft.battery_percent,draft.is_charging,now,m.sharing_epoch};devices[m.id]=d;refresh(room,now,true);return d;
 });}
 Result<std::vector<MapMark>> list_marks(const Id& actor,const Id& room) override{return run<std::vector<MapMark>>([&]{
   room_check(room);own(actor,room);std::vector<MapMark> result;
   for(const auto& [id,m]:marks)if(m.room_id==room&&lookup(m.creator_id,room))result.push_back(m);return result;
 });}
 Result<MapMark> create_mark(const Id& actor,const Id& room,const MapMarkDraft& draft,TimeMs now) override{return run<MapMark>([&]{
   room_check(room);own(actor,room);coordinate(draft.coordinate);
   if(draft.name.empty())reject(ErrorCode::invalid_argument,"empty name");
   int count=0;for(const auto& [id,m]:marks)if(m.room_id==room)++count;
   if(count>=200)reject(ErrorCode::conflict,"room mark limit reached");
   MapMark m{uid(),room,actor,draft.name,draft.coordinate,now,now};marks[m.id]=m;refresh(room,now);return m;
 });}
 Result<MapMark> rename_mark(const Id& actor,const Id& room,const Id& id,const std::string& name,TimeMs now) override{return run<MapMark>([&]{
   auto& r=room_check(room);own(actor,room);auto i=marks.find(id);
   if(i==marks.end()||i->second.room_id!=room)reject(ErrorCode::not_found,"mark not found");
   if(i->second.creator_id!=actor&&r.host_id!=actor)reject(ErrorCode::forbidden,"mark creator or host only");
   if(name.empty())reject(ErrorCode::invalid_argument,"empty name");
   i->second.name=name;i->second.updated_at_ms=now;refresh(room,now);return i->second;
 });}
 Result<Unit> delete_mark(const Id& actor,const Id& room,const Id& id) override{return run<Unit>([&]{
   auto& r=room_check(room);own(actor,room);auto i=marks.find(id);
   if(i==marks.end()||i->second.room_id!=room)reject(ErrorCode::not_found,"mark not found");
   if(i->second.creator_id!=actor&&r.host_id!=actor)reject(ErrorCode::forbidden,"mark creator or host only");
   marks.erase(i);return Unit{};
 });}
 Result<RoutePage> get_route(const Id& actor,const Id& room,const Id& id,std::int64_t after,int limit,TimeMs) override{return run<RoutePage>([&]{
   auto& r=room_check(room,true);auto& self=own(actor,room,true);
   if(after<0||limit<1||limit>500)reject(ErrorCode::invalid_argument,"invalid page");
   auto mid=id.empty()?self.id:id;bool mine=mid==self.id;
   if(!mine&&(!r.active||self.left_at_ms))reject(ErrorCode::forbidden,"active membership required");
   auto target=members.find(mid);if(target==members.end()||target->second.room_id!=room)reject(ErrorCode::not_found,"member not found");
   auto& m=target->second;RoutePage page;
   if(!mine&&(m.left_at_ms||!m.is_sharing))return page;
   for(const auto& point:routes[mid])if(point.id>after&&(mine||point.location.sharing_epoch==m.sharing_epoch)){
     page.points.push_back(point);if(page.points.size()>static_cast<std::size_t>(limit))break;
   }
   if(page.points.size()>static_cast<std::size_t>(limit)){page.points.pop_back();page.next_after_id=page.points.back().id;}return page;
 });}
 Result<Unit> collect_expired_data(TimeMs now) override{return run<Unit>([&]{
   for(auto i=rooms.begin();i!=rooms.end();){
     const auto& r=i->second;auto& state=retention.at(r.id);
     bool ended=!r.active&&r.ended_at_ms&&now-*r.ended_at_ms>=kRetentionMs;
     bool idle=!state.purged&&((state.stopped&&now-*state.stopped>=kRetentionMs)||now-state.activity>=kRoomInactivityMs+kRetentionMs);
     if(ended||idle){
       for(auto m=members.begin();m!=members.end();)if(m->second.room_id==r.id&&(ended||!m->second.left_at_ms)){
         erase_content(m->second,now);if(ended)m=members.erase(m);else ++m;
       }else ++m;
       if(ended){retention.erase(r.id);i=rooms.erase(i);continue;}state.purged=true;
     }++i;
   }
   std::erase_if(sessions,[&](const auto& s){return s.second.second<=now;});return Unit{};
 });}
};
}
namespace gommanna {std::unique_ptr<IRepository> make_fake_repository(){return std::make_unique<Fake>();}}
