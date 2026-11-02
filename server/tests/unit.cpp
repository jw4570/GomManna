#include "gommanna/core.hpp"
#include <jwt-cpp/jwt.h>
#include <iostream>
#include <set>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F fn, int status) {
    try { fn(); } catch (const gm::ApiError& e) { check(e.status == status,"Wrong error status"); return; }
    throw std::runtime_error("Expected rejection");
}
}
int main() {
    try {
        gm::Config config;
        config.auth_secret = std::string(64,'a');
        gm::Auth auth(config);
        const std::string user = "00000000-0000-4000-8000-000000000001";
        auto token = auth.issue(user);
        check(auth.verify(token) == user,"Token round trip");
        auto other = config; other.auth_secret = std::string(64,'b');
        rejects([&] { gm::Auth(other).verify(token); },401);
        auto expired = jwt::create().set_issuer("gommanna-server").set_audience("gommanna-ios")
            .set_subject(user).set_issued_at(std::chrono::system_clock::now()-std::chrono::hours(2))
            .set_expires_at(std::chrono::system_clock::now()-std::chrono::hours(1))
            .sign(jwt::algorithm::hs256{config.auth_secret});
        rejects([&] { auth.verify(expired); },401);
        auto no_exp = jwt::create().set_issuer("gommanna-server").set_audience("gommanna-ios")
            .set_subject(user).sign(jwt::algorithm::hs256{config.auth_secret});
        rejects([&] { auth.verify(no_exp); },401);
        rejects([&] { auth.verify("not-a-token"); },401);
        rejects([&] { gm::uuid("'; DROP TABLE users; --"); },400);
        check(gm::text({{"name","  곰만나  "}},"name",3) == "곰만나","Korean name length");
        rejects([&] { gm::text({{"name","곰만나요"}},"name",3); },400);
        rejects([&] { gm::text({{"name","  "}},"name",20); },400);
        rejects([&] { gm::number({{"latitude",91}},"latitude",-90,90); },400);
        std::set<std::string> codes;
        for(int i=0;i<1000;++i) {
            auto code=gm::random_code();
            check(code.size()==8 && code.find_first_not_of("ABCDEFGHJKLMNPQRSTUVWXYZ23456789")==std::string::npos,"Invalid invite code");
            codes.insert(code);
        }
        check(codes.size()==1000,"Invite collision sanity check");
        gm::RateLimiter limiter;
        limiter.check("user",1,std::chrono::minutes(1));
        rejects([&]{limiter.check("user",1,std::chrono::minutes(1));},429);
        std::cout << "Auth, expiry, input validation, invite codes and rate limits passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
