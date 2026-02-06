cmake_minimum_required(VERSION 3.24)

project(magus2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/MagusDependencies.cmake)
include(cmake/MagusOptions.cmake)
include(cmake/MagusTesting.cmake)

add_subdirectory(src/infra)
add_subdirectory(src/math)
add_subdirectory(src/trader)

add_library(magus2_lib src/lib.cpp)
target_link_libraries(magus2_lib
  PUBLIC
    magus2::infra
    magus2::math
    magus2::trader
    magus2::options
    magus2::warnings
    magus2::deps
)

target_include_directories(magus2_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(apps)

include(CTest)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
