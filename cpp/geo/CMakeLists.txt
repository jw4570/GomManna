add_library(gm_geo STATIC geo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../GomManna/Services/CRules/Sources/distance.c)
target_link_libraries(gm_geo PUBLIC gm_contract)
target_compile_features(gm_geo PUBLIC cxx_std_20)

if(BUILD_TESTING)
  add_executable(gm_geo_test ${CMAKE_CURRENT_LIST_DIR}/../tests/geo/geo_test.cpp)
  target_link_libraries(gm_geo_test PRIVATE gm_geo)
  add_test(NAME geo_rules COMMAND gm_geo_test)
endif()
