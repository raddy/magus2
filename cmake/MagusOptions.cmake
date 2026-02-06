add_library(magus2::options INTERFACE)
add_library(magus2::warnings INTERFACE)

target_compile_features(magus2::options INTERFACE cxx_std_20)

option(MAGUS2_ENABLE_PERF "Enable aggressive perf flags" ON)
option(MAGUS2_ENABLE_LTO "Enable LTO" ON)
option(MAGUS2_ENABLE_WARNINGS "Enable warnings" ON)
option(MAGUS2_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
option(MAGUS2_DISABLE_SIMD "Disable SIMD" OFF)
option(MAGUS2_USE_MOLD "Use mold linker when available" OFF)

set(MAGUS2_CPU_TUNING "native" CACHE STRING "CPU tuning (native or a specific arch)")

if(MAGUS2_DISABLE_SIMD)
  target_compile_definitions(magus2::options INTERFACE MAGUS2_DISABLE_SIMD)
endif()

if(MAGUS2_USE_MOLD AND NOT APPLE)
  find_program(MAGUS2_MOLD_EXE mold)
  if(NOT MAGUS2_MOLD_EXE)
    message(FATAL_ERROR "MAGUS2_USE_MOLD=ON but mold was not found in PATH")
  endif()
endif()

if(MAGUS2_ENABLE_PERF)
  target_compile_definitions(magus2::options INTERFACE MAGUS2_PERF_BUILD=1)
  target_compile_options(magus2::options INTERFACE
    -DNDEBUG
    -Ofast
    -march=${MAGUS2_CPU_TUNING}
    -mtune=${MAGUS2_CPU_TUNING}
    -mbmi2
    -funroll-loops
  )
  if(NOT APPLE)
    target_compile_options(magus2::options INTERFACE -fno-plt)
  endif()
endif()

if(MAGUS2_USE_MOLD AND NOT APPLE)
  target_link_options(magus2::options INTERFACE -fuse-ld=mold)
endif()

if(MAGUS2_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(magus2::options INTERFACE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
      -fno-common
    )
    target_link_options(magus2::options INTERFACE
      -fsanitize=address,undefined
    )
  endif()
endif()

if(MAGUS2_ENABLE_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(magus2::warnings INTERFACE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
      -Wcast-qual -Wformat=2 -Wundef -Werror=float-equal
      -Wshadow -Wcast-align -Wnull-dereference -Wdouble-promotion
      -Wimplicit-fallthrough -Wextra-semi -Woverloaded-virtual
      -Wnon-virtual-dtor -Wold-style-cast
      -Wno-unused-variable -Wno-unused-parameter
      -fdiagnostics-color
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(magus2::warnings INTERFACE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
      -Wcast-qual -Wformat=2 -Wundef -Werror=float-equal
      -Wshadow -Wcast-align -Wnull-dereference -Wdouble-promotion
      -Wimplicit-fallthrough -Wextra-semi -Woverloaded-virtual
      -Wnon-virtual-dtor -Wold-style-cast
      -Wno-unused-variable -Wno-unused-parameter
    )
  endif()
endif()

if(MAGUS2_ENABLE_LTO)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()
