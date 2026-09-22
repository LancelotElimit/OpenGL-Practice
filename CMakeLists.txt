cmake_minimum_required(VERSION 3.20)

project(OpenGLPractice LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# GLFW: window creation, input, and the OpenGL context.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# GLM: CPU-side vector and matrix mathematics.
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# stb_image: loads PNG/JPG/PPM image files into CPU memory.
FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(stb_image)
if(NOT stb_image_POPULATED)
    FetchContent_Populate(stb_image)
endif()

# tinyobjloader: parses Wavefront OBJ model files.
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(tinyobjloader)
if(NOT tinyobjloader_POPULATED)
    FetchContent_Populate(tinyobjloader)
endif()

# Download one small PNG test image used by the texture-loading example.
set(TEST_TEXTURE_PATH "${CMAKE_SOURCE_DIR}/assets/basi6a08.png")
if(NOT EXISTS "${TEST_TEXTURE_PATH}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/master/tests/pngsuite/primary/basi6a08.png"
        "${TEST_TEXTURE_PATH}"
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()

set(TEST_TEXTURE_PATH_2 "${CMAKE_SOURCE_DIR}/assets/basn6a08.png")
if(NOT EXISTS "${TEST_TEXTURE_PATH_2}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/master/tests/pngsuite/primary/basn6a08.png"
        "${TEST_TEXTURE_PATH_2}"
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()

set(TEST_TEXTURE_PATH_3 "${CMAKE_SOURCE_DIR}/assets/basi4a08.png")
if(NOT EXISTS "${TEST_TEXTURE_PATH_3}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/master/tests/pngsuite/primary/basi4a08.png"
        "${TEST_TEXTURE_PATH_3}"
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()

# GLAD: loads modern OpenGL functions after GLFW creates the context.
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(glad)
if(NOT glad_POPULATED)
    FetchContent_Populate(glad)
    add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)
    glad_add_library(glad_gl_core_33 STATIC REPRODUCIBLE LOADER API gl:core=3.3)
endif()

add_executable(OpenGLPractice
    src/main_clean.cpp
)

target_link_libraries(OpenGLPractice
    PRIVATE
        glfw
        glad_gl_core_33
        glm::glm
        opengl32
)

target_include_directories(OpenGLPractice
    PRIVATE
        "${stb_image_SOURCE_DIR}"
        "${tinyobjloader_SOURCE_DIR}"
)
