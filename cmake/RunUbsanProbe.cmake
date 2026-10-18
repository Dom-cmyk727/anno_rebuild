# Runs the sanitizer gate's probe (src/sim/tests/ubsan_probe.cpp) and checks
# that UBSan, and nothing else, stopped it:
#   cmake -DPROBE=<path to anno_sim_ubsan_probe> -P RunUbsanProbe.cmake
# Passes only when the probe printed its start marker (it ran), exited with a
# non-zero status (the process was stopped), and printed UBSan's report of the
# signed overflow. Exit status 0 means the sanitizer recovered: the gate is
# weakened. A crash, a link error or a missing runtime fail too, because the
# report is missing.
if(NOT PROBE)
  message(FATAL_ERROR "RunUbsanProbe.cmake: pass -DPROBE=<probe executable>")
endif()
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")
execute_process(COMMAND "${PROBE}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(all "${out}${err}")
if(NOT all MATCHES "ubsan_probe: start")
  message(FATAL_ERROR "the probe did not start (exit: ${rc}):\n${all}")
endif()
if(rc STREQUAL "0")
  message(FATAL_ERROR "the probe exited 0: UBSan did not stop the process "
                      "(is -fno-sanitize-recover=all missing?):\n${all}")
endif()
if(NOT all MATCHES "runtime error: signed integer overflow")
  message(FATAL_ERROR "the probe stopped (exit: ${rc}) but without UBSan's signed overflow "
                      "report, so something else failed:\n${all}")
endif()
message(STATUS "ubsan probe stopped by UBSan as intended (exit: ${rc})")
