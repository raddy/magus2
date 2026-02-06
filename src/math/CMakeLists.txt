add_library(magus2_math)
add_library(magus2::math ALIAS magus2_math)

target_sources(magus2_math PRIVATE math.cpp)

target_include_directories(magus2_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(magus2_math PUBLIC magus2::options magus2::warnings)
