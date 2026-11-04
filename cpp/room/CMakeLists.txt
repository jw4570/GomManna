add_subdirectory(c)

add_library(gm_room STATIC room_service.cpp)
target_link_libraries(gm_room PUBLIC gm_contract PRIVATE gm_room_c)
