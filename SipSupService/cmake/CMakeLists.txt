cmake_minimum_required(VERSION 3.16)

project(SipSupService LANGUAGES C CXX)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(EXE_NAME SipSupService)

message(STATUS "operation system is ${CMAKE_SYSTEM}")

file(GLOB SIP_SUP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/h26x/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../src/h26x/*.cpp"
)

add_executable(${EXE_NAME} ${SIP_SUP_SOURCES})

target_compile_features(${EXE_NAME} PRIVATE cxx_std_11)
target_compile_options(${EXE_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-g>
)

target_include_directories(${EXE_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../include/h26x"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/pjsip"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/jrtplib3"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/jthread"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/tinyxml2"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/libevent"
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/include/mpege"
)

target_link_directories(${EXE_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../3rd/lib"
)

target_link_libraries(${EXE_NAME} PRIVATE
    libglog.a
    libgflags.a
    Threads::Threads
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
    libjrtp.a
    libjthread.a
    libtinyxml2.a
    libjsoncpp.a
    libevent.a
    libevent_pthreads.a
    libmpeg.a
    ${OPENSSL_LIBRARIES}
)
