include_directories(PUBLIC "${PROJECT_DIRECTORY}/3rd-party/imgui/include")
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/imgui)
set_target_properties(imgui PROPERTIES FOLDER "3rd-party")