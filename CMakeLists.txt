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

# Dear ImGui: in-window diagnostics with the GLFW and OpenGL 3 backends.
option(LANCELOT_BUILD_EDITOR "Build the ImGui editor host" ON)
if(LANCELOT_BUILD_EDITOR)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6-docking
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(imgui PUBLIC glfw opengl32)

FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 18cef5e031d8c6973d80284c67f60549fafd78c1)
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()
add_library(imguizmo STATIC "${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp")
target_include_directories(imguizmo PUBLIC "${imguizmo_SOURCE_DIR}/src")
target_link_libraries(imguizmo PUBLIC imgui)

endif() # LANCELOT_BUILD_EDITOR

# GLM: CPU-side vector and matrix mathematics.
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# tinygltf: parses glTF 2.0 scenes, skins, and animations.
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.7
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)
endif()

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
option(LANCELOT_FETCH_SAMPLE_ASSETS "Download assets for the Sandbox sample project" ON)
if(LANCELOT_FETCH_SAMPLE_ASSETS)
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

# Small CC0 reference assets from Poly Haven for the real PBR pipeline.
set(POLY_HAVEN_ASSETS
    "sunset_jhbcentral_1k.hdr|https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/sunset_jhbcentral_1k.hdr"
    "concrete_diff_1k.jpg|https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete/concrete_diff_1k.jpg"
    "concrete_nor_gl_1k.jpg|https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete/concrete_nor_gl_1k.jpg"
    "concrete_rough_1k.jpg|https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete/concrete_rough_1k.jpg"
    "concrete_ao_1k.jpg|https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete/concrete_ao_1k.jpg"
)
foreach(ASSET_ENTRY IN LISTS POLY_HAVEN_ASSETS)
    string(REPLACE "|" ";" ASSET_FIELDS "${ASSET_ENTRY}")
    list(GET ASSET_FIELDS 0 ASSET_NAME)
    list(GET ASSET_FIELDS 1 ASSET_URL)
    set(ASSET_PATH "${CMAKE_SOURCE_DIR}/assets/${ASSET_NAME}")
    if(NOT EXISTS "${ASSET_PATH}")
        file(DOWNLOAD
            "${ASSET_URL}"
            "${ASSET_PATH}"
            SHOW_PROGRESS
            TLS_VERIFY ON
        )
    endif()
endforeach()

set(SIMPLE_SKIN_PATH "${CMAKE_SOURCE_DIR}/assets/SimpleSkin.gltf")
if(NOT EXISTS "${SIMPLE_SKIN_PATH}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/SimpleSkin/glTF-Embedded/SimpleSkin.gltf"
        "${SIMPLE_SKIN_PATH}"
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()

# A compact textured glTF scene for exercising the new importer.
set(BOX_TEXTURED_PATH "${CMAKE_SOURCE_DIR}/assets/BoxTextured.glb")
if(NOT EXISTS "${BOX_TEXTURED_PATH}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/BoxTextured/glTF-Binary/BoxTextured.glb"
        "${BOX_TEXTURED_PATH}"
        SHOW_PROGRESS
        TLS_VERIFY ON
        STATUS BOX_TEXTURED_DOWNLOAD
    )
    list(GET BOX_TEXTURED_DOWNLOAD 0 BOX_TEXTURED_STATUS)
    if(NOT BOX_TEXTURED_STATUS EQUAL 0)
        file(REMOVE "${BOX_TEXTURED_PATH}")
        message(WARNING "Optional BoxTextured.glb could not be downloaded")
    endif()
endif()

endif() # LANCELOT_FETCH_SAMPLE_ASSETS

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

add_library(LancelotEngine STATIC
    src/EngineApplication.cpp
    src/Project.cpp
    src/Input.cpp
    src/AssetLibrary.cpp
    src/SimulationRuntime.cpp
    src/RuntimeSession.cpp
    src/AssetPaths.cpp
    src/BlurBuffer.cpp
    src/Camera.cpp
    src/EnvironmentIBL.cpp
    src/Fluid2D.cpp
    src/Frustum.cpp
    src/FluidSystem.cpp
    src/GltfAnimatedModel.cpp
    src/GltfScene.cpp
    src/GpuParticleSystem.cpp
    src/Material.cpp
    src/Mesh.cpp
    src/Model.cpp
    src/PbrMaterial.cpp
    src/ParticleSystem.cpp
    src/RenderTarget.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/ShaderProgram.cpp
    src/ShadowMap.cpp
    src/Texture2D.cpp
    src/TextureCache.cpp
    src/Window.cpp
)
add_library(LancelotProjectScripts STATIC projects/Sandbox/Scripts/PlayerController.cpp)
target_include_directories(LancelotProjectScripts PUBLIC "${CMAKE_SOURCE_DIR}/projects/Sandbox/Scripts")
target_link_libraries(LancelotProjectScripts PUBLIC LancelotEngine)
add_executable(LancelotPlayer runtime/PlayerMain.cpp)
target_link_libraries(LancelotPlayer PRIVATE LancelotEngine LancelotProjectScripts)
if(LANCELOT_BUILD_EDITOR)
    add_library(LancelotEditor STATIC
        editor/src/EditorApplication.cpp
        editor/src/EditorInputRouter.cpp
        editor/src/EditorWorkspace.cpp
        editor/src/EditorLocale.cpp
        editor/src/EditorTheme.cpp
        editor/src/HierarchyPanel.cpp
        editor/src/InspectorPanel.cpp
        editor/src/ResourceBrowserPanel.cpp
        editor/src/OutputPanel.cpp
        editor/src/ProfilerPanel.cpp
        editor/src/SmokePanel.cpp
        editor/src/WaterPanel.cpp
        editor/src/SelectionController.cpp
    )
    target_compile_options(LancelotEditor PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_include_directories(LancelotEditor PUBLIC "${CMAKE_SOURCE_DIR}/editor/include")
    target_link_libraries(LancelotEditor PUBLIC LancelotEngine PRIVATE imgui imguizmo imm32)
    add_executable(OpenGLPractice editor/src/EditorMain.cpp)
    target_link_libraries(OpenGLPractice PRIVATE LancelotEditor LancelotProjectScripts)
endif()

set(SHADER_FILES
    shaders/main.vert
    shaders/main.frag
    shaders/light_marker.vert
    shaders/light_marker.frag
    shaders/depth.vert
    shaders/depth.frag
    shaders/debug.vert
    shaders/debug.frag
    shaders/postprocess.vert
    shaders/postprocess.frag
    shaders/bright_pass.frag
    shaders/blur.frag
    shaders/cubemap_capture.vert
    shaders/procedural_environment.frag
    shaders/equirectangular_to_cubemap.frag
    shaders/irradiance_convolution.frag
    shaders/prefilter_environment.frag
    shaders/brdf_lut.vert
    shaders/brdf_lut.frag
    shaders/skybox.vert
    shaders/skybox.frag
    shaders/skinned.vert
    shaders/skinned.frag
    shaders/particle.vert
    shaders/particle.frag
    shaders/gltf_scene.vert
    shaders/gltf_scene.frag
    shaders/fluid.vert
    shaders/fluid.frag
    shaders/fluid2d.frag
    shaders/smoke_scene.vert
    shaders/smoke_scene.frag
    shaders/gpu_particle_update.vert
    shaders/gpu_particle_update.frag
    shaders/gpu_particle.vert
    shaders/gpu_particle.frag
)

add_custom_command(TARGET LancelotPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:LancelotPlayer>/shaders")
target_sources(LancelotEngine PRIVATE ${SHADER_FILES})
set_source_files_properties(${SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

if(LANCELOT_BUILD_EDITOR)
add_custom_command(
    TARGET OpenGLPractice POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:OpenGLPractice>/shaders"
    COMMENT "Copying GLSL shader files next to the executable"
)
endif()

target_link_libraries(LancelotEngine
    PUBLIC
        glfw
        glad_gl_core_33
        glm::glm
        opengl32
)

target_include_directories(LancelotEngine
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
    PRIVATE
        "${stb_image_SOURCE_DIR}"
        "${tinyobjloader_SOURCE_DIR}"
        "${tinygltf_SOURCE_DIR}"
)

include(CTest)
if(BUILD_TESTING)
    if(LANCELOT_BUILD_EDITOR)
    add_executable(EditorLocaleTests tests/EditorLocaleTests.cpp)
    target_link_libraries(EditorLocaleTests PRIVATE LancelotEditor imgui)
    add_test(NAME EditorLocaleTests COMMAND EditorLocaleTests)
    add_executable(SceneEditorTests tests/SceneEditorTests.cpp)
    target_include_directories(SceneEditorTests PRIVATE "${CMAKE_SOURCE_DIR}/include")
    target_link_libraries(SceneEditorTests PRIVATE LancelotEditor imguizmo)
    add_test(NAME SceneEditorTests COMMAND SceneEditorTests)
    add_executable(SceneWorkflowTests tests/SceneWorkflowTests.cpp)
    target_link_libraries(SceneWorkflowTests PRIVATE LancelotEditor)
    add_test(NAME SceneWorkflowTests COMMAND SceneWorkflowTests)
    add_executable(PlaySessionTests tests/PlaySessionTests.cpp)
    target_link_libraries(PlaySessionTests PRIVATE LancelotEditor LancelotProjectScripts)
    add_test(NAME PlaySessionTests COMMAND PlaySessionTests)
    endif()
    add_executable(RuntimeSessionTests tests/RuntimeSessionTests.cpp)
    target_link_libraries(RuntimeSessionTests PRIVATE LancelotEngine LancelotProjectScripts)
    add_test(NAME RuntimeSessionTests COMMAND RuntimeSessionTests)
    add_executable(AssetsRuntimeTests tests/AssetsRuntimeTests.cpp)
    target_link_libraries(AssetsRuntimeTests PRIVATE LancelotEngine)
    add_test(NAME AssetsRuntimeTests COMMAND AssetsRuntimeTests "${CMAKE_SOURCE_DIR}")
    add_executable(ProjectTests tests/ProjectTests.cpp)
    target_link_libraries(ProjectTests PRIVATE LancelotEngine)
    add_test(NAME ProjectTests COMMAND ProjectTests "${CMAKE_SOURCE_DIR}/projects/Sandbox/Sandbox.lancelot")
    add_executable(EngineBoundaryTests tests/EngineBoundaryTests.cpp)
    target_link_libraries(EngineBoundaryTests PRIVATE LancelotEngine)
    add_test(NAME EngineBoundaryTests COMMAND EngineBoundaryTests)
endif()
