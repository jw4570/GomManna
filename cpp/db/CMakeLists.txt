cmake_minimum_required(VERSION 3.24)

# Can be built independently while the shared server CMake is being prepared.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(gommanna_db LANGUAGES CXX)
  include(CTest)
endif()

if(NOT TARGET gm_contract)
  add_library(gm_contract INTERFACE)
  target_include_directories(gm_contract INTERFACE "${CMAKE_CURRENT_LIST_DIR}/../include")
endif()

find_package(PostgreSQL 17 REQUIRED)
add_library(gm_db STATIC postgres_repository.cpp)
target_compile_features(gm_db PUBLIC cxx_std_20)
target_link_libraries(gm_db PUBLIC gm_contract PRIVATE PostgreSQL::PostgreSQL)
if(MSVC)
  target_compile_options(gm_db PRIVATE /utf-8 /W4)
else()
  target_compile_options(gm_db PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(gm_db_config_test ../tests/db/db_config_test.cpp)
  target_compile_features(gm_db_config_test PRIVATE cxx_std_20)
  target_link_libraries(gm_db_config_test PRIVATE gm_db)
  add_test(NAME gm_db_config COMMAND gm_db_config_test)
  add_executable(gm_db_integration_test ../tests/db/db_integration_test.cpp)
  target_compile_features(gm_db_integration_test PRIVATE cxx_std_20)
  target_link_libraries(gm_db_integration_test PRIVATE gm_db)
  if(MSVC)
    target_compile_options(gm_db_integration_test PRIVATE /utf-8 /W4)
  endif()
  add_test(NAME gm_db_integration COMMAND gm_db_integration_test)
  set_tests_properties(gm_db_integration PROPERTIES SKIP_RETURN_CODE 77)

  # On Windows, CTest also needs libpq.dll and its dependencies at runtime.
  # Find them alongside the installation used for the import library.
  if(WIN32)
    find_file(GOMMANNA_LIBPQ_DLL NAMES libpq.dll
      HINTS "${PostgreSQL_ROOT}/bin" "$ENV{PostgreSQL_ROOT}/bin"
            "${PostgreSQL_LIBRARY_DIRS}/../bin" "${PostgreSQL_LIBRARY_DIRS}")
    if(GOMMANNA_LIBPQ_DLL)
      get_filename_component(GOMMANNA_LIBPQ_BIN "${GOMMANNA_LIBPQ_DLL}" DIRECTORY)
      set_tests_properties(gm_db_config gm_db_integration PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${GOMMANNA_LIBPQ_BIN}")
    else()
      message(WARNING "libpq.dll was not found. Add the PostgreSQL bin directory to PATH before running the tests.")
    endif()
  endif()
endif()
