// The opening session's quick-save (W4-OPENING-1; docs/app/ship-opening.md section 5): the sim
// thread captures the world at a turn boundary (PlayRunner::request_capture) and a writer thread of
// this class does the file I/O, so the window thread and the sim thread never touch the disk. The
// result (written, or why not) is a line the window shows; a failed write leaves the session and any
// earlier save intact (the file is written under a temporary name and renamed over the target).
#pragma once

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include <anno/app/opening.hpp>
#include <anno/app/play.hpp>

namespace anno::app {

class OpeningSaver {
  public:
    /// `runner` outlives the saver. Saves go to `dir`/quicksave-seat<N>.sav.
    OpeningSaver(PlayRunner& runner, OpeningIdentity identity, std::filesystem::path dir);
    /// Finishes the write in progress and stops the writer.
    ~OpeningSaver();
    OpeningSaver(const OpeningSaver&) = delete;
    OpeningSaver& operator=(const OpeningSaver&) = delete;

    /// Window thread: asks for a save at the next turn boundary. The text says what happens.
    std::string request();
    /// The newest result line ("" before the first save finished); any thread.
    [[nodiscard]] std::string status() const;
    /// The file a save goes to.
    [[nodiscard]] std::filesystem::path target() const { return target_; }

    /// Writes `save` to `path` through a temporary file; the error text or empty. (Public for the
    /// tests: a write that fails must say so and leave the old file.)
    [[nodiscard]] static std::string write_file(const std::filesystem::path& path,
                                                const std::vector<std::uint8_t>& bytes);

  private:
    void work();

    PlayRunner& runner_;
    OpeningIdentity identity_;
    std::filesystem::path dir_, target_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<WorldCapture> jobs_;
    std::string status_;
    bool stop_ = false, pending_ = false;
    std::thread worker_;
};

} // namespace anno::app
