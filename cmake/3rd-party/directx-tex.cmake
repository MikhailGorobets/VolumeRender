include_directories(PUBLIC "${PROJECT_DIRECTORY}/3rd-party/directx-tex/include")
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/directx-tex)
set_target_properties(directx-tex PROPERTIES FOLDER "3rd-party")