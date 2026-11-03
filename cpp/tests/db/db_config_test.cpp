#include "gommanna/contracts.hpp"

#include <iostream>
#include <string>
#include <variant>

using namespace gommanna;

int main() {
    auto missing=make_postgres_repository(RepositoryConfig{});
    auto* missing_error=std::get_if<Error>(&missing);
    if(!missing_error || missing_error->code!=ErrorCode::invalid_argument) return 1;

    std::string contains_nul("prefix\0suffix",13);
    auto invalid=make_postgres_repository(RepositoryConfig{contains_nul});
    auto* invalid_error=std::get_if<Error>(&invalid);
    if(!invalid_error || invalid_error->code!=ErrorCode::invalid_argument) return 1;

    const std::string secret="test_secret_DO_NOT_LOG";
    auto malformed=make_postgres_repository(RepositoryConfig{"host='"+secret});
    auto* malformed_error=std::get_if<Error>(&malformed);
    if(!malformed_error || malformed_error->message.find(secret)!=std::string::npos) return 1;
    std::cout << "PASS: invalid DB configuration is rejected without exposing input\n";
}
