include_directories(${PROJECT_DIRECTORY}/3rd-party/implot/include)
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/implot)
set_target_properties(implot PROPERTIES FOLDER "3rd-party")