#include "gommanna/contracts.hpp"
#include "gommanna/data_policy.hpp"
#include "environment.hpp"
#include "photo_service.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_map>
using namespace gommanna;
using json = nlohmann::json;
namespace {
TimeMs now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
std::string hex(const unsigned char* bytes, size_t n) { static constexpr char chars[]="0123456789abcdef"; std::string s; s.reserve(n*2); for(size_t i=0;i<n;++i){s+=chars[bytes[i]>>4];s+=chars[bytes[i]&15];} return s; }
std::string sha256(std::string_view token) { unsigned char out[SHA256_DIGEST_LENGTH]; SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), out); return hex(out,sizeof out); }
std::string token() { unsigned char b[32]; if(RAND_bytes(b,sizeof b)!=1) throw std::runtime_error("secure random generation failed"); return hex(b,sizeof b); }
http_config::Values file_environment;
std::string env(const char* name, const char* fallback="") { return http_config::get(file_environment,name,fallback); }
std::string code(ErrorCode c) { switch(c) {
case ErrorCode::invalid_argument:return "INVALID_ARGUMENT"; case ErrorCode::unauthenticated:return "UNAUTHENTICATED";case ErrorCode::forbidden:return "FORBIDDEN";case ErrorCode::not_found:return "NOT_FOUND";case ErrorCode::conflict:return "CONFLICT";case ErrorCode::invite_code_conflict:return "INVITE_CODE_CONFLICT";case ErrorCode::room_closed:return "ROOM_CLOSED";case ErrorCode::sharing_off:return "SHARING_OFF";case ErrorCode::stale_update:return "STALE_UPDATE";case ErrorCode::db_unavailable:return "DB_UNAVAILABLE";case ErrorCode::storage_unavailable:return "STORAGE_UNAVAILABLE";case ErrorCode::photo_too_large:return "PHOTO_TOO_LARGE";case ErrorCode::not_implemented:return "NOT_IMPLEMENTED";default:return "INTERNAL";}}
int status(ErrorCode c) {switch(c){case ErrorCode::invalid_argument:return 400;case ErrorCode::unauthenticated:return 401;case ErrorCode::forbidden:return 403;case ErrorCode::not_found:return 404;case ErrorCode::conflict:case ErrorCode::invite_code_conflict:case ErrorCode::sharing_off:case ErrorCode::stale_update:return 409;case ErrorCode::room_closed:return 410;case ErrorCode::photo_too_large:return 413;case ErrorCode::not_implemented:return 501;case ErrorCode::db_unavailable:case ErrorCode::storage_unavailable:return 503;default:return 500;}}
void respond(httplib::Response& r, int status_code, const json& body) {r.status=status_code;r.set_header("Cache-Control","no-store");r.set_content(body.dump(),"application/json; charset=utf-8");}
void fail(httplib::Response& r, Error e) { // never return internal DB messages to clients
  std::string msg = (e.code==ErrorCode::internal || e.code==ErrorCode::db_unavailable || e.code==ErrorCode::storage_unavailable)?"Service temporarily unavailable or failed":e.message;
  respond(r,status(e.code),{{"ok",false},{"error",{{"code",code(e.code)},{"message",msg}}}});
}
void success(httplib::Response& r, json data, int s=200){respond(r,s,{{"ok",true},{"data",std::move(data)}});}
class RateLimiter {
  using Clock = std::chrono::steady_clock;
  std::mutex mutex_;
  std::unordered_map<std::string, std::deque<Clock::time_point>> requests_;
 public:
  bool allow(const std::string& client, size_t maximum) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto cutoff = Clock::now() - std::chrono::minutes(1);
    if (requests_.size() >= 4096 && !requests_.contains(client)) {
      for (auto it = requests_.begin(); it != requests_.end();) {
        if (it->second.empty() || it->second.back() <= cutoff) it = requests_.erase(it);
        else ++it;
      }
      if (requests_.size() >= 4096) return false;
    }
    auto& times = requests_[client];
    while (!times.empty() && times.front() <= cutoff) times.pop_front();
    if (times.size() >= maximum) return false;
    times.push_back(Clock::now());
    return true;
  }
};
void rate_limited(httplib::Response& r) {
  r.set_header("Retry-After", "60");
  respond(r, 429, {{"ok",false},{"error",{{"code","RATE_LIMITED"},{"message","Too many requests"}}}});
}
template<class T,class F> void output(httplib::Response& r, Result<T> result,F convert,int s=200){if(auto* e=std::get_if<Error>(&result)) fail(r,*e);else success(r,convert(std::get<T>(result)),s);}
json room(const Room& x){return {{"room_id",x.id},{"title",x.title},{"host_id",x.host_id},{"invite_code",x.invite_code},{"status",x.active?"active":"ended"},{"created_at_ms",x.created_at_ms},{"ended_at_ms",x.ended_at_ms}};}
json member(const Member& x){return {{"member_id",x.id},{"room_id",x.room_id},{"user_id",x.user_id},{"nickname",x.nickname},{"is_sharing",x.is_sharing},{"sharing_epoch",x.sharing_epoch},{"joined_at_ms",x.joined_at_ms}};}
json profile(const User& u){return {{"user_id",u.id},{"created_at_ms",u.created_at_ms},{"animal_icon",u.animal_icon}};}
json device_status(const DeviceStatus& d){return {{"battery_percent",d.battery_percent},{"is_charging",d.is_charging},
    {"updated_at_ms",d.updated_at_ms},{"sharing_epoch",d.sharing_epoch},
    {"status",now_ms()-d.updated_at_ms>=kStaleAfterMs?"stale":"recent"}};}
json map_mark(const MapMark& m){return {{"mark_id",m.id},{"room_id",m.room_id},{"creator_user_id",m.creator_id},
    {"name",m.name},{"latitude",m.coordinate.latitude},{"longitude",m.coordinate.longitude},
    {"created_at_ms",m.created_at_ms},{"updated_at_ms",m.updated_at_ms}};}
json location(const Location& x){return {{"latitude",x.coordinate.latitude},{"longitude",x.coordinate.longitude},{"accuracy_m",x.accuracy_m},{"captured_at_ms",x.captured_at_ms},{"received_at_ms",x.received_at_ms},{"sharing_epoch",x.sharing_epoch},{"sequence",x.sequence}};}
json note(const Note& x,const Id& room_id,const Id& member_id){json origin=nullptr;if(x.origin)origin={{"latitude",x.origin->latitude},{"longitude",x.origin->longitude},{"captured_at_ms",x.origin_captured_at_ms}};return {{"note_id",x.id},{"text",x.text},{"written_at_ms",x.written_at_ms},{"origin",origin},{"sharing_epoch",x.sharing_epoch},{"photo_available",x.photo.has_value()},{"photo_uploaded_at_ms",x.photo?json(x.photo->uploaded_at_ms):json(nullptr)},{"photo_path",x.photo && !member_id.empty()?json("/v1/rooms/"+room_id+"/members/"+member_id+"/photo"):json(nullptr)}};}
std::string freshness(Freshness x){switch(x){case Freshness::off:return "off";case Freshness::no_data:return "no_data";case Freshness::fresh:return "fresh";case Freshness::delayed:return "delayed";default:return "stale";}}
json visible(const MemberView& v){json j=member(v.member);j["location_status"]=freshness(v.location_status); // Defensive suppression; DB must also enforce visibility.
j["animal_icon"]=v.member.animal_icon;
j["device_status"]=v.member.is_sharing && v.device_status && v.device_status->sharing_epoch==v.member.sharing_epoch
    ?json(device_status(*v.device_status)):json(nullptr);
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
TimeMs form_integer(const std::string& input) {
  TimeMs value{};
  const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
  if (input.empty() || parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || value < 1)
    throw std::invalid_argument("Invalid multipart integer");
  return value;
}
std::string normalize_uuid(std::string value) {
  for (auto& c : value) if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
  static const std::regex format(R"(^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)");
  if (!std::regex_match(value, format)) throw std::invalid_argument("Invalid UUID");
  return value;
}
std::int64_t query_integer(const httplib::Request& req,const char* key,std::int64_t fallback) {
    if(!req.has_param(key)) return fallback;
    if(req.get_param_value_count(key)!=1) throw std::invalid_argument("Duplicate query parameter");
    const auto text=req.get_param_value(key);
    std::int64_t value{};
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<0)
        throw std::invalid_argument("Invalid page parameter");
    return value;
}
using Handler=std::function<void(const httplib::Request&,httplib::Response&,const Id&)>;
void protect(httplib::Server& server,IRepository& repo,const std::string& method,const std::string& path,Handler handler){auto wrapped=[&repo,handler=std::move(handler)](const httplib::Request& req,httplib::Response& res){try{auto h=req.get_header_value("Authorization");if(h.rfind("Bearer ",0)!=0||h.size()!=71){fail(res,{ErrorCode::unauthenticated,"Missing or invalid bearer token"});return;}auto raw=h.substr(7);if(raw.find_first_not_of("0123456789abcdef")!=std::string::npos){fail(res,{ErrorCode::unauthenticated,"Invalid token"});return;}auto auth=repo.authenticate_guest(sha256(raw),now_ms());if(auto* e=std::get_if<Error>(&auth)){fail(res,*e);return;}const auto& user=std::get<User>(auth);if(user.expires_at_ms>0)res.set_header("X-Session-Expires-At-Ms",std::to_string(user.expires_at_ms));handler(req,res,user.id);}catch(const std::exception& e){if(dynamic_cast<const std::invalid_argument*>(&e)||dynamic_cast<const json::exception*>(&e)||dynamic_cast<const std::out_of_range*>(&e))fail(res,{ErrorCode::invalid_argument,"Invalid request body or field"});else fail(res,{ErrorCode::internal,"Unexpected server failure"});}};
if(method=="GET")server.Get(path,wrapped);else if(method=="POST")server.Post(path,wrapped);else if(method=="PUT")server.Put(path,wrapped);else if(method=="DELETE")server.Delete(path,wrapped);}
}
int main(){try{
file_environment=http_config::load_file();
#ifdef GOMMANNA_USE_FAKE_REPOSITORY
std::cerr<<"WARNING: FAKE DATA mode. NOT a Supabase integration test.\n";
auto repo=make_fake_repository();
#else
std::string conn=env("GOMMANNA_DATABASE_URL");if(conn.empty())throw std::runtime_error("GOMMANNA_DATABASE_URL missing");auto created=make_postgres_repository({conn});if(auto* e=std::get_if<Error>(&created))throw std::runtime_error("DB initialization failed: "+code(e->code));auto repo=std::move(std::get<std::unique_ptr<IRepository>>(created));
#endif
auto rooms=make_room_service(*repo);
if(!rooms)throw std::runtime_error("Room service initialization failed");
std::unique_ptr<PhotoStorage> photo_storage;
std::unique_ptr<PhotoService> photos;
const auto storage_url=env("GOMMANNA_STORAGE_URL"), storage_key=env("GOMMANNA_STORAGE_SECRET");
if(!storage_url.empty() || !storage_key.empty()) {
  auto configured=make_supabase_photo_storage({storage_url,storage_key,
      env("GOMMANNA_STORAGE_BUCKET","gommanna-photos"),env("GOMMANNA_STORAGE_CA_FILE")});
  if(std::holds_alternative<Error>(configured)) throw std::runtime_error("Invalid photo storage configuration");
  photo_storage=std::move(std::get<std::unique_ptr<PhotoStorage>>(configured));
  photos=std::make_unique<PhotoService>(*repo,*photo_storage);
}
std::mutex cleanup_mutex;
std::condition_variable_any cleanup_wakeup;
std::atomic<bool> cleanup_requested{false};
std::jthread cleanup_worker;
cleanup_worker=std::jthread([&](std::stop_token stop){
  while(!stop.stop_requested()) {
    try { if(std::holds_alternative<Error>(repo->collect_expired_data(now_ms()))) std::cerr<<"Retention cleanup will be retried.\n"; if(photos) photos->collect_garbage(now_ms()); }
    catch(const std::exception&) { std::cerr<<"Photo cleanup will be retried.\n"; }
    std::unique_lock lock(cleanup_mutex);
    cleanup_wakeup.wait_for(lock,stop,std::chrono::seconds(60),[&]{return cleanup_requested.exchange(false);});
  }
});
RateLimiter guest_requests, join_requests;
httplib::Server server;
server.set_payload_max_length(5 * 1024 * 1024 + 64 * 1024);
server.set_read_timeout(15, 0);
server.set_error_handler([](const auto&,auto& r){if(r.status==413)fail(r,{ErrorCode::photo_too_large,"request exceeds maximum size"});});
server.set_post_routing_handler([&](const auto& req,auto& res){
  if(res.status>=200 && res.status<300 &&
     ((req.method=="PUT" && (req.path.ends_with("/note") || req.path.ends_with("/note/photo"))) ||
      (req.method=="DELETE") ||
      (req.method=="POST" && (req.path.ends_with("/leave") || req.path.ends_with("/end"))))) {
    cleanup_requested.store(true);
    cleanup_wakeup.notify_one();
  }
});
server.Get("/health",[](const auto&,auto& r){success(r,{{"status","ok"}});});
server.Post("/v1/guests",[&](const auto& req,auto& r){try{if(!guest_requests.allow(req.remote_addr,30)){rate_limited(r);return;}body(req,{});auto t=token();auto now=now_ms();auto expires=guest_session_expiry(now);auto result=repo->create_guest(sha256(t),now,expires);output(r,std::move(result),[&](const User& u)->json{return {{"user_id",u.id},{"token",t},{"expires_at_ms",expires}};},201);}catch(const std::invalid_argument&){fail(r,{ErrorCode::invalid_argument,"Invalid guest request"});}catch(const json::exception&){fail(r,{ErrorCode::invalid_argument,"Invalid guest request"});}catch(const std::exception&){fail(r,{ErrorCode::internal,"Guest creation failed"});}});
const std::string base=R"(/v1/rooms/([0-9a-fA-F-]{36}))";
#include "shared_data_routes.inc"
protect(server,*repo,"POST","/v1/rooms",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"title","nickname"});auto result=rooms->create_room(actor,bounded(j,"title",60),bounded(j,"nickname",20),now_ms());output(r,std::move(result),[](const RoomMembership& m)->json{return {{"room",room(m.room)},{"member",member(m.member)}};},201);});
protect(server,*repo,"POST","/v1/rooms/join",[&](auto& req,auto& r,const Id& actor){if(!join_requests.allow(req.remote_addr,30)){rate_limited(r);return;}auto j=body(req,{"invite_code","nickname"});auto result=rooms->join_room(actor,bounded(j,"invite_code",32),bounded(j,"nickname",20),now_ms());if(auto* e=std::get_if<Error>(&result)){fail(r,*e);return;}auto m=std::get<RoomMembership>(result);success(r,{{"room",room(m.room)},{"member",member(m.member)}});});
protect(server,*repo,"GET",base,[&](auto& req,auto& r,const Id& actor){output(r,repo->get_room(actor,req.matches[1]),[](const Room& x)->json{return {{"room",room(x)}};});});
protect(server,*repo,"GET",base+"/members",[&](auto& req,auto& r,const Id& actor){auto result=repo->list_visible_members(actor,req.matches[1],now_ms());output(r,std::move(result),[](const std::vector<MemberView>& v)->json{json arr=json::array();for(const auto& x:v)arr.push_back(visible(x));return {{"members",arr}};});});
protect(server,*repo,"GET",base+"/invite",[&](auto& req,auto& r,const Id& actor){output(r,rooms->get_invite(actor,req.matches[1]),[](const Invite& i)->json{return {{"invite_code",i.invite_code},{"qr_payload",i.qr_payload},{"qr_svg",i.qr_svg}};});});
protect(server,*repo,"POST",base+"/leave",[&](auto& req,auto& r,const Id& actor){body(req,{});output(r,rooms->leave_room(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"POST",base+"/end",[&](auto& req,auto& r,const Id& actor){body(req,{});output(r,rooms->end_room(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"PUT",base+"/sharing",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"enabled"});output(r,repo->set_sharing(actor,req.matches[1],json_boolean(j,"enabled"),now_ms()),[](const Member& x)->json{return {{"member",member(x)}};});});
protect(server,*repo,"PUT",base+"/location",[&](auto& req,auto& r,const Id& actor){auto j=json::parse(req.body);if(req.get_header_value("Content-Type").find("application/json")!=0||!j.is_object()||j.size()<5||j.size()>6)throw std::invalid_argument("Invalid JSON fields");for(const char* k:{"latitude","longitude","captured_at_ms","sharing_epoch","sequence"})if(!j.contains(k))throw std::invalid_argument("Missing JSON field");for(auto it=j.begin();it!=j.end();++it)if(it.key()!="latitude"&&it.key()!="longitude"&&it.key()!="accuracy_m"&&it.key()!="captured_at_ms"&&it.key()!="sharing_epoch"&&it.key()!="sequence")throw std::invalid_argument("Unexpected JSON field");LocationUpdate u;u.coordinate={number(j,"latitude"),number(j,"longitude")};if(std::abs(u.coordinate.latitude)>90||std::abs(u.coordinate.longitude)>180)throw std::invalid_argument("Invalid coordinates");if(j.contains("accuracy_m")&&!j.at("accuracy_m").is_null()){u.accuracy_m=number(j,"accuracy_m");if(*u.accuracy_m<0)throw std::invalid_argument("Invalid accuracy");}u.captured_at_ms=integer<TimeMs>(j,"captured_at_ms");u.sharing_epoch=integer<Epoch>(j,"sharing_epoch");u.sequence=integer<std::int64_t>(j,"sequence");if(u.sharing_epoch<0||u.sequence<1||u.captured_at_ms>now_ms()+kFutureToleranceMs)throw std::invalid_argument("Invalid timestamp or sequence");output(r,repo->update_location(actor,req.matches[1],u,now_ms()),[](const Location& x)->json{return {{"location",location(x)}};});});
protect(server,*repo,"PUT",base+"/note",[&](auto& req,auto& r,const Id& actor){auto j=body(req,{"text","sharing_epoch"});NoteDraft d{bounded(j,"text",200),integer<Epoch>(j,"sharing_epoch")};if(d.sharing_epoch<0)throw std::invalid_argument("Invalid epoch");output(r,repo->save_note(actor,req.matches[1],d,now_ms()),[&](const Note& n)->json{return {{"note",note(n,req.matches[1],"")}};});});
protect(server,*repo,"DELETE",base+"/note",[&](auto& req,auto& r,const Id& actor){output(r,repo->delete_note(actor,req.matches[1],now_ms()),[](Unit)->json{return json::object();});});
protect(server,*repo,"PUT",base+"/note/photo",[&](auto& req,auto& r,const Id& actor){
  if(!photos){fail(r,{ErrorCode::storage_unavailable,"Photo storage is not configured"});return;}
  if(!req.is_multipart_form_data() || req.form.fields.size()!=3 || req.form.files.size()!=1 ||
     req.form.get_field_count("note_id")!=1 || req.form.get_field_count("written_at_ms")!=1 ||
     req.form.get_field_count("sharing_epoch")!=1 || req.form.get_file_count("file")!=1)
    throw std::invalid_argument("Invalid photo form");
  const auto note_id=normalize_uuid(req.form.get_field("note_id"));
  const auto written=form_integer(req.form.get_field("written_at_ms"));
  const auto epoch=form_integer(req.form.get_field("sharing_epoch"));
  const auto file=req.form.get_file("file");
  output(r,photos->upload(actor,req.matches[1],note_id,written,epoch,file.content,now_ms()),
    [&](const AttachedPhoto& p)->json{return {{"note",note(p.note,req.matches[1],p.member_id)}};});
});
protect(server,*repo,"GET",base+R"(/members/([0-9a-fA-F-]{36})/photo)",[&](auto& req,auto& r,const Id& actor){
  if(!photos){fail(r,{ErrorCode::storage_unavailable,"Photo storage is not configured"});return;}
  auto result=photos->download(actor,req.matches[1],normalize_uuid(req.matches[2]),now_ms());
  if(auto* error=std::get_if<Error>(&result)){fail(r,*error);return;}
  const auto& photo=std::get<SanitizedPhoto>(result);
  r.status=200;r.set_header("Cache-Control","no-store");r.set_header("X-Content-Type-Options","nosniff");
  r.set_content(photo.bytes,photo.content_type);
});
const auto host=env("GOMMANNA_HOST","127.0.0.1");int port=std::stoi(env("GOMMANNA_PORT","8080"));if(port<1||port>65535)throw std::runtime_error("Invalid port");std::cerr<<"GomManna listening on "<<host<<":"<<port<<"\n";if(!server.listen(host,port))throw std::runtime_error("Could not bind HTTP server");
}catch(const std::exception& e){std::cerr<<"Server startup failed: "<<e.what()<<"\n";return 1;}}
