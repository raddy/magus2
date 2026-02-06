add_library(magus2_trader)
add_library(magus2::trader ALIAS magus2_trader)

target_sources(magus2_trader PRIVATE trader.cpp)

target_include_directories(magus2_trader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(magus2_trader PUBLIC magus2::options magus2::warnings)
