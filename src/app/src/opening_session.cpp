#include <cstdio>
#include <fstream>
#include <system_error>

#include <anno/app/cli.hpp>
#include <anno/app/opening_session.hpp>

namespace anno::app {

OpeningSaver::OpeningSaver(PlayRunner& runner, OpeningIdentity identity, std::filesystem::path dir)
    : runner_(runner), identity_(std::move(identity)), dir_(std::move(dir)) {
    target_ = dir_ / ("quicksave-seat" + std::to_string(identity_.seat) + ".sav");
    worker_ = std::thread([this] { work(); });
}

OpeningSaver::~OpeningSaver() {
    {
        const std::lock_guard lock(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::string OpeningSaver::request() {
    {
        const std::lock_guard lock(mu_);
        if (pending_) {
            return "a save is already pending";
        }
        pending_ = true;
    }
    const bool asked = runner_.request_capture([this](WorldCapture c) {
        {
            const std::lock_guard lock(mu_);
            jobs_.push_back(std::move(c));
        }
        cv_.notify_one();
    });
    if (!asked) {
        const std::lock_guard lock(mu_);
        pending_ = false;
        return "a capture is already pending";
    }
    return "saving at the next turn (result follows)";
}

std::string OpeningSaver::status() const {
    const std::lock_guard lock(mu_);
    return status_;
}

std::string OpeningSaver::write_file(const std::filesystem::path& path,
                                     const std::vector<std::uint8_t>& bytes) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return "cannot create " + to_utf8(path.parent_path()) + ": " + ec.message();
        }
    }
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return "cannot write " + to_utf8(tmp);
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            std::filesystem::remove(tmp, ec);
            return "write to " + to_utf8(tmp) + " failed (disk full?)";
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code ignore;
        std::filesystem::remove(tmp, ignore);
        return "cannot replace " + to_utf8(path) + ": " + ec.message();
    }
    return {};
}

void OpeningSaver::work() {
    for (;;) {
        WorldCapture capture;
        {
            std::unique_lock lock(mu_);
            cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (jobs_.empty()) {
                return; // stopping, nothing left to write
            }
            capture = std::move(jobs_.front());
            jobs_.pop_front();
        }
        OpeningSave save;
        save.identity = identity_;
        save.turn = capture.turn;
        save.tick = capture.tick;
        save.state_hash = capture.hash;
        save.world = std::move(capture.save);
        const std::string error = write_file(target_, format_opening_save(save));
        char hash[32];
        std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(capture.hash));
        std::string line = error.empty() ? "saved " + to_utf8(target_) + " (turn " +
                                               std::to_string(save.turn) + ", tick " +
                                               std::to_string(save.tick) + ", hash " + hash + ")"
                                         : "SAVE FAILED: " + error + " (the session continues)";
        const std::lock_guard lock(mu_);
        status_ = std::move(line);
        pending_ = false;
    }
}

} // namespace anno::app
