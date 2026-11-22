// Run only against an explicitly configured isolated database.
#include "gommanna/contracts.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
using namespace gommanna;
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
template<class T>T value(Result<T> r){if(auto* e=std::get_if<Error>(&r))throw std::runtime_error(e->message);return std::get<T>(std::move(r));}
template<class T>void denied(Result<T> r,ErrorCode code){check(std::holds_alternative<Error>(r)&&std::get<Error>(r).code==code,"unexpected access result");}
std::string hash(TimeMs n){std::ostringstream s;s<<std::hex<<std::setw(64)<<std::setfill('0')<<n;return s.str();}
int main(){
 const auto url=std::getenv("GOMMANNA_TEST_DATABASE_URL");
 if(!url || std::string(std::getenv("GOMMANNA_RUN_DB_INTEGRATION")?std::getenv("GOMMANNA_RUN_DB_INTEGRATION"):"")!="1")return 77;
 try {
 auto db=value(make_postgres_repository({url}));
 auto t=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
 auto host=value(db->create_guest(hash(t*10),t,t+kGuestSessionTtlMs));
 auto guest=value(db->create_guest(hash(t*10+1),t,t+kGuestSessionTtlMs));
 auto outsider=value(db->create_guest(hash(t*10+2),t,t+kGuestSessionTtlMs));
 auto departed=value(db->create_guest(hash(t*10+3),t,t+kGuestSessionTtlMs));
 auto room=value(db->create_room(host.id,"trip test","host","TRP234",t));auto rid=room.room.id;
 auto gm=value(db->join_room(guest.id,rid,"guest",t+1));
 value(db->join_room(departed.id,rid,"departed",t+1));
 value(db->set_sharing(host.id,rid,true,t+2));value(db->set_sharing(guest.id,rid,true,t+2));
 value(db->update_location(guest.id,rid,{{37.5,127},5,t+3,1,1},t+3));
 auto n=value(db->save_note(guest.id,rid,{"first",1},t+4));
 std::string key="v1/"+std::string(64,'a')+".jpg";
 value(db->reserve_photo_cleanup(key,t+4));value(db->attach_photo(guest.id,rid,n.id,n.written_at_ms,{key,t+5},1,t+5));
 value(db->save_note(guest.id,rid,{"second",1},t+4));
 check(value(db->due_photo_cleanup(t+7)).empty(),"historical photo must not be collected");
 check(value(db->list_trips(host.id,0,20,t+7)).empty(),"active room is not a recent trip");
 denied(db->get_trip(host.id,rid,t+7),ErrorCode::conflict);
 value(db->set_trip_sharing(guest.id,rid,true,t+7));
 value(db->leave_room(departed.id,rid,t+8));value(db->end_room(host.id,rid,t+9));
 auto recent=value(db->list_trips(host.id,0,20,t+10));check(recent.size()==1,"trip discovery");
 check(recent[0].expires_at_ms==t+9+kRetentionMs,"server computes expiry");
 auto detail=value(db->get_trip(host.id,rid,t+10));check(detail.members.size()==2,"departed members hidden from others");
 denied(db->get_trip(outsider.id,rid,t+10),ErrorCode::forbidden);
 check(value(db->list_trips(outsider.id,0,20,t+10)).empty(),"outsider trip list");
 auto route=value(db->get_trip_route(host.id,rid,gm.id,0,1,t+10));check(route.points.size()==1,"history works after live sharing OFF at end");
 denied(db->get_route(host.id,rid,gm.id,0,500,t+10),ErrorCode::forbidden);
 denied(db->get_trip_route(departed.id,rid,gm.id,0,500,t+10),ErrorCode::forbidden);
 auto page=value(db->get_trip_notes(host.id,rid,0,1,t+10));check(page.notes.size()==1&&page.next_after_id.has_value(),"notes pagination");
 auto record=page.notes[0].record_id;check(value(db->get_trip_photo(host.id,rid,record,t+10)).key==key,"historical photo ref");
 check(value(db->get_trip_notes(host.id,rid,*page.next_after_id,1,t+10)).notes[0].note.text=="second","note revision retained");
 value(db->set_trip_sharing(guest.id,rid,false,t+11));
 check(value(db->get_trip_notes(host.id,rid,0,200,t+12)).notes.empty(),"revocation hides notes immediately");
 denied(db->get_trip_photo(host.id,rid,record,t+12),ErrorCode::forbidden);
 denied(db->get_trip_route(host.id,rid,gm.id,0,500,t+12),ErrorCode::forbidden);
 check(value(db->get_trip_notes(guest.id,rid,0,200,t+12)).notes.size()==2,"owner keeps own records");
 denied(db->delete_trip_note(host.id,rid,record,t+12),ErrorCode::not_found);
 value(db->delete_trip_note(guest.id,rid,record,t+13));
 check(value(db->get_trip_notes(guest.id,rid,0,200,t+14)).notes.size()==1,"delete old revision only");
 check(value(db->due_photo_cleanup(t+60000)).size()==1,"unreferenced historical photo is collectable");
 value(db->set_trip_sharing(guest.id,rid,true,t+15));
 value(db->delete_member_data(guest.id,rid,false,t+16));
 check(value(db->get_trip_notes(guest.id,rid,0,200,t+17)).notes.empty(),"personal erasure deletes history");
 denied(db->get_trip_route(host.id,rid,gm.id,0,500,t+17),ErrorCode::forbidden);
 denied(db->get_trip(host.id,rid,t+9+kRetentionMs),ErrorCode::not_found);
 check(value(db->list_trips(host.id,0,20,t+9+kRetentionMs)).empty(),"expired trips hidden before cleanup");
 value(db->collect_expired_data(t+9+kRetentionMs));
 denied(db->get_trip(host.id,rid,t+10+kRetentionMs),ErrorCode::not_found);
 for(const auto& u:{host,guest,outsider,departed})value(db->delete_user(u.id,t+11+kRetentionMs));
 std::cout<<"PASS: trip discovery, revisions, photo lifecycle, opt-in/revocation, access control, erasure and expiry\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
