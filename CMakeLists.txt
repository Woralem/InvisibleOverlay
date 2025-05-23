cmake_minimum_required(VERSION 3.15)
project(OverlayChatClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_definitions(-D_WIN32_WINNT=0x0A00)

# Подключение библиотек через FetchContent
include(FetchContent)

# nlohmann/json
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib
FetchContent_Declare(
        cpphttplib_content
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cpphttplib_content)

# Исходные файлы
set(SOURCES
        src/main.cpp
        src/window.cpp
        src/chat_renderer.cpp
        src/network_client.cpp
        src/utils.cpp
)

set_source_files_properties(src/window.cpp PROPERTIES
        COMPILE_FLAGS "-Wno-unused-member-function"
)

add_executable(OverlayChatClient ${SOURCES})

target_include_directories(OverlayChatClient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${nlohmann_json_SOURCE_DIR}/include
        ${cpphttplib_content_SOURCE_DIR}
)

# Системные библиотеки Windows
target_link_libraries(OverlayChatClient PRIVATE
        user32
        gdi32
        ws2_32
)