// Identity of a prepared test-budget coordinator (T10c F6).
// The CMake build and the g++ fallback pass the same literals. A binary is
// reusable only when it contains the marker and its manifest matches.
#pragma once

#ifndef ANNO_COORD_CONFIG_LITERAL
#define ANNO_COORD_CONFIG_LITERAL "none"
#endif
#ifndef ANNO_COORD_FLAGS_LITERAL
#define ANNO_COORD_FLAGS_LITERAL "-std=c++20 -O2 -pthread"
#endif
#ifndef ANNO_COORD_MARKER_LITERAL
#define ANNO_COORD_MARKER_LITERAL "anno1503-coord-id unset"
#endif

inline const char* coordinator_config_name() {
    return ANNO_COORD_CONFIG_LITERAL;
}

inline const char* coordinator_flags_text() {
    return ANNO_COORD_FLAGS_LITERAL;
}

inline const char* coordinator_marker() {
    return ANNO_COORD_MARKER_LITERAL;
}
