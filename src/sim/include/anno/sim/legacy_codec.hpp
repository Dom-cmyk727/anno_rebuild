// The original's group codec of the Build record 0x64, as an optional import for
// replays of the original (spec: docs/sim/state.md §8.5, §13 D14, OQ-049; stage S10).
// The rebuild's own wire format is the plain widened list of command.hpp; nothing in
// the simulation or the net layer sends this form.
//
// Evidence: AnnoFrame.dll tNetClientNet slot +0x54 = 0x1005cae0 packs the 10-byte
// placement records into it, the turn dispatcher 0x1005ffd0 -> 0x1005c7f0 unpacks it
// (docs/re/tick-loop.md §4); the group layout (0x1005c8cf, 0x1005cc07) is the one of
// docs/sim/state.md §8.5 and D14.
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/command.hpp>

namespace anno::sim::legacy {

/// Decodes the payload of an original 0x64 record (without the u16 type, u16 length
/// header): `u32 flags, u8 island, u8 city`, then groups until the payload ends, each
/// an 8-byte head `{u16 type | n << 12, u16 x, u16 y, u8 rotation | rec7 << 2, u8
/// rec8}` followed by `n` (0..15) followers of 3 bytes `{u8 rotation, s8 dx, s8 dy}`.
/// A follower is placed at the HEAD's tile plus (dx, dy) (relative to the head, not
/// cumulative) and takes the head's type, rec7 and rec8 (the original merges records
/// of equal type and rec8 into one group, so rec7 is lossy, D14). The placements come
/// out in stream order: the head, then its followers.
///
/// The flags word is read and dropped: the rebuild carries no build flags on the wire
/// (F13; the free-build flag 0x2000000 of the original becomes Rules::free_build_cheat).
/// A follower's rotation byte is kept whole (apply refuses a rotation above 3); the
/// head's is its low two bits. Errors (a message with the byte offset): a payload
/// shorter than its 6-byte head, a group head cut short, followers cut short. Total:
/// no input throws, over-reads or allocates more than one placement per 3 input bytes.
[[nodiscard]] formats::Result<BuildCmd, std::string>
decode_0x64(std::span<const std::uint8_t> payload);

} // namespace anno::sim::legacy
