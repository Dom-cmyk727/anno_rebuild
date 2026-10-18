# Strict warnings for project targets (not for fetched dependencies).
#
#   anno_target_defaults(<target> [SANITIZE_NO_RECOVER])
#
# SANITIZE_NO_RECOVER (with ANNO_SANITIZE, GCC/Clang): a UBSan finding in this
# target aborts the process, so a test fails instead of printing a report
# while ctest stays green. Used by the simulation targets, whose gate is
# proven by src/sim/tests/ubsan_probe.cpp (docs/sim/state.md §14.6). The other
# suites keep the default (recovering) UBSan until a first Linux asan run has
# shown them clean; then the keyword goes onto their targets as well.
function(anno_target_defaults target)
  cmake_parse_arguments(ARG "SANITIZE_NO_RECOVER" "" "" ${ARGN})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /fp:strict)
    # std::getenv & co. are standard C++; MSVC's C4996 suggestion of the
    # _s variants does not apply to portable code.
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(ANNO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
      -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference
      -ffp-contract=off)
    if(ANNO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(ANNO_SANITIZE)
      set(san_compile -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer)
      set(san_link -fsanitize=address,undefined,float-cast-overflow)
      if(ARG_SANITIZE_NO_RECOVER)
        list(APPEND san_compile -fno-sanitize-recover=all)
        list(APPEND san_link -fno-sanitize-recover=all)
      endif()
      target_compile_options(${target} PRIVATE ${san_compile})
      target_link_options(${target} PRIVATE ${san_link})
    endif()
  endif()
endfunction()
