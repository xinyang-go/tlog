cmake_minimum_required(VERSION 3.15)
project(tlog VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wall -Werror=return-type -Wno-unused-variable)
if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_compile_options(-O2 -flto )
elseif(CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    add_compile_options(-O2 -flto -g)
elseif(CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
    add_compile_options(-Os -flto)
else()
    add_compile_options(-O0 -g)
endif()

find_package(Threads REQUIRED)

add_library(tlog SHARED src/tlog.cpp src/relay.c)
target_include_directories(tlog PRIVATE include)
target_link_libraries(tlog Threads::Threads)
set_target_properties(tlog PROPERTIES 
                      PUBLIC_HEADER include/tlog/tlog.hpp)

add_executable(tlog_attach src/attach.c)

option(BUILD_EXAMPLE ON)
if(BUILD_EXAMPLE)    
    add_executable(example example/example.cpp)
    target_link_libraries(example tlog)
    target_include_directories(example PRIVATE include)
endif()

install(
    TARGETS tlog tlog_attach
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/tlog
)

