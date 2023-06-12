cmake_minimum_required (VERSION 3.19)

project(VolumeRender CXX)

set(PROJECT_CONTENT            "${CMAKE_CURRENT_SOURCE_DIR}/content")
set(PROJECT_DIRECTORY          "${CMAKE_CURRENT_SOURCE_DIR}")
set(PROJECT_OUTPUT_DIRECTORY   "${PROJECT_DIRECTORY}/x64")
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_DIRECTORY}/cmake")

include(cmake_settings)
include(compiler_settings)

include(3rd-party/fmt)
include(3rd-party/glfw)
include(3rd-party/imgui)
include(3rd-party/implot)
include(3rd-party/directx-tex)
include(3rd-party/nlohmann)

set(INCLUDE 
    include/Application.h
    include/ApplicationVolumeRender.h
    include/Common.h
    include/TransferFunction.h
)

set(SOURCE
    source/Application.cpp
    source/ApplicationVolumeRender.cpp
    source/Main.cpp
)

file(GLOB SHADERS "content/Shaders/*.hlsl")

source_group("include" FILES ${INCLUDE})
source_group("source" FILES  ${SOURCE})
source_group("shaders" FILES  ${SHADERS})

add_executable(VolumeRender ${INCLUDE} ${SOURCE} ${SHADERS})

target_link_libraries(VolumeRender PRIVATE fmt glfw imgui implot nlohmann_json directx-tex d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib)
target_include_directories(VolumeRender PRIVATE "include")

set_target_properties(VolumeRender PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIRECTORY}")
set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
