cmake_minimum_required(VERSION 3.15)
project(OverlayChatClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(-D_WIN32_WINNT=0x0A00)

# FetchContent для nlohmann/json
include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- cpp-httplib ---
# Вариант 1: Просто положить httplib.h в проект и указать путь
# target_include_directories(OverlayChatClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include) # если httplib.h в CMAKE_CURRENT_SOURCE_DIR/include

# Вариант 2: FetchContent для cpp-httplib
FetchContent_Declare(
        cpphttplib_content # Имя для FetchContent
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3 # Используйте актуальную версию
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cpphttplib_content)
# cpp-httplib обычно header-only, но если бы это была библиотека:
# target_link_libraries(OverlayChatClient PRIVATE cpp-httplib)
# Для header-only достаточно добавить include_directories, если FetchContent сам не сделает
# Обычно FetchContent_MakeAvailable создает target, который можно линковать или он добавляет include dirs.
# Для cpp-httplib может потребоваться target_include_directories(OverlayChatClient PRIVATE ${cpphttplib_content_SOURCE_DIR})

add_executable(OverlayChatClient main.cpp)

target_include_directories(OverlayChatClient PRIVATE
        ${nlohmann_json_SOURCE_DIR}/include
        ${cpphttplib_content_SOURCE_DIR} # Путь к исходникам cpp-httplib, где лежит httplib.h
)

# Для cpp-httplib с HTTP нам не нужны сложные линковки SSL.
# Для Windows могут понадобиться ws2_32 и wininet для сетевых функций.
target_link_libraries(OverlayChatClient PRIVATE
        user32
        gdi32
        ws2_32   # Для сокетов Windows
        # wininet  # Может понадобиться, но cpp-httplib обычно использует ws2_32
)

# Если cpp-httplib сконфигурирован для HTTPS с OpenSSL, то нужны будут библиотеки OpenSSL.
# Но для вашего локального HTTP Flask-сервера это не требуется.
# if(CPPHTTPLIB_OPENSSL_SUPPORT AND MINGW)
#    find_package(OpenSSL REQUIRED)
#    target_link_libraries(OverlayChatClient PRIVATE OpenSSL::SSL OpenSSL::Crypto)
# endif()