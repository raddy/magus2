add_library(magus2_infra)
add_library(magus2::infra ALIAS magus2_infra)

target_sources(magus2_infra
  PRIVATE
    app/host.cpp
    infra.cpp
    topology/runtime.cpp
)

target_include_directories(magus2_infra
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_include_directories(magus2_infra SYSTEM
  PUBLIC
    ${PROJECT_SOURCE_DIR}/external
)

target_link_libraries(magus2_infra PUBLIC magus2::options magus2::warnings)

target_compile_definitions(magus2_infra PUBLIC TLOG_TRACE_SOURCE_RDTSC)

if(NOT TARGET fmt::fmt)
  message(FATAL_ERROR "vendored fmt target missing at external/fmtlog/fmt")
endif()

target_link_libraries(magus2_infra PUBLIC fmt::fmt)
