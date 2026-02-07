add_executable(magus2_hello hello/main.cpp)
target_link_libraries(magus2_hello PRIVATE magus2_lib)

add_library(magus2_mvp_app
  mvp/logging.cpp
  mvp/nodes.cpp
  mvp/runtime.cpp
)
target_include_directories(magus2_mvp_app
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(magus2_mvp_app
  PUBLIC
    magus2::infra
    magus2::options
    magus2::warnings
)

add_executable(magus2_mvp mvp/main.cpp)
target_link_libraries(magus2_mvp PRIVATE magus2_mvp_app)
