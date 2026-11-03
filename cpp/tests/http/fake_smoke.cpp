#include "gommanna/contracts.hpp"
#include <cassert>
#include <iostream>
using namespace gommanna;
namespace gommanna {std::unique_ptr<IRoomService> make_fake_room_service(IRepository&);}
int main(){
 auto db=make_fake_repository();auto service=make_fake_room_service(*db);
 auto a=std::get<User>(db->create_guest("hash1",1000,999999));
 auto b=std::get<User>(db->create_guest("hash2",1000,999999));
 auto r=std::get<RoomMembership>(service->create_room(a.id,"Travel","A",1000));
 auto j=std::get<RoomMembership>(service->join_room(b.id,r.room.invite_code,"B",1000));
 assert(j.room.id==r.room.id);
 auto on=std::get<Member>(db->set_sharing(a.id,r.room.id,true,2000));assert(on.sharing_epoch==1);
 LocationUpdate update{{37.5,127.0},10.0,2000,1,1};assert(std::holds_alternative<Location>(db->update_location(a.id,r.room.id,update,2100)));
 assert(std::holds_alternative<Error>(db->update_location(a.id,r.room.id,update,2200)));
 auto note=std::get<Note>(db->save_note(a.id,r.room.id,{"3rd floor",1},2200));assert(note.origin.has_value());
 auto list=std::get<std::vector<MemberView>>(db->list_visible_members(b.id,r.room.id,2300));assert(list.size()==2);
 auto off=std::get<Member>(db->set_sharing(a.id,r.room.id,false,2400));assert(!off.is_sharing);
 list=std::get<std::vector<MemberView>>(db->list_visible_members(b.id,r.room.id,2500));for(auto& v:list)if(v.member.user_id==a.id){assert(!v.location.has_value());assert(!v.note.has_value());}
 on=std::get<Member>(db->set_sharing(a.id,r.room.id,true,2600));assert(on.sharing_epoch==2);
 assert(std::holds_alternative<Error>(db->update_location(a.id,r.room.id,update,2700)));
 update.sharing_epoch=2;assert(std::holds_alternative<Location>(db->update_location(a.id,r.room.id,update,2800)));
 assert(std::holds_alternative<Unit>(service->end_room(a.id,r.room.id,3000)));
 assert(std::holds_alternative<Error>(db->get_room(b.id,r.room.id)));
 std::cout<<"PASS: fake repository smoke test (NOT Supabase)\n";
}
