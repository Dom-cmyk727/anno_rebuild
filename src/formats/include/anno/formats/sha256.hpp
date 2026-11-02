// SHA-256 (FIPS 180-4). Used to check the content hash of our own manifests
// (docs/sim/ship-start-integration.md §1). Not a world checksum.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <anno/formats/bytes.hpp>

namespace anno::formats {

using Sha256Digest = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256Digest sha256(Bytes data);

/// Lower-case hexadecimal, 64 characters (Python's `hexdigest()`).
[[nodiscard]] std::string sha256_hex(Bytes data);

} // namespace anno::formats
