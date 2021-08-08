cmake_minimum_required (VERSION 3.19)

project(implot CXX)


set(INCLUDE
    include/implot/implot.h
    include/implot/implot_internal.h
)
   
set(SOURCE
    source/implot.cpp
    source/implot_demo.cpp
    source/implot_items.cpp
)

source_group("include" FILES ${INCLUDE})
source_group("source"  FILES ${SOURCE})

add_library(implot ${INCLUDE} ${SOURCE})