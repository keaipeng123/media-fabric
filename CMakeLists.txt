cmake_minimum_required(VERSION 3.16)

project(media-fabric VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MEDIA_FABRIC_BUILD_LEGACY_SERVICES "Build historical SipSupService and SipSubService targets" OFF)
option(MEDIA_FABRIC_ENABLE_PJSIP "Enable PJSIP transport adapter for real SIP networking" ON)
option(MEDIA_FABRIC_ENABLE_JRTPLIB "Enable experimental JRTPLIB RTP adapter for media-fabric" OFF)
set(MEDIA_FABRIC_SELF_TEST_CONFIG "conf/media-fabric.conf" CACHE STRING "Config file used by media-fabric-self-test")
if(IS_ABSOLUTE "${MEDIA_FABRIC_SELF_TEST_CONFIG}")
    set(MEDIA_FABRIC_SELF_TEST_CONFIG_PATH "${MEDIA_FABRIC_SELF_TEST_CONFIG}")
else()
    set(MEDIA_FABRIC_SELF_TEST_CONFIG_PATH "${CMAKE_SOURCE_DIR}/${MEDIA_FABRIC_SELF_TEST_CONFIG}")
endif()

add_library(gb28181_common STATIC
    src/common/src/ConfReader.cpp
    src/common/src/ECEventPoll.cpp
    src/common/src/ECSocket.cpp
    src/common/src/ECThread.cpp
    src/common/src/Logger.cpp
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

add_executable(media-fabric
    src/main.cpp
    src/core/BusinessQueryService.cpp
    src/core/BusinessState.cpp
    src/core/DigestAuth.cpp
    src/core/GB28181Node.cpp
    src/core/ManagementServer.cpp
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

add_executable(mfcli src/mfcli.cpp)

if(MEDIA_FABRIC_ENABLE_PJSIP)
    target_sources(media-fabric PRIVATE
        src/adapters/pjsip/PjsipStackAdapter.cpp
    )
endif()

if(MEDIA_FABRIC_ENABLE_JRTPLIB)
    target_sources(media-fabric PRIVATE
        src/adapters/jrtplib/JrtplibRtpSessionAdapter.cpp
    )
endif()

target_include_directories(media-fabric PRIVATE
    src/core
    src/capabilities
    src/media
)

if(MEDIA_FABRIC_ENABLE_PJSIP)
    find_package(OpenSSL REQUIRED)

    target_include_directories(media-fabric PRIVATE
        src/adapters/pjsip
        3rd/include/pjsip
    )
    target_compile_definitions(media-fabric PRIVATE
        GB28181_ENABLE_PJSIP
    )
    target_link_directories(media-fabric PRIVATE
        3rd/lib
    )
    target_link_libraries(media-fabric PRIVATE
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

if(MEDIA_FABRIC_ENABLE_JRTPLIB)
    target_include_directories(media-fabric PRIVATE
        src/adapters/jrtplib
        3rd/include
    )
    target_compile_definitions(media-fabric PRIVATE
        GB28181_ENABLE_JRTPLIB
    )
    find_library(JRTPLIB_LIBRARY NAMES jrtp jrtplib PATHS 3rd/lib NO_DEFAULT_PATH)
    find_library(JTHREAD_LIBRARY NAMES jthread PATHS 3rd/lib NO_DEFAULT_PATH)
    if(NOT JRTPLIB_LIBRARY)
        message(FATAL_ERROR "MEDIA_FABRIC_ENABLE_JRTPLIB=ON requires libjrtp in 3rd/lib")
    endif()
    if(NOT JTHREAD_LIBRARY)
        message(FATAL_ERROR "MEDIA_FABRIC_ENABLE_JRTPLIB=ON requires libjthread in 3rd/lib")
    endif()
    target_link_libraries(media-fabric PRIVATE
        ${JRTPLIB_LIBRARY}
        ${JTHREAD_LIBRARY}
    )
endif()

target_compile_options(media-fabric PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(media-fabric PRIVATE
    gb28181_common
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(
        NAME media-fabric-self-test
        COMMAND $<TARGET_FILE:media-fabric> -c ${MEDIA_FABRIC_SELF_TEST_CONFIG_PATH} --self-test
    )
    set_tests_properties(media-fabric-self-test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        PASS_REGULAR_EXPRESSION "REGISTER_AUTH_RETRY=ok.*INVITE_ACK=ok.*sessions=[0-9]+.*endpoints=1.*upstream_peers=1.*downstream_peers=1.*routes=11.*sent_messages=[0-9]+"
    )
endif()

if(MEDIA_FABRIC_BUILD_LEGACY_SERVICES)
    add_subdirectory(SipSupService/cmake SipSupService_legacy)
    add_subdirectory(SipSubService/cmake SipSubService_legacy)
endif()
