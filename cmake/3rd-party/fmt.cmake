include_directories(PUBLIC "${PROJECT_DIRECTORY}/3rd-party/fmt/include")
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/fmt)
set_target_properties(fmt PROPERTIES FOLDER "3rd-party")