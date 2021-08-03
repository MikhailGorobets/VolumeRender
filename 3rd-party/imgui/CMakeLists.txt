cmake_minimum_required (VERSION 3.19)

project(imgui CXX)

set(INCLUDE
    include/imgui/imconfig.h
    include/imgui/imgui.h
    include/imgui/imgui_impl_dx11.h
    include/imgui/imgui_impl_glfw.h
    include/imgui/imgui_internal.h 
    include/imgui/imstb_rectpack.h
    include/imgui/imstb_textedit.h
    include/imgui/imstb_truetype.h
)

set(SOURCE
    source/imgui.cpp
    source/imgui_demo.cpp
    source/imgui_draw.cpp    
    source/imgui_impl_dx11.cpp
    source/imgui_impl_glfw.cpp
    source/imgui_tables.cpp 
    source/imgui_widgets.cpp
)

source_group("include" FILES ${INCLUDE})
source_group("source"  FILES ${SOURCE})

add_library(imgui STATIC ${INCLUDE} ${SOURCE})