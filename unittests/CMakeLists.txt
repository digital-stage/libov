cmake_minimum_required(VERSION 3.16.0)

project(libovTests
        LANGUAGES CXX
        VERSION 0.1
        DESCRIPTION "Tests for libov")

set(CMAKE_CXX_STANDARD 20)
set(TESTOVBOXVERSION "0.7.26-280b228-modified")
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../cmake/modules)
set(LIBOV_DIR ${PROJECT_SOURCE_DIR}/../src)
set(LIBTASCAR_DIR ${PROJECT_SOURCE_DIR}/../tascar/libtascar)

# Dependencies
find_package(Threads REQUIRED)
find_package(XercesC REQUIRED)
find_package(Jack REQUIRED)
find_package(LibSndFile REQUIRED)
find_package(LibSamplerate REQUIRED)
find_package(SoundIo REQUIRED)
find_package(FFTW REQUIRED)
find_package(CURL REQUIRED)
find_package(GTest REQUIRED)
if (UNIX AND NOT APPLE)
    find_package(ALSA REQUIRED)
endif ()

add_compile_definitions(OVBOXVERSION="${TESTOVBOXVERSION}")


include_directories(${GTEST_INCLUDE_DIRS} ${LIBOV_DIR} ${LIBTASCAR_DIR}/include)



file(GLOB
        HEADER
        ${PROJECT_SOURCE_DIR}/src/*.h
        )

file(GLOB
        SOURCES
        ${PROJECT_SOURCE_DIR}/*.cc
        ${LIBOV_DIR}/callerlist.cc
        ${LIBOV_DIR}/common.cc
        ${LIBOV_DIR}/errmsg.cc
        ${LIBOV_DIR}/ov_client_orlandoviols.cc
        ${LIBOV_DIR}/ov_client_udpsocket.cc
        ${LIBOV_DIR}/ov_client_listsounddevs.cc
        ${LIBOV_DIR}/ov_tools.cc
        ${LIBOV_DIR}/ovboxclient.cc
        ${LIBOV_DIR}/ov_types.cc
        ${LIBOV_DIR}/soundcardtools.cc
        ${LIBOV_DIR}/udpsocket.cc
        ${LIBTASCAR_DIR}/src/audiochunks.cc
        ${LIBTASCAR_DIR}/src/coordinates.cc
        ${LIBTASCAR_DIR}/src/errorhandling.cc
        ${LIBTASCAR_DIR}/src/tscconfig.cc
        )

# link
set(EXTERNAL_LIBRARIES
        ${GTEST_LIBRARIES}
        ${CURL_LIBRARIES}
        ${LIBSNDFILE_LIBRARIES}
        ${LIBSAMPLERATE_LIBRARIES}
        ${SOUNDIO_LIBRARY}
        ${ALSA_LIBRARY}
        ${XercesC_LIBRARIES}
        ${LIBXML++_LIBRARIES}
        ${LIBXML++_LIBRARIES}
        pthread )

enable_testing()

# Link runTests with what we want to test and the GTest and pthread library
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${EXTERNAL_LIBRARIES})
gtest_discover_tests(${PROJECT_NAME})