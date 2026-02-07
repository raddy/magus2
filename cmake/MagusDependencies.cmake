add_library(magus2_deps INTERFACE)
add_library(magus2::deps ALIAS magus2_deps)

if(EXISTS "${PROJECT_SOURCE_DIR}/external/fmtlog/fmt/CMakeLists.txt")
  add_subdirectory(${PROJECT_SOURCE_DIR}/external/fmtlog/fmt external/fmt_build EXCLUDE_FROM_ALL)
endif()
