cmake_minimum_required(VERSION 3.22)
project(yet_another_curl_wrapper VERSION 0.1.0 LANGUAGES C)

add_subdirectory(
    vendor/glfw
)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME} "")

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/main.c
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        OpenGL::GL
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
