#include "gommanna/contracts.hpp"
#include "gommanna/data_policy.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace gommanna;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T value(Result<T> result,const char* message){
    if(auto* e=std::get_if<Error>(&result))throw std::runtime_error(std::string(message)+": "+e->message);
    return std::get<T>(std::move(result));
}
template<class T>void error(Result<T> result,ErrorCode code,const char* message){
    check(std::holds_alternative<Error>(result)&&std::get<Error>(result).code==code,message);
}
std::string hash(std::uint64_t seed){std::ostringstream s;s<<std::hex<<std::setw(64)<<std::setfill('0')<<seed;return s.str();}
TimeMs date(int year,unsigned month,unsigned day){
    using namespace std::chrono;return duration_cast<milliseconds>(sys_days{std::chrono::year{year}/month/day}.time_since_epoch()).count();
}
void policies(){
    check(guest_session_expiry(date(2026,8,31))==date(2027,2,28),"month-end clamp");
    check(guest_session_expiry(date(2027,8,31))==date(2028,2,29),"leap-year clamp");
    check(guest_session_expiry(date(2028,2,29)+123456)==date(2028,8,29)+123456,"preserve UTC time of day");
    Location first{{0,0},{},1000,1000,1,1},next=first;
    next.coordinate.longitude=19.99/kEarthRadiusM*180/3.14159265358979323846;
    next.received_at_ms=60000;check(!should_store_route(first,next),"under 20m and under 60sec");
    next.received_at_ms=61000;check(should_store_route(first,next),"exact 60sec");
    next.received_at_ms=1001;next.coordinate.longitude=20.01/kEarthRadiusM*180/3.14159265358979323846;
    check(should_store_route(first,next),"20m movement");next=first;next.sharing_epoch=2;
    check(should_store_route(first,next),"new sharing epoch starts a segment");
}
void exercise(IRepository& db,TimeMs t){
    auto hhash=hash(t*100+1),ghash=hash(t*100+2),xhash=hash(t*100+3);
    auto host=value(db.create_guest(hhash,t,guest_session_expiry(t)),"host");
    auto guest=value(db.create_guest(ghash,t,guest_session_expiry(t)),"guest");
    auto outsider=value(db.create_guest(xhash,t,guest_session_expiry(t)),"outsider");
    auto renewed=value(db.authenticate_guest(hhash,t+86400000),"renew token");
    check(renewed.expires_at_ms==guest_session_expiry(t+86400000),"six calendar months after authentication");
    auto expired_hash=hash(t*100+4);value(db.create_guest(expired_hash,t,t+1000),"short session");
    error(db.authenticate_guest(expired_hash,t+1000),ErrorCode::unauthenticated,"expired token never renews");
    auto room=value(db.create_room(host.id,"privacy","host","ABCD23",t),"room");
    auto id=room.room.id;
    auto guest_member=value(db.join_room(guest.id,id,"guest",t+1),"join");
    value(db.update_profile(host.id,"fox"),"profile");
    error(db.update_profile(host.id,"unapproved"),ErrorCode::invalid_argument,"animal allow-list");
    auto shared=value(db.set_sharing(host.id,id,true,t+2),"host share");
    value(db.set_sharing(guest.id,id,true,t+2),"guest share");
    value(db.update_device_status(host.id,id,{12,false,shared.sharing_epoch},t+3),"battery");
    error(db.update_device_status(host.id,id,{101,false,shared.sharing_epoch},t+3),ErrorCode::invalid_argument,"battery bound");
    auto views=value(db.list_visible_members(guest.id,id,t+4),"visible");
    bool found=false;for(const auto& view:views)if(view.member.user_id==host.id){
        found=true;check(view.member.animal_icon=="fox"&&view.device_status&&view.device_status->battery_percent==12,"profile and battery shared");
    }check(found,"host visible");
    auto mark=value(db.create_mark(guest.id,id,{"meet",{37.5,127}},t+5),"create mark");
    error(db.list_marks(outsider.id,id),ErrorCode::forbidden,"outsider cannot see marks");
    auto host_mark=value(db.create_mark(host.id,id,{"host",{37.6,127}},t+5),"host mark");
    error(db.rename_mark(guest.id,id,host_mark.id,"hack",t+6),ErrorCode::forbidden,"creator or host only");
    value(db.rename_mark(host.id,id,mark.id,"renamed",t+6),"host can rename");
    check(value(db.list_marks(guest.id,id),"marks").size()==2,"marks shared");
    value(db.delete_mark(guest.id,id,mark.id),"delete own mark");
    auto update=[&](const Id& user,double meters,TimeMs at,std::int64_t seq,Epoch epoch=1){
        return db.update_location(user,id,{{0,meters/kEarthRadiusM*180/3.14159265358979323846},1.0,at,epoch,seq},at);
    };
    value(update(host.id,0,t+1000,1),"first point");
    value(update(host.id,10,t+6000,2),"small movement");
    value(update(host.id,22,t+11000,3),"cumulative movement");
    value(update(host.id,22,t+70000,4),"under minute");
    value(update(host.id,22,t+71000,5),"minute point");
    auto route=value(db.get_route(host.id,id,"",0,500,t+71000),"route");
    check(route.points.size()==3,"sampled route count");
    auto first=value(db.get_route(guest.id,id,room.member.id,0,2,t+71000),"page one");
    check(first.points.size()==2&&first.next_after_id,"bounded first page");
    check(value(db.get_route(guest.id,id,room.member.id,*first.next_after_id,2,t+71000),"page two").points.size()==1,"second page");
    error(update(host.id,99,t+72000,5),ErrorCode::stale_update,"duplicate rejected");
    check(value(db.get_route(host.id,id,"",0,500,t+72000),"route after stale").points.size()==3,"duplicate not stored");
    value(update(guest.id,0,t+72000,1),"guest route");
    auto note=value(db.save_note(guest.id,id,{"guest private note",1},t+72001),"note");
    const auto key="v1/"+hash(t)+".jpg";
    value(db.reserve_photo_cleanup(key,t+72002),"reserve");
    value(db.attach_photo(guest.id,id,note.id,note.written_at_ms,{key,t+72002},1,t+72002),"attach photo");
    value(db.delete_member_data(host.id,id,true,t+73000),"delete only own route");
    check(value(db.get_route(host.id,id,"",0,500,t+73000),"own deleted").points.empty(),"own route erased");
    check(value(db.get_route(guest.id,id,"",0,500,t+73000),"other kept").points.size()==1,"other route retained");
    value(update(host.id,30,t+74000,6),"continue recording after own route deletion");
    value(db.leave_room(guest.id,id,t+75000),"leave retains private data");
    error(db.list_visible_members(guest.id,id,t+75000),ErrorCode::forbidden,"departed member loses room access");
    check(value(db.get_route(guest.id,id,"",0,500,t+75000),"departed own route").points.size()==1,"departed can access own retained route");
    value(db.set_sharing(host.id,id,false,t+76000),"all off");
    auto off=value(db.list_visible_members(host.id,id,t+76000),"off views");
    check(off.size()==1&&!off[0].location&&!off[0].note&&!off[0].device_status,"off hides private fields");
    value(db.collect_expired_data(t+76000+kRetentionMs-1),"before deadline");
    check(!value(db.get_route(host.id,id,"",0,500,t),"still retained").points.empty(),"not early");
    value(db.collect_expired_data(t+76000+kRetentionMs),"at deadline");
    check(value(db.get_route(host.id,id,"",0,500,t),"current expired").points.empty(),"current data expires at 48h");
    check(value(db.get_route(guest.id,id,"",0,500,t),"left retained").points.size()==1,"left data waits for room end");
    check(value(db.due_photo_cleanup(t+76000+kRetentionMs),"left photo retained").empty(),"left photo not deleted early");
    const auto ended=t+76000+kRetentionMs+1000;
    value(db.end_room(host.id,id,ended),"end");
    value(db.collect_expired_data(ended+kRetentionMs-1),"before end deadline");
    check(!value(db.get_route(guest.id,id,"",0,500,t),"left before end deadline").points.empty(),"48h end grace");
    value(db.collect_expired_data(ended+kRetentionMs),"end deadline");
    error(db.get_route(guest.id,id,"",0,500,t),ErrorCode::not_found,"ended room purged");
    auto queued=value(db.due_photo_cleanup(ended+kRetentionMs),"queued photo");
    check(std::find(queued.begin(),queued.end(),key)!=queued.end(),"photo object queued for physical deletion");
    value(db.finish_photo_cleanup(key),"acknowledge photo removal");

    auto r2=value(db.create_room(host.id,"resume","host","EFGH45",t),"second room");
    id=r2.room.id;
    value(db.set_sharing(host.id,id,true,t),"share");
    value(update(host.id,0,t,1),"route");
    value(db.set_sharing(host.id,id,false,t+1000),"off");
    const auto resumed=t+1000+kRetentionMs-1;
    value(db.set_sharing(host.id,id,true,resumed),"resume");
    value(update(host.id,0,resumed,1,2),"fresh epoch point");
    value(db.collect_expired_data(t+1000+kRetentionMs),"cancelled timer");
    check(value(db.get_route(host.id,id,"",0,500,t),"resume retains").points.size()==2,"resumption cancels expiry");
    const auto idle_deadline=resumed+kRoomInactivityMs+kRetentionMs;
    value(db.create_mark(host.id,id,{"does not renew location activity",{0,0}},idle_deadline-1),"mark during idle");
    value(db.collect_expired_data(resumed+kRoomInactivityMs+kRetentionMs),"inactive deadline");
    check(value(db.get_route(host.id,id,"",0,500,t),"inactivity clears").points.empty(),"idle even if switch remains on");
    check(value(db.list_marks(host.id,id),"idle marks").empty(),"mark editing does not extend retention");

    auto gm=value(db.join_room(guest.id,id,"guest",t),"second join");
    value(db.set_sharing(guest.id,id,true,t),"second share");
    value(update(guest.id,0,t+1,1),"other route before delete");
    value(db.create_mark(host.id,id,{"host mark",{0,0}},t+2),"host mark before delete");
    value(db.create_mark(guest.id,id,{"guest mark",{0,0}},t+3),"guest mark before delete");
    value(db.delete_user(host.id,t+4),"delete user");
    error(db.authenticate_guest(hhash,t+5),ErrorCode::unauthenticated,"all deleted user sessions invalid");
    error(db.get_profile(host.id),ErrorCode::unauthenticated,"users row removed");
    check(value(db.get_room(guest.id,id),"successor room").host_id==guest.id,"host transferred without deleting others");
    check(value(db.list_marks(guest.id,id),"remaining marks").size()==1,"only own marks deleted");
    check(value(db.get_route(guest.id,id,"",0,500,t),"remaining route").points.size()==1,"other user data survives");
    value(db.delete_member_data(guest.id,id,false,t+6),"delete own room data");
    check(value(db.get_route(guest.id,id,"",0,500,t),"deleted own data").points.empty(),"personal erasure");
    check(value(db.get_profile(guest.id),"account remains").id==guest.id,"room-data deletion preserves user");
    value(db.delete_user(guest.id,t+7),"delete last host");
    value(db.delete_user(outsider.id,t+7),"delete outsider");

    // Ending a room must not postpone an earlier sharing-OFF deletion deadline.
    auto late=value(db.create_guest(hash(t*100+5),t,guest_session_expiry(t)),"late host");
    auto r3=value(db.create_room(late.id,"earlier deadline","host","JKLM67",t),"deadline room");
    id=r3.room.id;
    value(db.set_sharing(late.id,id,true,t),"deadline sharing");
    value(update(late.id,0,t,1),"deadline route");
    value(db.set_sharing(late.id,id,false,t+1000),"deadline off");
    value(db.end_room(late.id,id,t+2000),"end later");
    value(db.collect_expired_data(t+1000+kRetentionMs-1),"before earlier deadline");
    check(value(db.get_route(late.id,id,"",0,500,t),"ended but retained").points.size()==1,"retain until earliest deadline");
    value(db.collect_expired_data(t+1000+kRetentionMs),"earlier off deadline");
    check(value(db.get_route(late.id,id,"",0,500,t),"earlier purge").points.empty(),"end cannot extend privacy deadline");
    value(db.collect_expired_data(t+2000+kRetentionMs),"whole room expiry");
    error(db.get_route(late.id,id,"",0,500,t),ErrorCode::not_found,"room deleted at end deadline");
    value(db.delete_user(late.id,t+3000+kRetentionMs),"delete deadline fixture");
}
}
int main(){
    try{
        policies();
#ifdef GOMMANNA_TEST_REAL_DB
        if(!std::getenv("GOMMANNA_RUN_SHARED_DB_TESTS")||std::string(std::getenv("GOMMANNA_RUN_SHARED_DB_TESTS"))!="1"||
           !std::getenv("GOMMANNA_TEST_DATABASE_URL"))return 77;
        auto db=value(make_postgres_repository({std::getenv("GOMMANNA_TEST_DATABASE_URL")}),"connect");
#else
        auto db=make_fake_repository();
#endif
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        exercise(*db,now);
        std::cout<<"PASS: six-month sessions, profile, marks, device privacy, route sampling/paging, deletion and retention\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
