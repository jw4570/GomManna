#include "gommanna/contracts.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace gommanna;
using json = nlohmann::json;
namespace {
TimeMs now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
std::string hex(const unsigned char* bytes, size_t n) { static constexpr char chars[]="0123456789abcdef"; std::string s; s.reserve(n*2); for(size_t i=0;i<n;++i){s+=chars[bytes[i]>>4];s+=chars[bytes[i]&15];} return s; }
std::string sha256(std::string_view token) { unsigned char out[SHA256_DIGEST_LENGTH]; SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), out); return hex(out,sizeof out); }
std::string token() { unsigned char b[32]; if(RAND_bytes(b,sizeof b)!=1) throw std::runtime_error("secure random generation failed"); return hex(b,sizeof b); }
std::string env(const char* name, const char* fallback="") { const char* v=std::getenv(name); return v? v:fallback; }
std::string code(ErrorCode c) { switch(c) {
case ErrorCode::invalid_argument:return "INVALID_ARGUMENT"; case ErrorCode::unauthenticated:return "UNAUTHENTICATED";case ErrorCode::forbidden:return "FORBIDDEN";case ErrorCode::not_found:return "NOT_FOUND";case ErrorCode::conflict:return "CONFLICT";case ErrorCode::invite_code_conflict:return "INVITE_CODE_CONFLICT";case ErrorCode::room_closed:return "ROOM_CLOSED";case ErrorCode::sharing_off:return "SHARING_OFF";case ErrorCode::stale_update:return "STALE_UPDATE";case ErrorCode::db_unavailable:return "DB_UNAVAILABLE";case ErrorCode::not_implemented:return "NOT_IMPLEMENTED";default:return "INTERNAL";}}
int status(ErrorCode c) {switch(c){case ErrorCode::invalid_argument:return 400;case ErrorCode::unauthenticated:return 401;case ErrorCode::forbidden:return 403;case ErrorCode::not_found:return 404;case ErrorCode::conflict:case ErrorCode::invite_code_conflict:case ErrorCode::sharing_off:case ErrorCode::stale_update:return 409;case ErrorCode::room_closed:return 410;case ErrorCode::not_implemented:return 501;case ErrorCode::db_unavailable:return 503;default:return 500;}}
void respond(httplib::Response& r, int status_code, const json& body) {r.status=status_code;r.set_content(body.dump(),"application/json; charset=utf-8");}
void fail(httplib::Response& r, Error e) { // never return internal DB messages to clients
  std::string msg = (e.code==ErrorCode::internal || e.code==ErrorCode::db_unavailable)?"Service temporarily unavailable or failed":e.message;
  respond(r,status(e.code),{{"ok",false},{"error",{{"code",code(e.code)},{"message",msg}}}});
}
void success(httplib::Response& r, json data, int s=200){respond(r,s,{{"ok",true},{"data",std::move(data)}});}
template<class T,class F> void output(httplib::Response& r, Result<T> result,F convert,int s=200){if(auto* e=std::get_if<Error>(&result)) fail(r,*e);else success(r,convert(std::get<T>(result)),s);}
json room(const Room& x){return {{"room_id",x.id},{"title",x.title},{"host_id",x.host_id},{"invite_code",x.invite_code},{"status",x.active?"active":"ended"},{"created_at_ms",x.created_at_ms},{"ended_at_ms",x.ended_at_ms}};}
json member(const Member& x){return {{"member_id",x.id},{"room_id",x.room_id},{"user_id",x.user_id},{"nickname",x.nickname},{"is_sharing",x.is_sharing},{"sharing_epoch",x.sharing_epoch},{"joined_at_ms",x.joined_at_ms}};}
json location(const Location& x){return {{"latitude",x.coordinate.latitude},{"longitude",x.coordinate.longitude},{"accuracy_m",x.accuracy_m},{"captured_at_ms",x.captured_at_ms},{"received_at_ms",x.received_at_ms},{"sharing_epoch",x.sharing_epoch},{"sequence",x.sequence}};}
json note(const Note& x,const Id& room_id,const Id& member_id){json origin=nullptr;if(x.origin)origin={{"latitude",x.origin->latitude},{"longitude",x.origin->longitude},{"captured_at_ms",x.origin_captured_at_ms}};return {{"note_id",x.id},{"text",x.text},{"written_at_ms",x.written_at_ms},{"origin",origin},{"sharing_epoch",x.sharing_epoch},{"photo_available",x.photo.has_value()},{"photo_uploaded_at_ms",x.photo?json(x.photo->uploaded_at_ms):json(nullptr)},{"photo_path",x.photo && !member_id.empty()?json("/v1/rooms/"+room_id+"/members/"+member_id+"/photo"):json(nullptr)}};}
std::string freshness(Freshness x){switch(x){case Freshness::off:return "off";case Freshness::no_data:return "no_data";case Freshness::fresh:return "fresh";case Freshness::delayed:return "delayed";default:return "stale";}}
json visible(const MemberView& v){json j=member(v.member);j["location_status"]=freshness(v.location_status); // Defensive suppression; DB must also enforce visibility.
if(!v.member.is_sharing || v.location_status==Freshness::off){j["location"]=nullptr;j["note"]=nullptr;return j;}
j["location"]=v.location_status!=Freshness::no_data && v.location && v.location->sharing_epoch==v.member.sharing_epoch?json(location(*v.location)):json(nullptr);
j["note"]=v.note && v.note->sharing_epoch==v.member.sharing_epoch?json(note(*v.note,v.member.room_id,v.member.id)):json(nullptr);return j;}
bool utf8_length(const std::string& s,size_t& count){count=0;for(size_t i=0;i<s.size();){unsigned char c=s[i];int n=c<128?1:(c>=0xC2&&c<=0xDF?2:(c>=0xE0&&c<=0xEF?3:(c>=0xF0&&c<=0xF4?4:0)));if(!n||i+n>s.size())return false;unsigned cp=c & (n==1?127:(n==2?31:(n==3?15:7)));for(int k=1;k<n;k++){unsigned char d=s[i+k];if((d&0xC0)!=0x80)return false;cp=(cp<<6)|(d&63);}if((n==2&&cp<128)||(n==3&&cp<2048)||(n==4&&cp<65536)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))return false;i+=n;++count;}return true;}
std::string trim(std::string s){size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?"":s.substr(a,b-a+1);}
std::string bounded(const json& j,const char* key,size_t max){if(!j.at(key).is_string())throw std::invalid_argument(std::string(key)+" must be a string");auto s=trim(j.at(key).get<std::string>());size_t n;if(!utf8_length(s,n)||n<1||n>max)throw std::invalid_argument(std::string(key)+" has invalid length or UTF-8");return s;}
json body(const httplib::Request& req,std::initializer_list<const char*> keys){if(req.get_header_value("Content-Type").find("application/json")!=0)throw std::invalid_argument("Expected application/json");auto j=json::parse(req.body);if(!j.is_object()||j.size()!=keys.size())throw std::invalid_argument("Invalid JSON fields");for(auto k:keys)if(!j.contains(k))throw std::invalid_argument("Missing JSON field");return j;}
template<class T>T integer(const json& j,const char* key){if(!j.at(key).is_number_integer())throw std::invalid_argument(std::string(key)+" must be integer");return j.at(key).get<T>();}
bool json_boolean(const json& j,const char* key){if(!j.at(key).is_boolean())throw std::invalid_argument(std::string(key)+" must be boolean");return j.at(key).get<bool>();}
double number(const json& j,const char* key){if(!j.at(key).is_number())throw std::invalid_argument(std::string(key)+" must be number");auto x=j.at(key).get<double>();if(!std::isfinite(x))throw std::invalid_argument("Non-finite number");return x;}
using Handler=std::function<void(const httplib::Request&,httplib::Response&,const Id&)>;
void protect(httplib::Server& server,IRepository& repo,const std::string& method,const std::string& path,Handler handler){auto wrapped=[&repo,handler=std::move(handler)](const httplib::Request& req,httplib::Response& res){try{auto h=req.get_header_value("Authorization");if(h.rfind("Bearer ",0)!=0||h.size()!=71){fail(res,{ErrorCode::unauthenticated,"Missing or invalid bearer token"});return;}auto raw=h.substr(7);if(raw.find_first_not_of("0123456789abcdef")!=std::string::npos){fail(res,{ErrorCode::unauthenticated,"Invalid token"});return;}auto auth=repo.authenticate_guest(sha256(raw),now_ms());if(auto* e=std::get_if<Error>(&auth)){fail(res,*e);return;}handler(req,res,std::get<User>(auth).id);}catch(const std::exception& e){if(dynamic_cast<const std::invalid_argument*>(&e)||dynamic_cast<const json::exception*>(&e)||dynamic_cast<const std::out_of_range*>(&e))fail(res,{ErrorCode::invalid_argument,"Invalid request body or field"});else fail(res,{ErrorCode::internal,"Unexpected server failure"});}};
if(method=="GET")server.Get(path,wrapped);else if(method=="POST")server.Post(path,wrapped);else if(method=="PUT")server.Put(path,wrapped);else if(method=="DELETE")server.Delete(path,wrapped);}
}
namespace gommanna { std::unique_ptr<IRoomService> make_fake_room_service(IRepository&); }
int main(){try{
#ifdef GOMMANNA_USE_FAKE_REPOSITORY
std::cerr<<"WARNING: FAKE DATA mode. NOT a Supabase integration test.\n";
auto repo=make_fake_repository();
#else
std::string conn=env("GOMMANNA_DATABASE_URL");if(conn.empty())throw std::runtime_error("GOMMANNA_DATABASE_URL missing");auto created=make_postgres_repository({conn});if(auto* e=std::get_if<Error>(&created))throw std::runtime_error("DB initialization failed: "+code(e->code));auto repo=std::move(std::get<std::unique_ptr<IRepository>>(created));
#endif
#ifdef GOMMANNA_USE_FAKE_REPOSITORY
auto rooms=make_fake_room_service(*repo);
#else
auto rooms=make_room_service(*repo);
#endif
if(!rooms)throw std::runtime_error("Room service initialization failed");
httplib::Server server;server.Get("/health",[](const auto&,auto& r){success(r,{{"status","ok"}});});
server.Post("/v1/guests",[&](const auto& req,auto& r){try{body(req,{});auto t=token();auto now=now_ms();auto expires=now+kGuestSessionTtlMs;auto result=repo->create_guest(sha256(t),now,expires);output(r,std::move(result),[&](const User& u)->json{return {{"user_id",u.id},{"token",t},{"expires_at_ms",expires}};},201);}catch(const std::invalid_argument&){fail(r,{ErrorCode::invalid_argument,"Invalid guest request"});}catch(const json::exception&){fail(r,{ErrorCode::invalid_argument,"Invalid guest request"});}catch(const std::exception&){fail(r,{ErrorCode::internal,"Guest creation failed"});}});
const std::string base=R"(/v1/rooms/([0-9a-fA-F-]{36}))";
protect(server,*repo,"POST","/v1/rooms",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"title","nickname"});auto result=rooms->create_room(actor,bounded(j,"title",60),bounded(j,"nickname",20),now_ms());output(r,std::move(result),[](const RoomMembership& m)->json{return {{"room",room(m.room)},{"member",member(m.member)}};},201);});
protect(server,*repo,"POST","/v1/rooms/join",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"invite_code","nickname"});auto result=rooms->join_room(actor,bounded(j,"invite_code",32),bounded(j,"nickname",20),now_ms());if(auto* e=std::get_if<Error>(&result)){fail(r,*e);return;}auto m=std::get<RoomMembership>(result);success(r,{{"room",room(m.room)},{"member",member(m.member)}});});
protect(server,*repo,"GET",base,[&](auto& req,auto& r,const Id& actor){output(r,repo->get_room(actor,req.matches[1]),[](const Room& x)->json{return {{"room",room(x)}};});});
protect(server,*repo,"GET",base+"/members",[&](auto& req,auto& r,const Id& actor){auto result=repo->list_visible_members(actor,req.matches[1],now_ms());output(r,std::move(result),[](const std::vector<MemberView>& v)->json{json arr=json::array();for(const auto& x:v)arr.push_back(visible(x));return {{"members",arr}};});});
protect(server,*repo,"GET",base+"/invite",[&](auto& req,auto& r,const Id& actor){output(r,rooms->get_invite(actor,req.matches[1]),[](const Invite& i)->json{return {{"invite_code",i.invite_code},{"qr_payload",i.qr_payload},{"qr_svg",i.qr_svg}};});});
protect(server,*repo,"POST",base+"/leave",[&](auto& req,auto& r,const Id& actor){body(req,{});output(r,rooms->leave_room(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"POST",base+"/end",[&](auto& req,auto& r,const Id& actor){body(req,{});output(r,rooms->end_room(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"PUT",base+"/sharing",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"enabled"});output(r,repo->set_sharing(actor,req.matches[1],json_boolean(j,"enabled"),now_ms()),[](const Member& x)->json{return {{"member",member(x)}};});});
protect(server,*repo,"PUT",base+"/location",[&](auto& req,auto& r,const Id& actor){auto j=json::parse(req.body);if(req.get_header_value("Content-Type").find("application/json")!=0||!j.is_object()||j.size()<5||j.size()>6)throw std::invalid_argument("Invalid JSON fields");for(const char* k:{"latitude","longitude","captured_at_ms","sharing_epoch","sequence"})if(!j.contains(k))throw std::invalid_argument("Missing JSON field");for(auto it=j.begin();it!=j.end();++it)if(it.key()!="latitude"&&it.key()!="longitude"&&it.key()!="accuracy_m"&&it.key()!="captured_at_ms"&&it.key()!="sharing_epoch"&&it.key()!="sequence")throw std::invalid_argument("Unexpected JSON field");LocationUpdate u;u.coordinate={number(j,"latitude"),number(j,"longitude")};if(std::abs(u.coordinate.latitude)>90||std::abs(u.coordinate.longitude)>180)throw std::invalid_argument("Invalid coordinates");if(j.contains("accuracy_m")&&!j.at("accuracy_m").is_null()){u.accuracy_m=number(j,"accuracy_m");if(*u.accuracy_m<0)throw std::invalid_argument("Invalid accuracy");}u.captured_at_ms=integer<TimeMs>(j,"captured_at_ms");u.sharing_epoch=integer<Epoch>(j,"sharing_epoch");u.sequence=integer<std::int64_t>(j,"sequence");if(u.sharing_epoch<0||u.sequence<1||u.captured_at_ms>now_ms()+kFutureToleranceMs)throw std::invalid_argument("Invalid timestamp or sequence");output(r,repo->update_location(actor,req.matches[1],u,now_ms()),[](const Location& x)->json{return {{"location",location(x)}};});});
protect(server,*repo,"PUT",base+"/note",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"text","sharing_epoch"});NoteDraft d{bounded(j,"text",200),integer<Epoch>(j,"sharing_epoch")};if(d.sharing_epoch<0)throw std::invalid_argument("Invalid epoch");output(r,repo->save_note(actor,req.matches[1],d,now_ms()),[&](const Note& n)->json{return {{"note",note(n,req.matches[1],"")}};});});
protect(server,*repo,"DELETE",base+"/note",[&](auto& req,auto& r,const Id& actor){output(r,repo->delete_note(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"PUT",base+"/note/photo",[](auto&,auto& r,const Id&){fail(r,{ErrorCode::not_implemented,"Photo upload not implemented"});});
protect(server,*repo,"GET",base+R"(/members/([0-9a-fA-F-]{36})/photo)",[](auto&,auto& r,const Id&){fail(r,{ErrorCode::not_implemented,"Photo retrieval not implemented"});});
const auto host=env("GOMMANNA_HOST","127.0.0.1");int port=std::stoi(env("GOMMANNA_PORT","8080"));if(port<1||port>65535)throw std::runtime_error("Invalid port");std::cerr<<"GomManna listening on "<<host<<":"<<port<<"\n";if(!server.listen(host,port))throw std::runtime_error("Could not bind HTTP server");
}catch(const std::exception& e){std::cerr<<"Server startup failed: "<<e.what()<<"\n";return 1;}}
