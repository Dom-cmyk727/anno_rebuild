# Strict warnings for project targets (not for fetched dependencies).
function(anno_target_defaults target)
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
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined,float-cast-overflow)
    endif()
  endif()
endfunction()
