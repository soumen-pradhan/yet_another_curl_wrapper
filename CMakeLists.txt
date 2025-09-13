cmake_minimum_required(VERSION 3.22)
project(yet_another_curl_wrapper VERSION 0.1.0 LANGUAGES C)

add_subdirectory(
    vendor/glfw
)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} "")

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
)

target_sources(${PROJECT_NAME}
    PUBLIC
        FILE_SET common
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/include
        FILES
            src/include/log.h
            src/include/app.h

        FILE_SET nuklearHeaders
        TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/Nuklear
        FILES
            vendor/Nuklear/nuklear.h

    PRIVATE
        src/main.c
        src/app.c
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        Vulkan::Vulkan
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)

get_filename_component(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
string(LENGTH "${PROJECT_SOURCE_DIR}/" YACW_BASE_DIR_LEN)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        YACW_BASE_DIR_LEN=${YACW_BASE_DIR_LEN}
)

# Shader files

find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(YACW_VERT_SHADER_SRC "${PROJECT_SOURCE_DIR}/src/shaders/shader.vert")
set(YACW_FRAG_SHADER_SRC "${PROJECT_SOURCE_DIR}/src/shaders/shader.frag")

set(YACW_VERT_SHADER_BIN ${PROJECT_BINARY_DIR}/shader.vert.spv)
set(YACW_FRAG_SHADER_BIN ${PROJECT_BINARY_DIR}/shader.frag.spv)

add_custom_command(
    OUTPUT ${YACW_VERT_SHADER_BIN} ${YACW_FRAG_SHADER_BIN}
    COMMAND ${GLSLC_EXECUTABLE} -o ${YACW_VERT_SHADER_BIN} ${YACW_VERT_SHADER_SRC}
    COMMAND ${GLSLC_EXECUTABLE} -o ${YACW_FRAG_SHADER_BIN} ${YACW_FRAG_SHADER_SRC}
    DEPENDS ${YACW_VERT_SHADER_SRC} ${YACW_FRAG_SHADER_SRC}
    COMMENT "Compiling shaders"
)

add_custom_target(YacwCompileShaders
    DEPENDS ${YACW_VERT_SHADER_BIN} ${YACW_FRAG_SHADER_BIN}
)

add_dependencies(${PROJECT_NAME} YacwCompileShaders)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        YACW_VERT_SHADER_PATH="${YACW_VERT_SHADER_BIN}"
        YACW_FRAG_SHADER_PATH="${YACW_FRAG_SHADER_BIN}"
)
