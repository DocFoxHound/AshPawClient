cmake_minimum_required(VERSION 3.24)

project(AshPawClient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASHPAW_BUILD_TESTS "Build AshPaw test suite" ON)
option(ASHPAW_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ASHPAW_BUILD_CLIENT "Build the OpenGL client and engine targets" ON)
option(ASHPAW_BUILD_HANDSHAKE_SERVER "Build the development handshake responder" ON)

include(FetchContent)

function(ashpaw_enable_warnings target_name)
    if(NOT ASHPAW_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.10
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    SDL2_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-2.8.2
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
    GIT_SHALLOW TRUE
)

if(ASHPAW_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
endif()

set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)

if(NOT ASHPAW_BUILD_CLIENT AND ASHPAW_BUILD_TESTS)
    message(WARNING "ASHPAW_BUILD_TESTS requires ASHPAW_BUILD_CLIENT. Disabling tests for this configure.")
    set(ASHPAW_BUILD_TESTS OFF CACHE BOOL "Build AshPaw test suite" FORCE)
endif()

FetchContent_MakeAvailable(enet spdlog nlohmann_json)
if(ASHPAW_BUILD_CLIENT)
    FetchContent_MakeAvailable(SDL2 SDL2_image imgui)
endif()
if(ASHPAW_BUILD_TESTS)
    FetchContent_MakeAvailable(Catch2)
endif()

if(ASHPAW_BUILD_CLIENT)
    add_library(imgui_support STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
    )
    target_include_directories(imgui_support
        PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_support PUBLIC SDL2::SDL2)
    ashpaw_enable_warnings(imgui_support)

    find_package(OpenGL REQUIRED)
    if(NOT OpenGL_FOUND)
        message(FATAL_ERROR "Desktop OpenGL development libraries were not found. Install Mesa/OpenGL dev packages on Linux, or use the native OpenGL SDK on macOS/Windows.")
    endif()
endif()

if(NOT TARGET ashpaw_protocol)
    add_library(ashpaw_protocol INTERFACE)
endif()

if(ASHPAW_BUILD_CLIENT)
    add_library(ashpaw_engine STATIC
        engine/src/assets/AssetManager.cpp
        engine/src/camera/Camera2D.cpp
        engine/src/config/Config.cpp
        engine/src/input/InputSystem.cpp
        engine/src/net/HandshakeProtocol.cpp
        engine/src/net/NetworkClient.cpp
        engine/src/net/TemporaryProtocol.cpp
        engine/src/platform/PlatformContext.cpp
        engine/src/render/RenderSystem.cpp
        engine/src/ui/UiLayer.cpp
        engine/src/util/Log.cpp
        engine/src/world/ClientWorld.cpp
    )
    target_include_directories(ashpaw_engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
            ${enet_SOURCE_DIR}/include
    )
    target_link_libraries(ashpaw_engine
        PUBLIC
            SDL2::SDL2
            SDL2_image::SDL2_image
            enet
            imgui_support
            nlohmann_json::nlohmann_json
            spdlog::spdlog
            OpenGL::GL
            ashpaw_protocol
    )
    target_compile_definitions(ashpaw_engine PUBLIC SDL_MAIN_HANDLED)
    ashpaw_enable_warnings(ashpaw_engine)

    add_executable(ashpaw_client
        client/src/ClientApp.cpp
        client/src/main.cpp
    )
    target_include_directories(ashpaw_client
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/client/include
    )
    target_link_libraries(ashpaw_client PRIVATE ashpaw_engine)
    ashpaw_enable_warnings(ashpaw_client)

    target_compile_definitions(ashpaw_client PRIVATE ASHPAW_DEFAULT_CONFIG_PATH="assets/config/client.json")
endif()

if(ASHPAW_BUILD_HANDSHAKE_SERVER)
    add_executable(ashpaw_handshake_server
        tools/handshake_server/main.cpp
        engine/src/net/HandshakeProtocol.cpp
        engine/src/net/TemporaryProtocol.cpp
        engine/src/util/Log.cpp
    )
    target_include_directories(ashpaw_handshake_server
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
            ${enet_SOURCE_DIR}/include
    )
    target_link_libraries(ashpaw_handshake_server
        PRIVATE
            enet
            nlohmann_json::nlohmann_json
            spdlog::spdlog
    )
    ashpaw_enable_warnings(ashpaw_handshake_server)
endif()

if(ASHPAW_BUILD_CLIENT AND ASHPAW_BUILD_TESTS)
    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

    add_executable(ashpaw_tests
        tests/EngineTests.cpp
    )
    target_include_directories(ashpaw_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/client/include)
    target_link_libraries(ashpaw_tests PRIVATE ashpaw_engine Catch2::Catch2WithMain)
    ashpaw_enable_warnings(ashpaw_tests)

    include(Catch)
    catch_discover_tests(ashpaw_tests)
endif()
