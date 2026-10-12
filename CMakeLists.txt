cmake_minimum_required(VERSION 3.24)

project(WorldFabric LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "WorldFabric only builds for 64-bit Windows (the prebuilt libs in lib/ are x64).")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Vulkan headers: prefer an installed Vulkan SDK, otherwise fetch the matching
# Vulkan-Headers + Vulkan-Utility-Libraries (for vk_enum_string_helper.h).
# Only include directories are used; vulkan-1.lib is linked from lib/.
find_package(Vulkan QUIET)
if(Vulkan_FOUND AND EXISTS "${Vulkan_INCLUDE_DIRS}/vulkan/vk_enum_string_helper.h")
    set(WF_VULKAN_INCLUDE_DIRS ${Vulkan_INCLUDE_DIRS})
    message(STATUS "Using Vulkan SDK headers: ${Vulkan_INCLUDE_DIRS}")
else()
    include(FetchContent)
    set(WF_VULKAN_HEADERS_TAG "v1.4.309" CACHE STRING "Tag used when fetching Vulkan headers")
    # SOURCE_SUBDIR points at a non-existent dir so the projects are downloaded but not configured.
    FetchContent_Declare(vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG ${WF_VULKAN_HEADERS_TAG}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _none)
    FetchContent_Declare(vulkan_utility
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
        GIT_TAG ${WF_VULKAN_HEADERS_TAG}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _none)
    FetchContent_MakeAvailable(vulkan_headers vulkan_utility)
    set(WF_VULKAN_INCLUDE_DIRS
        "${vulkan_headers_SOURCE_DIR}/include"
        "${vulkan_utility_SOURCE_DIR}/include")
    message(STATUS "Vulkan SDK not found; using fetched Vulkan headers ${WF_VULKAN_HEADERS_TAG}")
endif()

set(WF_SOURCES
    engine/source/AsyncPlugin.cpp
    engine/source/AudioPlugin.cpp
    engine/source/BSPNode.cpp
    engine/source/ConvexShape.cpp
    engine/source/ConvexSolid.cpp
    engine/source/GLTF.cpp
    engine/source/NetPhysics.cpp
    engine/source/OpenXRPlugin.cpp
    engine/source/OptimizationProblem.cpp
    engine/source/PanelPlugin.cpp
    engine/source/ParticlePlugin.cpp
    engine/source/Physics.cpp
    engine/source/Polygon.cpp
    engine/source/RadialVolume.cpp
    engine/source/SavePlugin.cpp
    engine/source/ScenePlugin.cpp
    engine/source/Sockets.cpp
    engine/source/StateMachine.cpp
    engine/source/StatePlugin.cpp
    engine/source/SteamWorksPlugin.cpp
    engine/source/TableInterface.cpp
    engine/source/Timeline.cpp
    engine/source/Variant.cpp
    engine/source/ViewPlugin.cpp
    engine/source/VkBootstrap.cpp
    engine/source/vk_initializers.cpp
    engine/source/VolumeNode.cpp
    engine/source/VulkanPlugin.cpp
    engine/source/WorldPlugin.cpp
    include/glm/detail/glm.cpp
    include/stb_vorbis.c
    Narball/source/Ball.cpp
    Narball/source/Nameplate.cpp
    Narball/source/NarballGame.cpp
    Narball/source/NarballMenu.cpp
    Narball/source/NarballObjects.cpp
    Narball/source/NarballServer.cpp
    Narball/source/Narwhal.cpp
    source/BallTestApp.cpp
    source/BallThrowApp.cpp
    source/CollisionTestApp.cpp
    source/ConstraintTestApp.cpp
    source/Main.cpp
    source/MirrorApp.cpp
    source/NetPhysicsApp.cpp
    source/PlaySong.cpp
    source/PyramidApp.cpp
    source/SceneDemoApp.cpp
    source/SceneDemoApp2.cpp
    source/SocketTest.cpp
    source/TraceApp.cpp
    source/VulkanDemoApp.cpp
)

file(GLOB WF_HEADERS CONFIGURE_DEPENDS
    engine/header/*.h
    header/*.h
    Narball/header/*.h)

add_executable(WorldFabric ${WF_SOURCES} ${WF_HEADERS})

# The repo's include/ must come before the Vulkan SDK so its bundled SDL/GLM copies don't win.
target_include_directories(WorldFabric PRIVATE
    engine/header
    header
    Narball/header
    include
    ${WF_VULKAN_INCLUDE_DIRS})

target_compile_definitions(WorldFabric PRIVATE
    _CONSOLE
    UNICODE
    _UNICODE
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

set(WF_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(WorldFabric PRIVATE
    ${WF_LIB_DIR}/glew32.lib
    ${WF_LIB_DIR}/OpenAL32.lib
    ${WF_LIB_DIR}/OpenGL32.Lib
    ${WF_LIB_DIR}/openvr_api.lib
    ${WF_LIB_DIR}/sdkencryptedappticket64.lib
    ${WF_LIB_DIR}/SDL3.lib
    ${WF_LIB_DIR}/SDL3_image.lib
    ${WF_LIB_DIR}/SDL3_mixer.lib
    ${WF_LIB_DIR}/SDL3_net.lib
    ${WF_LIB_DIR}/SDL3_rtf.lib
    ${WF_LIB_DIR}/SDL3_ttf.lib
    ${WF_LIB_DIR}/steam_api64.lib
    ${WF_LIB_DIR}/vulkan-1.lib)

if(MSVC)
    target_compile_options(WorldFabric PRIVATE
        /W3 /sdl /permissive- /MP
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Oy /GT /Gy /GL /fp:strict>)
    target_link_options(WorldFabric PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF /DEBUG>)
    # Static MSVC libs that MinGW can't consume.
    target_link_libraries(WorldFabric PRIVATE
        ${WF_LIB_DIR}/glew32s.lib
        ${WF_LIB_DIR}/SDL3_test.lib)
else()
    target_compile_options(WorldFabric PRIVATE
        $<$<CONFIG:Release>:-O2>)
    # stdc++exp provides std::print's terminal support in libstdc++.
    target_link_libraries(WorldFabric PRIVATE ws2_32 glu32 stdc++exp)
    # Statically link the MinGW runtime so the exe doesn't need MSYS2's bin dir on PATH.
    target_link_options(WorldFabric PRIVATE -static)
endif()

# Assets and shaders are loaded via paths relative to the working directory (./assets, ./Narball/...),
# so they are copied next to the exe and the app runs from there.
get_property(WF_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(WF_MULTI_CONFIG)
    set(WF_RUNTIME_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
    set(WF_RUNTIME_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
set_target_properties(WorldFabric PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${WF_RUNTIME_DIR}
    VS_DEBUGGER_WORKING_DIRECTORY ${WF_RUNTIME_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT WorldFabric)

# Shaders compile to <name>.spv next to their source, because the app loads them from ./shader and ./Narball/shader.
# Generated .spv files are not committed, so glslc (from the Vulkan SDK) is required.
find_program(WF_GLSLC NAMES glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} "$ENV{VULKAN_SDK}/Bin" DOC "glslc shader compiler")
option(WF_COMPILE_SHADERS "Compile GLSL shaders to SPIR-V during the build" ON)
file(GLOB WF_SHADER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/shader/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shader/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shader/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*.comp)
if(WF_COMPILE_SHADERS AND WF_GLSLC)
    message(STATUS "Compiling shaders with ${WF_GLSLC}")
    set(WF_SHADER_OUTPUTS)
    foreach(shader ${WF_SHADER_SOURCES})
        file(RELATIVE_PATH shader_rel ${CMAKE_CURRENT_SOURCE_DIR} ${shader})
        set(spv ${shader}.spv)
        set(depfile ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_rel}.d)
        get_filename_component(depfile_dir ${depfile} DIRECTORY)
        add_custom_command(
            OUTPUT ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${depfile_dir}
            COMMAND ${WF_GLSLC} -MD -MF ${depfile} ${shader} -o ${spv}
            DEPENDS ${shader}
            DEPFILE ${depfile}
            COMMENT "Compiling shader ${shader_rel}"
            VERBATIM)
        list(APPEND WF_SHADER_OUTPUTS ${spv})
    endforeach()
    add_custom_target(shaders ALL DEPENDS ${WF_SHADER_OUTPUTS} SOURCES ${WF_SHADER_SOURCES})
    add_dependencies(WorldFabric shaders)
elseif(WF_COMPILE_SHADERS)
    message(WARNING "glslc not found (install the Vulkan SDK or set WF_GLSLC); shaders will not be compiled")
endif()

# OUTPUT can't use target-dependent generator expressions, hence WF_RUNTIME_DIR instead of $<TARGET_FILE_DIR>.
file(GLOB_RECURSE WF_RUNTIME_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/*
    ${CMAKE_CURRENT_SOURCE_DIR}/shader/*
    ${CMAKE_CURRENT_SOURCE_DIR}/Narball/asset/*
    ${CMAKE_CURRENT_SOURCE_DIR}/Narball/shader/*)
list(APPEND WF_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/server_settings.json)
if(WF_SHADER_OUTPUTS)
    list(REMOVE_ITEM WF_RUNTIME_SOURCES ${WF_SHADER_OUTPUTS})
    list(APPEND WF_RUNTIME_SOURCES ${WF_SHADER_OUTPUTS})
endif()
set(WF_RUNTIME_OUTPUTS)
foreach(src ${WF_RUNTIME_SOURCES})
    file(RELATIVE_PATH src_rel ${CMAKE_CURRENT_SOURCE_DIR} ${src})
    set(dst ${WF_RUNTIME_DIR}/${src_rel})
    add_custom_command(
        OUTPUT ${dst}
        COMMAND ${CMAKE_COMMAND} -E copy ${src} ${dst}
        DEPENDS ${src}
        VERBATIM)
    list(APPEND WF_RUNTIME_OUTPUTS ${dst})
endforeach()
add_custom_target(runtime_files ALL DEPENDS ${WF_RUNTIME_OUTPUTS})
add_dependencies(WorldFabric runtime_files)

file(GLOB WF_RUNTIME_DLLS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dll/*.dll)
add_custom_command(TARGET WorldFabric POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WF_RUNTIME_DLLS} $<TARGET_FILE_DIR:WorldFabric>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLLs next to WorldFabric.exe")

add_custom_target(run
    COMMAND $<TARGET_FILE:WorldFabric>
    DEPENDS WorldFabric
    WORKING_DIRECTORY ${WF_RUNTIME_DIR}
    USES_TERMINAL
    COMMENT "Running WorldFabric from the build directory")
