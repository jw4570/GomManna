cmake_minimum_required(VERSION 3.24)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(GomMannaRoomC LANGUAGES C)
  include(CTest)
else()
  get_property(_gm_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  if(NOT "C" IN_LIST _gm_languages)
    message(FATAL_ERROR "Enable C in the parent project: project(GomManna LANGUAGES C CXX)")
  endif()
endif()

add_library(gm_room_c STATIC
  src/room_code.c
  src/secure_random.c
  src/room_qr.c
  third_party/qrcodegen/qrcodegen.c
)
target_include_directories(gm_room_c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(gm_room_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen)
target_compile_features(gm_room_c PRIVATE c_std_11)
set_target_properties(gm_room_c PROPERTIES C_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_link_libraries(gm_room_c PRIVATE bcrypt)
endif()
if(MSVC)
  target_compile_options(gm_room_c PRIVATE /W4 /utf-8)
else()
  target_compile_options(gm_room_c PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(GM_ROOM_BUILD_EXAMPLE "Build the pure C code/QR command-line example" ON)
if(GM_ROOM_BUILD_EXAMPLE)
  add_executable(room_invite_example examples/room_invite_example.c)
  target_link_libraries(room_invite_example PRIVATE gm_room_c)
endif()

if(BUILD_TESTING)
  add_executable(gm_room_c_test tests/test_room_invite.c)
  target_link_libraries(gm_room_c_test PRIVATE gm_room_c)
  add_test(NAME room_invite_c COMMAND gm_room_c_test)

  # Link a deterministic failing RNG only into this test, never into the library.
  add_executable(gm_room_random_failure_test tests/test_random_failure.c src/room_code.c)
  target_include_directories(gm_room_random_failure_test PRIVATE include src)
  target_compile_features(gm_room_random_failure_test PRIVATE c_std_11)
  add_test(NAME room_random_failure COMMAND gm_room_random_failure_test)
endif()
