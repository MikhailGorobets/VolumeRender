option(JSON_Install "Generate installation target" OFF)

include_directories(PUBLIC "${PROJECT_DIRECTORY}/3rd-party/nlohmann/include")
add_subdirectory(${PROJECT_DIRECTORY}/3rd-party/nlohmann)
set_target_properties(nlohmann_json PROPERTIES FOLDER "3rd-party")