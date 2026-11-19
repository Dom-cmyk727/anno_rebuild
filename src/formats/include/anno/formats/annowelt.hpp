// Walk tables in AnnoWelt.dll. Spec: docs/formats/annowelt-walk-tables.md;
// reference: tools/annowelt_tables.py dump.
//
// The original's table bytes are never stored in source or fixtures. This
// reader maps RVAs through the PE section table and copies the three tables
// field by field. Only the GOG 2.0.0.5 (TimeDateStamp, SizeOfImage) pair is
// accepted; a modded DLL can be accepted later behind an explicit option
// that also names the RVAs and sizes (PLAN §0.1).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::annowelt {

/// Original mapper length (D AnnoWelt 0x1009005f; 512 × u16 at RVA 0xc1af8).
inline constexpr std::size_t kMapperCount = 512;
/// Original walking-layer count (D 0x1000dd20; L = 0..4).
inline constexpr std::size_t kLayerCount = 5;
/// Bytes per layer block (D 0x1000dd20; 0x1c00).
inline constexpr std::size_t kLayerSize = 0x1c00;
/// Admission table length at the start of a layer block (D 0x10006ab0).
inline constexpr std::size_t kAdmissionCount = 0x400;
/// Cost-row stride (D 0x10006ab9; byte 0x400 + 8·code + j).
inline constexpr std::size_t kCostStride = 8;
/// Goal-order rows (D 0x100036fb..0x1000371c; 5 arrivals).
inline constexpr std::size_t kGoalOrderRows = 5;
/// Original goal-order row width (nine 4-byte entries). A later option can
/// store variable-length rows (PLAN §0.1).
inline constexpr std::size_t kGoalOrderWidth = 9;

inline constexpr std::uint32_t kRvaMapper = 0xc1af8;
inline constexpr std::uint32_t kRvaLayers = 0xd1140;
inline constexpr std::uint32_t kRvaGoalOrder = 0xc1160;

/// GOG 2.0.0.5 AnnoWelt.dll COFF TimeDateStamp (P: game/app/AnnoWelt.dll).
inline constexpr std::uint32_t kGogTimeDateStamp = 0x44c0c353;
/// GOG 2.0.0.5 SizeOfImage (P: the same file).
inline constexpr std::uint32_t kGogSizeOfImage = 0x000f1000;

enum class WalkTablesError { NotPe, WrongBuild, RvaOutsideSection, ShortFile };

[[nodiscard]] const char* error_message(WalkTablesError e);

struct GoalEntry {
    std::uint16_t mask = 0;
    std::uint8_t col = 0;
    std::uint8_t row = 0;
};

struct WalkTables {
    std::uint32_t time_date_stamp = 0;
    std::uint32_t size_of_image = 0;
    std::vector<std::uint16_t> mapper;
    std::vector<std::vector<std::uint8_t>> layers;
    std::vector<std::array<GoalEntry, kGoalOrderWidth>> goal_order;
};

Result<WalkTables, WalkTablesError> read_walk_tables(Bytes dll);

// Welt 0x10084670–0x10084698: selected 5x5 kernel; seven evidenced selectors,
// NOT the table's total extent or an engine-wide climate limit.
inline constexpr std::uint32_t kRvaGroundKernel = 0xca008;
inline constexpr std::uint32_t kGroundKernelSelectors = 7;
inline constexpr std::size_t kGroundKernelBytes = 25;
struct GroundKernel {
    std::uint32_t selector = 0, rva = 0;
    std::uint64_t raw_offset = 0;
    std::uint32_t time_date_stamp = 0, size_of_image = 0, image_base = 0;
    std::array<std::uint8_t, kGroundKernelBytes> weights{};
    std::string source_sha256;
};
// UNWIRED supplied-image data read, no loading/relocation/execution. Matching
// metadata establishes layout only; SHA identifies bytes, not authenticity or
// original city/INFO provenance. Validates a unique readable/non-writable fully
// file-backed window, then owns all returned data; input need not outlive call.
Result<GroundKernel, std::string> read_ground_kernel(Bytes dll, std::uint32_t selector);

} // namespace anno::formats::annowelt
