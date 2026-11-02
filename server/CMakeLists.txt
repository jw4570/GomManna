cmake_minimum_required(VERSION 3.24)
project(GomMannaServer VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
find_package(Threads REQUIRED)
find_package(OpenSSL 3 REQUIRED)
find_package(PostgreSQL REQUIRED)

# 의존성 버전과 다운로드 해시를 고정합니다.
FetchContent_Declare(httplib
  URL https://codeload.github.com/yhirose/cpp-httplib/tar.gz/refs/tags/v0.63.0
  URL_HASH SHA256=44ab42e16926e95f7641d286bc90d739ab1c16886114d7cc5bc7a9f905c5edf8
  SOURCE_SUBDIR header_only)
FetchContent_Declare(json
  URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
  URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jwt
  URL https://codeload.github.com/Thalhammer/jwt-cpp/tar.gz/refs/tags/v0.7.2
  URL_HASH SHA256=6e815d86c168eb521a27937d603747dec0ca3c39ffc12d6fa72e2cf78a5b02d2)
FetchContent_MakeAvailable(httplib json jwt)
add_library(gommanna_core src/core.cpp src/db.cpp src/app.cpp)
target_include_directories(gommanna_core PUBLIC include)
target_include_directories(gommanna_core SYSTEM PUBLIC ${httplib_SOURCE_DIR})
target_link_libraries(gommanna_core PUBLIC nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp PostgreSQL::PostgreSQL Threads::Threads)
target_compile_options(gommanna_core PRIVATE -Wall -Wextra -Wpedantic)
add_executable(gommanna_server src/main.cpp)
target_link_libraries(gommanna_server PRIVATE gommanna_core)
include(CTest)
if(BUILD_TESTING)
  add_executable(gommanna_unit_tests tests/unit.cpp)
  target_link_libraries(gommanna_unit_tests PRIVATE gommanna_core)
  add_test(NAME gommanna_unit COMMAND gommanna_unit_tests)
endif()
install(TARGETS gommanna_server RUNTIME DESTINATION bin)
