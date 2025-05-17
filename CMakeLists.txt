# Copyright 2023 ShenMian
# License(Apache-2.0)

cmake_minimum_required(VERSION 3.12)

project(sokoban CXX)

set(BUILD_SHARED_LIBS FALSE)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0)
FetchContent_MakeAvailable(SFML)

include(FetchContent)
FetchContent_Declare(SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG 3.3.2)
FetchContent_MakeAvailable(SQLiteCpp)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE SFML::Graphics SFML::Audio SQLiteCpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/level/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/assets/level/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/img/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/assets/img/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/assets/audio/)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sokoban.rc)
endif()

if (WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}> COMMAND_EXPAND_LISTS)
endif()

install(TARGETS ${PROJECT_NAME})
