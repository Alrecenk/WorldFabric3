cmake_minimum_required(VERSION 3.24)

project(WorldFabric
    DESCRIPTION "WorldFabric Engine and Narball"
    LANGUAGES   C CXX
)

# ==============================================================================
# Platform & Architecture Validation
# ==============================================================================
# Prebuilt import libraries in lib/ and runtime binaries in dll/ target x86_64
# Windows exclusively. Fail fast during configuration on unsupported targets.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "WorldFabric requires a 64-bit Windows target (prebuilt binaries in lib/ and dll/ are x64).")
endif()

# ==============================================================================
# Build Options
# ==============================================================================
option(WF_COMPILE_SHADERS "Compile GLSL shaders to SPIR-V (.spv) during the build" ON)

set(WF_VULKAN_HEADERS_TAG "v1.4.309" CACHE STRING
    "Git tag used when fetching fallback Vulkan headers via FetchContent"
)

# ==============================================================================
# Language Standards
# ==============================================================================
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Dependencies: Vulkan Headers & Shader Compiler
# ==============================================================================
# Prefer headers from a locally installed Vulkan SDK when available and complete
# (specifically checking for vk_enum_string_helper.h). Otherwise, fetch the
# matching Khronos header repositories. Only headers are needed here because
# vulkan-1.lib is linked directly from the repository's lib/ directory.
find_package(Vulkan QUIET)

if(Vulkan_FOUND AND EXISTS "${Vulkan_INCLUDE_DIRS}/vulkan/vk_enum_string_helper.h")
    set(WF_VULKAN_INCLUDE_DIRS ${Vulkan_INCLUDE_DIRS})
    message(STATUS "Vulkan: Using installed SDK headers (${Vulkan_INCLUDE_DIRS})")
else()
    include(FetchContent)

    # Point SOURCE_SUBDIR at a non-existent directory so FetchContent downloads
    # the header repositories without executing their CMakeLists.txt files.
    FetchContent_Declare(vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        ${WF_VULKAN_HEADERS_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _header_only_noop
    )
    FetchContent_Declare(vulkan_utility
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
        GIT_TAG        ${WF_VULKAN_HEADERS_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _header_only_noop
    )
    FetchContent_MakeAvailable(vulkan_headers vulkan_utility)

    set(WF_VULKAN_INCLUDE_DIRS
        "${vulkan_headers_SOURCE_DIR}/include"
        "${vulkan_utility_SOURCE_DIR}/include"
    )
    message(STATUS "Vulkan: SDK not found; using fetched headers (${WF_VULKAN_HEADERS_TAG})")
endif()

find_program(WF_GLSLC
    NAMES glslc
    HINTS ${Vulkan_GLSLC_EXECUTABLE} "$ENV{VULKAN_SDK}/Bin"
    DOC   "Path to the Vulkan glslc SPIR-V shader compiler"
)

# ==============================================================================
# Primary Executable Target: WorldFabric
# ==============================================================================
add_executable(WorldFabric)

# Collect project sources and headers. CONFIGURE_DEPENDS instructs CMake/Ninja
# to automatically re-check these globs at build time when files are added or
# removed.
file(GLOB WF_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/glm/detail/*.cpp"
)

file(GLOB WF_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/header/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/header/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/header/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

target_sources(WorldFabric PRIVATE
    ${WF_SOURCES}
    ${WF_HEADERS}
)

# Local include/ must precede WF_VULKAN_INCLUDE_DIRS so the repository's own
# SDL3 and GLM headers take priority over copies bundled in the Vulkan SDK.
target_include_directories(WorldFabric PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/header"
    "${CMAKE_CURRENT_SOURCE_DIR}/header"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/header"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${WF_VULKAN_INCLUDE_DIRS}
)

target_compile_definitions(WorldFabric PRIVATE
    _CONSOLE
    UNICODE
    _UNICODE
    GLM_FORCE_CTOR_INIT
    GLM_FORCE_PURE
)

target_compile_options(WorldFabric PRIVATE
    $<$<CONFIG:Release>:-O2>
    -ffp-contract=off
)

# Link all prebuilt import libraries in lib/ alongside required system and
# runtime libraries.
file(GLOB WF_PREBUILT_LIBS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.lib"
)

target_link_libraries(WorldFabric PRIVATE
    ${WF_PREBUILT_LIBS}
    ws2_32
    glu32
)

# Statically link the MinGW runtime so the executable does not depend on
# external compiler runtime DLLs, use LLD for linking, and emit a CodeView
# .pdb alongside the executable for the Visual Studio profiler.
target_link_options(WorldFabric PRIVATE
    -static
)

# ==============================================================================
# Runtime Output Directory & IDE Configuration
# ==============================================================================
# The engine loads assets and shaders via paths relative to the current working
# directory (e.g., ./assets, ./shader, ./Narball/...). All build outputs,
# compiled shaders, and runtime assets are placed together in WF_RUNTIME_DIR.
#
# Note: WF_RUNTIME_DIR is computed explicitly because add_custom_command(OUTPUT)
# does not support target-dependent generator expressions like $<TARGET_FILE_DIR>.
get_property(WF_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(WF_IS_MULTI_CONFIG)
    set(WF_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
else()
    set(WF_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

set_target_properties(WorldFabric PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY      "${WF_RUNTIME_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${WF_RUNTIME_DIR}"
)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY VS_STARTUP_PROJECT WorldFabric
)

# ==============================================================================
# Shader Compilation (GLSL -> SPIR-V)
# ==============================================================================
# Shaders are compiled directly into <WF_RUNTIME_DIR>/<relative_shader_path>.spv
# so each build tree maintains isolated outputs without polluting the source tree.
file(GLOB WF_SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.comp"
)

if(WF_COMPILE_SHADERS AND WF_GLSLC)
    message(STATUS "Shaders: Compiling with ${WF_GLSLC}")
    set(WF_SHADER_OUTPUTS)

    foreach(shader IN LISTS WF_SHADER_SOURCES)
        file(RELATIVE_PATH shader_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${shader}")
        set(spv_output "${WF_RUNTIME_DIR}/${shader_rel}.spv")
        set(depfile    "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_rel}.d")
        get_filename_component(depfile_dir "${depfile}" DIRECTORY)

        add_custom_command(
            OUTPUT  "${spv_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${depfile_dir}"
            COMMAND "${WF_GLSLC}" -MD -MF "${depfile}" "${shader}" -o "${spv_output}"
            DEPENDS "${shader}"
            DEPFILE "${depfile}"
            COMMENT "Compiling shader ${shader_rel}"
            VERBATIM
        )
        list(APPEND WF_SHADER_OUTPUTS "${spv_output}")
    endforeach()

    add_custom_target(shaders ALL
        DEPENDS ${WF_SHADER_OUTPUTS}
        SOURCES ${WF_SHADER_SOURCES}
    )
    add_dependencies(WorldFabric shaders)
elseif(WF_COMPILE_SHADERS)
    message(WARNING "Shaders: glslc not found (install the Vulkan SDK or set WF_GLSLC); SPIR-V compilation skipped.")
endif()

# ==============================================================================
# Runtime Assets & DLL Deployment
# ==============================================================================
# Mirror runtime assets, raw shader sources, and configuration files into the
# runtime directory. Pre-compiled .spv files in the source tree are excluded
# so they do not conflict with outputs produced by the `shaders` target.
file(GLOB_RECURSE WF_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/asset/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*"
)
list(FILTER WF_RUNTIME_SOURCES EXCLUDE REGEX "\\.spv$")
list(APPEND WF_RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/server_settings.json"
)

set(WF_RUNTIME_OUTPUTS)
foreach(src IN LISTS WF_RUNTIME_SOURCES)
    file(RELATIVE_PATH src_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${src}")
    set(dst "${WF_RUNTIME_DIR}/${src_rel}")

    add_custom_command(
        OUTPUT  "${dst}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${src}" "${dst}"
        DEPENDS "${src}"
        VERBATIM
    )
    list(APPEND WF_RUNTIME_OUTPUTS "${dst}")
endforeach()

add_custom_target(runtime_files ALL
    DEPENDS ${WF_RUNTIME_OUTPUTS}
)
add_dependencies(WorldFabric runtime_files)

# Copy third-party runtime DLLs alongside the built executable after linking.
file(GLOB WF_RUNTIME_DLLS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/dll/*.dll"
)

add_custom_command(TARGET WorldFabric POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${WF_RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:WorldFabric>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLLs to output directory"
    VERBATIM
)

# ==============================================================================
# Convenience Targets
# ==============================================================================
# Launch WorldFabric directly from its runtime directory (e.g., `cmake --build --target run`).
add_custom_target(run
    COMMAND           "$<TARGET_FILE:WorldFabric>"
    DEPENDS           WorldFabric
    WORKING_DIRECTORY "${WF_RUNTIME_DIR}"
    USES_TERMINAL
    COMMENT           "Running WorldFabric from ${WF_RUNTIME_DIR}"
    VERBATIM
)

