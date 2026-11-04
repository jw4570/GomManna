// Test fixture only: makes a synthetic room directly in the real repository.
// The room service itself is the teammate's separate implementation.
#include "gommanna/contracts.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <variant>

using namespace gommanna;
namespace {
TimeMs now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string test_invite() {
    constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::random_device random;
    std::string result;
    for (int i = 0; i < 8; ++i) result += alphabet[random() % 32];
    return result;
}
} // namespace

int main(int argc, char** argv) {
    const char* url = std::getenv("GOMMANNA_DATABASE_URL");
    if (!url || !*url || argc != 4) { std::cerr << "Fixture configuration missing\n"; return 1; }
    auto created = make_postgres_repository({url});
    if (std::holds_alternative<Error>(created)) { std::cerr << "Fixture DB connection failed\n"; return 1; }
    auto repo = std::move(std::get<std::unique_ptr<IRepository>>(created));
    const std::string command = argv[1], host = argv[2], other = argv[3];
    if (command == "cleanup") {
        return std::holds_alternative<Unit>(repo->end_room(host, other, now_ms())) ? 0 : 1;
    }
    if (command != "prepare") return 1;
    for (int attempt = 0; attempt < 5; ++attempt) {
        auto room = repo->create_room(host, "HTTP integration test", "테스트 이안", test_invite(), now_ms());
        if (auto* e = std::get_if<Error>(&room)) {
            if (e->code == ErrorCode::invite_code_conflict) continue;
            std::cerr << "Fixture room creation failed\n";
            return 1;
        }
        auto membership = std::get<RoomMembership>(std::move(room));
        auto joined = repo->join_room(other, membership.room.id, "테스트 원규", now_ms());
        if (std::holds_alternative<Error>(joined)) {
            (void)repo->end_room(host, membership.room.id, now_ms());
            std::cerr << "Fixture participant creation failed\n";
            return 1;
        }
        std::cout << membership.room.id << '\n';
        return 0;
    }
    std::cerr << "Fixture could not allocate an invite code\n";
    return 1;
}
