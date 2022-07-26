cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(dson)

set(CMAKE_CXX_STANDARD 17)

set(LIB_NAME dson)

add_library(${LIB_NAME} INTERFACE)
#add_library(${LIB_NAME} STATIC)

target_include_directories(${LIB_NAME}
  INTERFACE
  include/
)
#target_include_directories(${LIB_NAME}
#  PUBLIC
#  include/
#)
#set_target_properties(${LIB_NAME} PROPERTIES
#    CXX_STANDARD 17
#    CXX_STANDARD_REQUIRED ON
#)
#file(GLOB_RECURSE LIB_SRC
#       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
#   )
#target_sources(${LIB_NAME}
#    PRIVATE
#    ${LIB_SRC}
#    )

option(BUILD_EXAMPLES "Build examples " ON)
option(BUILD_PERFORMANCE_TESTS  "Build performance tests " ON)
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_EXAMPLES)
    message(STATUS "building dson examples..")
    add_subdirectory(examples)
endif()

#if(BUILD_PERFORMANCE_TESTS)
#    message(STATUS "building performance tests..")
#    add_subdirectory(performance_tests)
#endif()

if(BUILD_TESTING)
    message(STATUS "building googletest..")
    # https://google.github.io/googletest/quickstart-cmake.html
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    message(STATUS "building thread_highways unit tests..")
    add_subdirectory(tests)
endif()

