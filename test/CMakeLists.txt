add_executable(magus2_smoke smoke.cpp)

target_link_libraries(magus2_smoke PRIVATE magus2_lib)

add_test(NAME magus2_smoke COMMAND magus2_smoke)
