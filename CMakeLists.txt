cmake_minimum_required(VERSION 3.28)
project(baker_godot LANGUAGES CXX)

# Baker for Godot: a GDExtension over Baker's public headers.
#
#   cmake -B build -DGODOTCPP_TARGET=template_debug     # the game, run from the editor
#   cmake -B build-editor -DGODOTCPP_TARGET=editor      # the editor plugin (step 2)
#   cmake -B build-release -DGODOTCPP_TARGET=template_release   # exports: BAKER_SHIPPING
#
# The library lands in sample/addons/baker/bin/, where baker.gdextension names it.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Everything links into one shared library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# godot-cpp, for one Godot version line (4.5). Its headers are not ours to warn about.
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
FetchContent_Declare(godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG        e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77)  # godot-4.5-stable
FetchContent_MakeAvailable(godot-cpp)

# Baker, through its public headers only, at a released version.
# A local checkout: -DFETCHCONTENT_SOURCE_DIR_BAKER=/path/to/baker
if(GODOTCPP_TARGET STREQUAL "template_release")
  set(BAKER_SHIPPING ON CACHE BOOL "Exports are shipping builds" FORCE)
endif()
set(BAKER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BAKER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BAKER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(baker
  GIT_REPOSITORY https://github.com/zach/baker.git
  GIT_TAG        v0.2.1)
FetchContent_MakeAvailable(baker)

add_library(baker_godot SHARED
  src/register_types.cpp
  src/godot_files.cpp
  src/values.cpp
  src/baker_runtime.cpp
  src/baker_binding.cpp)
target_link_libraries(baker_godot PRIVATE godot-cpp baker::baker baker::host)
if(MSVC)
  target_compile_options(baker_godot PRIVATE /W4 /permissive- /utf-8 /EHs-c-)
  # fopen is fine: every result is checked.
  target_compile_definitions(baker_godot PRIVATE _HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(baker_godot PRIVATE -Wall -Wextra -Wpedantic -Wshadow -fno-exceptions)
endif()
option(BAKER_GODOT_WERROR "Warnings are errors" OFF)
if(BAKER_GODOT_WERROR)
  set_target_properties(baker_godot PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endif()

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set(BAKER_GODOT_BIN "${CMAKE_CURRENT_SOURCE_DIR}/sample/addons/baker/bin")
set_target_properties(baker_godot PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  LIBRARY_OUTPUT_DIRECTORY "$<1:${BAKER_GODOT_BIN}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${BAKER_GODOT_BIN}>"
  PDB_OUTPUT_DIRECTORY     "$<1:${BAKER_GODOT_BIN}>"
  PREFIX "lib"
  OUTPUT_NAME "baker_godot${GODOTCPP_SUFFIX}")
