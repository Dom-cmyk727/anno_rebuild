# river_qa: gap and connectivity measurement of enhanced river water (see main.cpp).
add_executable(river_qa main.cpp)
target_link_libraries(river_qa PRIVATE anno_app_core)
anno_target_defaults(river_qa)
