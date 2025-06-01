cmake_minimum_required(VERSION 3.22)
project(yet_another_curl_wrapper VERSION 0.1.0 LANGUAGES C)

add_subdirectory(
    vendor/glfw
)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} "")

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/main.c
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
