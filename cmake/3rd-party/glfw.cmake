option(GLFW_INSTALL        "Generate installation target"    OFF)
option(GLFW_BUILD_DOCS     "Build the GLFW documentation"    OFF)
option(GLFW_BUILD_TESTS    "Build the GLFW test programs"    OFF)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)

include_directories(PUBLIC "${PROJECT_DIRECTORY}/3rd-party/glfw/include")
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/glfw)
set_target_properties(glfw PROPERTIES FOLDER "3rd-party")
set_target_properties(update_mappings PROPERTIES FOLDER "3rd-party")