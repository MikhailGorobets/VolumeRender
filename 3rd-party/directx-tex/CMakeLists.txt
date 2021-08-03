cmake_minimum_required (VERSION 3.19)

project(directx-tex CXX)

set(INCLUDE
    include/directx-tex/DDSTextureLoader.h
    include/directx-tex/WICTextureLoader.h
)

set(SOURCE
    source/DDSTextureLoader.cpp
    source/WICTextureLoader.cpp
)

source_group("include" FILES ${INCLUDE})
source_group("source"  FILES ${SOURCE})

add_library(directx-tex STATIC ${INCLUDE} ${SOURCE})