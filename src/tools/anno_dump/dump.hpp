// Shared helpers of the anno-dump commands. Every command appends its text
// to `out` and returns false when the file does not parse (exit code 1), as
// the Python references in tools/ do.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/tex.hpp>

namespace anno::dump {

using formats::Bytes;

/// FNV-1a 64-bit.
std::uint64_t fnv64(Bytes bytes);
/// As 16 lower-case hex digits.
std::string hex64(std::uint64_t v);
/// Bytes as lower-case hex.
std::string hex(Bytes bytes);
/// Bytes up to the first NUL, quoted: printable ASCII except `"` and `\` as
/// is, everything else as \xHH.
std::string quote(Bytes raw);
/// As quote, over all bytes (no NUL cut).
std::string quote_all(Bytes raw);
/// FNV-1a 64 over the 256 entries as R, G, B, as 16 hex digits.
std::string palette_hash(const formats::tex::Palette& p);
/// The `--full` palette lines `  P<row> <16 × rrggbb>`.
void palette_rows(const formats::tex::Palette& p, std::string& out);

bool dump_chunks(Bytes buf, std::string& out);
bool dump_tex(Bytes buf, bool full, std::string& out);
bool dump_inc(Bytes buf, std::string& out);
bool dump_colors(Bytes buf, bool full, std::string& out);
bool dump_figuren(Bytes buf, bool full, std::string& out);
bool dump_anlagen(Bytes buf, bool full, std::string& out);
bool dump_insel(Bytes buf, bool full, std::string& out);
bool dump_sze(Bytes buf, bool full, std::string& out);
bool dump_keyed(Bytes buf, std::string& out);
bool dump_texte(Bytes buf, bool full, std::string& out);
bool dump_zei(Bytes buf, bool full, std::string& out);
bool dump_bigbrother(Bytes buf, bool full, std::string& out);
bool dump_walk_tables(Bytes buf, bool full, std::string& out);
bool dump_fixed_map(Bytes buf, bool full, std::string& out);
/// `frames <game_dir> <entry id>...`; returns the exit code.
int dump_frames(const std::string& dir, const std::vector<std::string>& ids);

} // namespace anno::dump
