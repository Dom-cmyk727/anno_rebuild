# anno-sim-replay: drive and diagnose a simulation replay log (docs/sim/state.md
# §8.7, §15 S9). A thin front end over anno_sim's replay API.
add_executable(sim_replay main.cpp)
set_target_properties(sim_replay PROPERTIES OUTPUT_NAME anno-sim-replay)
target_link_libraries(sim_replay PRIVATE anno_sim)
anno_target_defaults(sim_replay)
