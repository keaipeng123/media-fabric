cmake_minimum_required(VERSION 3.16)

project(GB28181Server VERSION 0.1.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GB28181_BUILD_LEGACY_SERVICES "Build historical SipSupService and SipSubService targets" OFF)

add_library(gb28181_common STATIC
    src/common/src/ConfReader.cpp
    src/common/src/ECEventPoll.cpp
    src/common/src/ECSocket.cpp
    src/common/src/ECThread.cpp
    src/common/src/SipMessage.cpp
    src/common/src/ThreadPool.cpp
    src/common/src/XmlParser.cpp
)

target_include_directories(gb28181_common
    PUBLIC
        src/common/include
        3rd/include
        3rd/include/tinyxml2
)

target_compile_options(gb28181_common PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(gb28181_common
    PUBLIC
        Threads::Threads
)

add_executable(gb28181-server
    src/main.cpp
)

target_compile_options(gb28181-server PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(gb28181-server PRIVATE
    gb28181_common
)

if(GB28181_BUILD_LEGACY_SERVICES)
    add_subdirectory(SipSupService/cmake SipSupService_legacy)
    add_subdirectory(SipSubService/cmake SipSubService_legacy)
endif()
