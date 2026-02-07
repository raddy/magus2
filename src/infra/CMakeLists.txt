add_library(magus2_infra)
add_library(magus2::infra ALIAS magus2_infra)

target_sources(magus2_infra
  PRIVATE
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

if(MAGUS2_ENABLE_FMTLOG_TRACE)
  if(MAGUS2_FMT_INCLUDE_DIR STREQUAL "")
    if(EXISTS "/opt/homebrew/include/fmt/core.h")
      set(MAGUS2_FMT_INCLUDE_DIR "/opt/homebrew/include")
    elseif(EXISTS "/usr/local/include/fmt/core.h")
      set(MAGUS2_FMT_INCLUDE_DIR "/usr/local/include")
    else()
      message(FATAL_ERROR "MAGUS2_ENABLE_FMTLOG_TRACE=ON requires MAGUS2_FMT_INCLUDE_DIR to point to fmt headers")
    endif()
  endif()
  target_include_directories(magus2_infra SYSTEM PUBLIC ${MAGUS2_FMT_INCLUDE_DIR})

  find_library(MAGUS2_FMT_LIB fmt PATHS /opt/homebrew/lib /usr/local/lib REQUIRED)
  target_link_libraries(magus2_infra PUBLIC ${MAGUS2_FMT_LIB})
else()
  target_compile_definitions(magus2_infra PUBLIC TLOG_NOFMTLOG)
endif()
