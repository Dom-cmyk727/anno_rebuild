// Checked publication of a requested evidence file (MAP-BUILD-1 package A).
//
// Opt-in tests export measurement tables and replay logs to a path the operator names. A request
// that cannot be honoured (target is a directory, open fails, a write or the final flush/close
// fails) must fail the calling case and must not print its success message. The file is written
// to "<target>.partial" and renamed over the target only after every operation succeeded, so an
// old or partial file is never left behind looking like the current run (an existing target is
// removed before the attempt). Both functions are [[nodiscard]]: a caller that drops the result or
// the REQUIRE on it fails to compile under -Werror. Close() failure cannot be injected into a real
// ofstream; that branch is checked by reading only.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

namespace anno::sim::test {

/// Writes `bytes` to `os` and flushes it. Returns an empty string, or "<operation> failed" naming
/// the first operation that failed. `what` is the path or label used in the message.
[[nodiscard]] inline std::string write_stream_checked(std::ostream& os, std::string_view bytes,
                                                      const std::string& what) {
    os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!os) {
        return what + ": write failed";
    }
    os.flush();
    if (!os) {
        return what + ": flush failed";
    }
    return {};
}

/// Publishes `bytes` at `target`. Returns an empty string on success, else a message naming the
/// failing path and operation. On failure `target` is absent (an earlier file is removed first),
/// never partial.
[[nodiscard]] inline std::string write_evidence_file(const std::filesystem::path& target,
                                                     std::string_view bytes) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(target, ec)) {
        return target.string() + ": destination is a directory";
    }
    // An earlier run's file must not survive a failed publication looking current.
    if (fs::is_regular_file(target, ec)) {
        fs::remove(target, ec);
    }
    const fs::path partial = fs::path(target.string() + ".partial");
    {
        std::ofstream f(partial, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) {
            return partial.string() + ": open failed";
        }
        std::string err = write_stream_checked(f, bytes, partial.string());
        if (err.empty()) {
            f.close();
            if (f.fail()) {
                err = partial.string() + ": close failed";
            }
        }
        if (!err.empty()) {
            f.close();
            fs::remove(partial, ec);
            return err;
        }
    }
    fs::rename(partial, target, ec);
    if (ec) {
        fs::remove(partial, ec);
        return target.string() + ": rename failed";
    }
    return {};
}

} // namespace anno::sim::test
