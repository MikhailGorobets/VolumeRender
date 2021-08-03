set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

if(WIN32)
   set(PLATFORM_WIN32 TRUE CACHE INTERNAL "Target platform: Win32")
   message("Target platform: Win32. SDK Version: " ${CMAKE_SYSTEM_VERSION})
endif()

if(PLATFORM_WIN32)
    set(GLOBAL_COMPILE_DEFINITIONS GLFW_EXPOSE_NATIVE_WIN32=1 NOMINMAX)
else()
    message(FATAL_ERROR "Unknown platform")
endif()

add_compile_definitions(${GLOBAL_COMPILE_DEFINITIONS})
