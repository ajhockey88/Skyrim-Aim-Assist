cmake_minimum_required(VERSION 3.21)

project(AimAssist VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "ajhockey88"
    USE_ADDRESS_LIBRARY
    SOURCES src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
