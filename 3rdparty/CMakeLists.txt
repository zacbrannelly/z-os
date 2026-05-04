cmake_minimum_required(VERSION 3.25)

# Build openlibm as a static dependency for the freestanding kernel.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
add_subdirectory(./openlibm EXCLUDE_FROM_ALL)

# openlibm's own CMake target exports host-style interface flags (for example
# strict C standard settings). Clear those exported interface properties so
# linking openlibm does not change how the kernel itself is compiled.
target_include_directories(openlibm SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/openlibm/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openlibm/src"
)

get_target_property(OPENLIBM_COMPILE_OPTIONS openlibm INTERFACE_COMPILE_OPTIONS)
if(OPENLIBM_COMPILE_OPTIONS)
    set_target_properties(openlibm PROPERTIES INTERFACE_COMPILE_OPTIONS "")
endif()

get_target_property(OPENLIBM_INCLUDE_DIRECTORIES openlibm INTERFACE_INCLUDE_DIRECTORIES)
if(OPENLIBM_INCLUDE_DIRECTORIES)
    set_target_properties(openlibm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
endif()
