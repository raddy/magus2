add_library(magus2_infra)
add_library(magus2::infra ALIAS magus2_infra)

target_sources(magus2_infra PRIVATE infra.cpp)

target_include_directories(magus2_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(magus2_infra PUBLIC magus2::options magus2::warnings)
