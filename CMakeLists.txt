cmake_minimum_required(VERSION 3.21)

project(AimAssist VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# CommonLibSSE-NG (alandtse) from source: the old Color-Glass vcpkg port predates the
# Skyrim 1.7.99+ runtime and its format 5 Address Library, so SKSE refuses to load it there.
# On a release tag in GitHub Actions it links the project's prebuilt bundle instead of compiling.
include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG v13.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(CommonLibSSE)

find_package(spdlog CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "ajhockey88"
    USE_ADDRESS_LIBRARY
    SOURCES src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
