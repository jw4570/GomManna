#include "gommanna/contracts.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace gommanna;

namespace {
void check(bool condition, const char* what) {
    if(!condition) throw std::runtime_error(what);
}
template<class T> T value(Result<T> result, const char* what) {
    if(auto* err=std::get_if<Error>(&result)) {
        std::cerr << "FAIL " << what << ": " << err->message << '\n';
        throw std::runtime_error(what);
    }
    return std::get<T>(std::move(result));
}
template<class T> void error(Result<T> result, ErrorCode expected, const char* what) {
    auto* e=std::get_if<Error>(&result);
    if(!e || e->code!=expected) throw std::runtime_error(what);
}
std::string hash_for(std::uint64_t n) {
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(64) << n; return out.str();
}
std::string code_for(std::uint64_t n) {
    constexpr char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code(8,'A');
    for(char& c : code) { c=alphabet[n%32]; n/=32; }
    return code;
}
const MemberView& for_user(const std::vector<MemberView>& members, const Id& user) {
    for(const auto& member : members) if(member.member.user_id==user) return member;
    throw std::runtime_error("user missing from member list");
}
} // namespace

int main() {
    const char* run=std::getenv("GOMMANNA_RUN_DB_INTEGRATION");
    const char* url=std::getenv("GOMMANNA_TEST_DATABASE_URL");
    if(!run || std::string(run)!="1" || !url || !*url) {
        std::cout << "SKIP: set GOMMANNA_RUN_DB_INTEGRATION=1 and GOMMANNA_TEST_DATABASE_URL for a disposable test database.\n";
        return 77;
    }
    try {
        auto repository=value(make_postgres_repository(RepositoryConfig{url}),"connect to PostgreSQL");
        const TimeMs now=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto seed=static_cast<std::uint64_t>(now);
        User host=value(repository->create_guest(hash_for(seed*10+1),now,now+kGuestSessionTtlMs),"create host");
        User guest=value(repository->create_guest(hash_for(seed*10+2),now,now+kGuestSessionTtlMs),"create guest");
        User outsider=value(repository->create_guest(hash_for(seed*10+3),now,now+kGuestSessionTtlMs),"create outsider");
        check(value(repository->authenticate_guest(hash_for(seed*10+1),now),"authenticate").id==host.id,"authenticate identity");
        error(repository->authenticate_guest(hash_for(seed*10+1),now+kGuestSessionTtlMs),ErrorCode::unauthenticated,"expired session");
        error(repository->authenticate_guest("bad token",now),ErrorCode::unauthenticated,"invalid session");
        auto room=value(repository->create_room(host.id,"DB test room","이안",code_for(seed),now),"create room");
        check(!room.member.is_sharing && room.member.sharing_epoch==0,"default sharing off");
        error(repository->create_room(guest.id,"duplicate code","서영",code_for(seed),now),
              ErrorCode::invite_code_conflict,"invite conflict");
        error(repository->get_room(outsider.id,room.room.id),ErrorCode::forbidden,"outsider cannot view room");
        check(value(repository->find_room_by_code(room.room.invite_code),"find invite").id==room.room.id,"find invite id");
        auto second=value(repository->join_room(guest.id,room.room.id,"서영",now),"join room");
        check(value(repository->join_room(guest.id,room.room.id,"다른 이름",now),"idempotent join").id==second.id,
              "join should not duplicate member");
        error(repository->end_room(guest.id,room.room.id,now),ErrorCode::forbidden,"only host can end");
        auto on=value(repository->set_sharing(host.id,room.room.id,true,now),"sharing on");
        check(on.sharing_epoch==1 && on.is_sharing,"first sharing epoch");
        auto empty_note=value(repository->save_note(host.id,room.room.id,NoteDraft{"위치 없는 메모",1},now),"note without location");
        check(!empty_note.origin,"note may have no location");
        const Coordinate first{37.566567891,126.978123456};
        LocationUpdate loc{first,12.5,now,1,1};
        auto stored=value(repository->update_location(host.id,room.room.id,loc,now),"save location");
        check(std::abs(stored.coordinate.latitude-first.latitude)<1e-9,"location precision");
        error(repository->update_location(host.id,room.room.id,loc,now),ErrorCode::stale_update,"old sequence rejected");
        auto invalid=loc; invalid.coordinate.latitude=91;
        error(repository->update_location(host.id,room.room.id,invalid,now),ErrorCode::invalid_argument,"invalid coordinate rejected");
        auto note=value(repository->save_note(host.id,room.room.id,NoteDraft{"3층 에스컬레이터 옆",1},now+1000),"save note");
        check(note.id==empty_note.id && note.origin && std::abs(note.origin->latitude-first.latitude)<1e-9,
              "note captures current location and retains id");
        loc.coordinate={37.565,126.979}; loc.sequence=2; loc.captured_at_ms=now+2000;
        value(repository->update_location(host.id,room.room.id,loc,now+2000),"move location");
        auto visible=value(repository->list_visible_members(guest.id,room.room.id,now+3000),"view shared data");
        const auto& before_off=for_user(visible,host.id);
        check(before_off.location && before_off.note && before_off.note->origin &&
              std::abs(before_off.note->origin->latitude-first.latitude)<1e-9,"note origin remains unchanged");
        value(repository->set_sharing(host.id,room.room.id,false,now+4000),"sharing off");
        auto hidden=value(repository->list_visible_members(guest.id,room.room.id,now+4000),"view after off");
        const auto& off=for_user(hidden,host.id);
        check(off.location_status==Freshness::off && !off.location && !off.note,"off hides old location and note");
        error(repository->update_location(host.id,room.room.id,loc,now+4000),ErrorCode::sharing_off,"off rejects update");
        error(repository->save_note(host.id,room.room.id,NoteDraft{"비공개",1},now+4000),ErrorCode::sharing_off,"off rejects note");
        auto resumed=value(repository->set_sharing(host.id,room.room.id,true,now+5000),"sharing resumes");
        check(resumed.sharing_epoch==2,"epoch increments");
        auto cleared=value(repository->list_visible_members(guest.id,room.room.id,now+5000),"view new epoch");
        const auto& no_data=for_user(cleared,host.id);
        check(no_data.location_status==Freshness::no_data && !no_data.location && !no_data.note,"old data stays hidden on resume");
        error(repository->update_location(host.id,room.room.id,loc,now+5000),ErrorCode::stale_update,"old epoch rejected");
        loc.sharing_epoch=2; loc.sequence=1; loc.captured_at_ms=now+5000;
        value(repository->update_location(host.id,room.room.id,loc,now+5000),"new epoch sequence one");
        value(repository->save_note(host.id,room.room.id,NoteDraft{"새 메모",2},now+6000),"new note");
        value(repository->delete_note(host.id,room.room.id,now+7000),"delete note");
        auto deleted=value(repository->list_visible_members(guest.id,room.room.id,now+7000),"view deleted note");
        check(!for_user(deleted,host.id).note,"note deleted");
        value(repository->leave_room(guest.id,room.room.id,now+8000),"member leaves");
        error(repository->get_room(guest.id,room.room.id),ErrorCode::forbidden,"left member blocked");
        value(repository->end_room(host.id,room.room.id,now+9000),"host ends room");
        value(repository->end_room(host.id,room.room.id,now+9000),"ending is idempotent");
        error(repository->get_room(host.id,room.room.id),ErrorCode::room_closed,"closed room blocked");
        std::cout << "PASS: Supabase repository integration (guest, rooms, privacy, locations, notes, epochs)\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
