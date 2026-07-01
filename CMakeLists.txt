cmake_minimum_required(VERSION 3.16)

project(GB28181Server VERSION 0.1.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GB28181_BUILD_LEGACY_SERVICES "Build historical SipSupService and SipSubService targets" OFF)
option(GB28181_ENABLE_PJSIP "Enable experimental PJSIP transport adapter for gb28181-server" OFF)
option(GB28181_ENABLE_JRTPLIB "Enable experimental JRTPLIB RTP adapter for gb28181-server" OFF)

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
    src/core/BusinessQueryService.cpp
    src/core/BusinessState.cpp
    src/core/DigestAuth.cpp
    src/core/GB28181Node.cpp
    src/core/ManscdpMessage.cpp
    src/core/NodeConfig.cpp
    src/core/PeerRegistry.cpp
    src/core/SessionManager.cpp
    src/core/SipStack.cpp
    src/core/SipTransport.cpp
    src/core/TaskScheduler.cpp
    src/media/MediaManager.cpp
    src/media/ElementaryStream.cpp
    src/media/MediaFrameSink.cpp
    src/media/ProgramStream.cpp
    src/media/RtpPacket.cpp
    src/media/RtpSessionAdapter.cpp
    src/media/SdpSession.cpp
    src/media/StreamFileFrameSource.cpp
    src/capabilities/GB28181Capabilities.cpp
    src/capabilities/LifecycleCapability.cpp
    src/capabilities/StandardCapabilities.cpp
)

if(GB28181_ENABLE_PJSIP)
    target_sources(gb28181-server PRIVATE
        src/adapters/pjsip/PjsipStackAdapter.cpp
    )
endif()

if(GB28181_ENABLE_JRTPLIB)
    target_sources(gb28181-server PRIVATE
        src/adapters/jrtplib/JrtplibRtpSessionAdapter.cpp
    )
endif()

target_include_directories(gb28181-server PRIVATE
    src/core
    src/capabilities
    src/media
)

if(GB28181_ENABLE_PJSIP)
    find_package(OpenSSL REQUIRED)

    target_include_directories(gb28181-server PRIVATE
        src/adapters/pjsip
        3rd/include/pjsip
    )
    target_compile_definitions(gb28181-server PRIVATE
        GB28181_ENABLE_PJSIP
    )
    target_link_directories(gb28181-server PRIVATE
        3rd/lib
    )
    target_link_libraries(gb28181-server PRIVATE
        -lpjsua2-x86_64-unknown-linux-gnu
        -lpjsua-x86_64-unknown-linux-gnu
        -lpjsip-ua-x86_64-unknown-linux-gnu
        -lpjsip-simple-x86_64-unknown-linux-gnu
        -lpjsip-x86_64-unknown-linux-gnu
        -lpjmedia-codec-x86_64-unknown-linux-gnu
        -lpjmedia-videodev-x86_64-unknown-linux-gnu
        -lpjmedia-audiodev-x86_64-unknown-linux-gnu
        -lpjmedia-x86_64-unknown-linux-gnu
        -lpjnath-x86_64-unknown-linux-gnu
        -lpjlib-util-x86_64-unknown-linux-gnu
        -lsrtp-x86_64-unknown-linux-gnu
        -lresample-x86_64-unknown-linux-gnu
        -lgsmcodec-x86_64-unknown-linux-gnu
        -lspeex-x86_64-unknown-linux-gnu
        -lilbccodec-x86_64-unknown-linux-gnu
        -lg7221codec-x86_64-unknown-linux-gnu
        -lyuv-x86_64-unknown-linux-gnu
        -lwebrtc-x86_64-unknown-linux-gnu
        -lpj-x86_64-unknown-linux-gnu
        -lpjsdp-x86_64-unknown-linux-gnu
        -lasound
        ${OPENSSL_LIBRARIES}
    )
endif()

if(GB28181_ENABLE_JRTPLIB)
    target_include_directories(gb28181-server PRIVATE
        src/adapters/jrtplib
        3rd/include
    )
    target_compile_definitions(gb28181-server PRIVATE
        GB28181_ENABLE_JRTPLIB
    )
    find_library(JRTPLIB_LIBRARY NAMES jrtp jrtplib PATHS 3rd/lib NO_DEFAULT_PATH)
    find_library(JTHREAD_LIBRARY NAMES jthread PATHS 3rd/lib NO_DEFAULT_PATH)
    if(NOT JRTPLIB_LIBRARY)
        message(FATAL_ERROR "GB28181_ENABLE_JRTPLIB=ON requires libjrtp in 3rd/lib")
    endif()
    if(NOT JTHREAD_LIBRARY)
        message(FATAL_ERROR "GB28181_ENABLE_JRTPLIB=ON requires libjthread in 3rd/lib")
    endif()
    target_link_libraries(gb28181-server PRIVATE
        ${JRTPLIB_LIBRARY}
        ${JTHREAD_LIBRARY}
    )
endif()

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
