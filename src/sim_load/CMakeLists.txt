# anno_sim_load: the file I/O around a scenario load that the simulation does not do
# (docs/sim/state.md §10, S14 stage 2): the island templates of a game directory for
# the INSELLNK resolver, read and handed to anno_sim as bytes.
add_library(anno_sim_load src/wildlife_prefix.cpp src/wildlife_graphics.cpp src/wildlife_postload_fill.cpp src/wildlife_first_call.cpp src/wildlife_create.cpp src/wildlife_create_batch.cpp src/wildlife_release.cpp src/wildlife_species.cpp src/wildlife_chunk.cpp src/wildlife_materialize.cpp src/wildlife_final_facts.cpp src/wildlife_habitat_load.cpp src/wildlife_import_plan.cpp src/wildlife_geometry.cpp src/wildlife_ground_prepare.cpp src/wildlife_ground_facts.cpp src/wildlife_growth_load.cpp src/scenario_load.cpp src/original_registry.cpp src/fixed_map.cpp
            src/ship_start.cpp src/script_base.cpp)
target_include_directories(anno_sim_load PUBLIC include)
target_link_libraries(anno_sim_load PUBLIC anno_sim)
target_compile_features(anno_sim_load PUBLIC cxx_std_20)
anno_target_defaults(anno_sim_load)
