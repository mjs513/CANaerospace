cmake_minimum_required(VERSION 2.6)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Default build is RelWithDebInfo" FORCE)
endif()

project(canaerospace)

#
# libcanaerospace
#
# -DCANAEROSPACE_DEBUG=1 enables the tracing feature that writes debug info into stdout.
# Normally this feature should only be used at library development.
#
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O1 -g")
set(CMAKE_C_FLAGS_RELEASE "-O1 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g3 -DCANAEROSPACE_DEBUG=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror -pedantic -fstrict-aliasing")

include_directories(include)
file(GLOB CFILES      RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
file(GLOB HEADERS     RELATIVE ${CMAKE_SOURCE_DIR} "src/*.h")
file(GLOB SRV_CFILES  RELATIVE ${CMAKE_SOURCE_DIR} "src/services/*.c")
file(GLOB SRV_HEADERS RELATIVE ${CMAKE_SOURCE_DIR} "src/services/*.h")

add_library(canaerospace SHARED ${CFILES} ${SRV_CFILES})

install(FILES ${CMAKE_BINARY_DIR}/libcanaerospace.so          DESTINATION lib     COMPONENT Lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME} DESTINATION include COMPONENT Lib)

install(FILES ${CFILES} ${HEADERS}         DESTINATION src/${PROJECT_NAME}          COMPONENT Src)
install(FILES ${SRV_CFILES} ${SRV_HEADERS} DESTINATION src/${PROJECT_NAME}/services COMPONENT Src)

#
# tests
#
set(CMAKE_CXX_FLAGS "-g -DCANAEROSPACE_DEBUG=1 -Wall -Wextra -Werror -pedantic")
find_package(GTest QUIET)
if (GTEST_FOUND)
    find_package(Threads REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    file(GLOB_RECURSE TEST_CFILES RELATIVE ${CMAKE_SOURCE_DIR} "test/*.cpp")
    add_executable(tests EXCLUDE_FROM_ALL ${TEST_CFILES})
    add_dependencies(tests canaerospace)

    target_link_libraries(tests ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(tests ${CMAKE_BINARY_DIR}/libcanaerospace.so)

    add_custom_command(TARGET tests POST_BUILD
                       COMMAND "./tests"
                       WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    message(">> Say 'make tests' to make tests")
else (GTEST_FOUND)
    message(">> Google test is not found, you will not be able to run tests")
endif (GTEST_FOUND)
