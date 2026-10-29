# anno_sim_load: the file I/O around a scenario load that the simulation does not do
# (docs/sim/state.md §10, S14 stage 2): the island templates of a game directory for
# the INSELLNK resolver, read and handed to anno_sim as bytes.
add_library(anno_sim_load src/scenario_load.cpp src/original_registry.cpp src/fixed_map.cpp)
target_include_directories(anno_sim_load PUBLIC include)
target_link_libraries(anno_sim_load PUBLIC anno_sim)
target_compile_features(anno_sim_load PUBLIC cxx_std_20)
anno_target_defaults(anno_sim_load)
