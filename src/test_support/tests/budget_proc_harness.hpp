// Process harness shared by the T10c F3 and F4 budget regressions.
// Moved unchanged from budget_f3_test.cpp so F4 can reuse the helpers
// without editing an F3 case. Each including translation unit gets its
// own copy (unnamed namespace).
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "child_process.hpp"

namespace {

using anno::test::child::Child;
using anno::test::child::set_env;
using anno::test::child::sibling;
using namespace std::chrono_literals;

std::string hex16(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

using Vars = std::vector<std::pair<std::string, std::string>>;

/// Sets environment variables for the spawn of one child, then restores the
/// test process's own values. A child takes its environment at spawn time.
class Env {
  public:
    explicit Env(Vars vars) : vars_(std::move(vars)) {
        for (const auto& [name, value] : vars_) {
            const char* old = std::getenv(name.c_str());
            saved_.push_back(old != nullptr ? std::optional<std::string>(old) : std::nullopt);
            set_env(name.c_str(), value.c_str());
        }
    }
    ~Env() {
        for (std::size_t i = 0; i < vars_.size(); ++i) {
            set_env(vars_[i].first.c_str(), saved_[i] ? saved_[i]->c_str() : "");
        }
    }
    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;

  private:
    Vars vars_;
    std::vector<std::optional<std::string>> saved_;
};

/// The environment of an F3 client: a coordinator on `port` (local), its own
/// budget name, fast rechecks, spawning off unless `spawn` is "1".
Vars client_vars(std::uint16_t port, const char* budget_id, const char* admit_seconds,
                 const char* spawn = "0") {
    return {{"ANNO_TEST_BUDGET_COORD", "local"}, {"ANNO_TEST_BUDGET_PORT", std::to_string(port)},
            {"ANNO_TEST_BUDGET_ID", budget_id},  {"ANNO_TEST_BUDGET_RECHECK_MS", "100"},
            {"ANNO_TEST_BUDGET_SPAWN", spawn},   {"ANNO_TEST_ADMIT_WAIT_S", admit_seconds}};
}

void remove_file(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
}

/// Starts this case's own local coordinator. Its first line ("LISTENING ...")
/// comes back in `listening`. A coordinator that rejects an option or cannot
/// bind exits, and the case fails here with that line.
Child start_coordinator(std::uint16_t port, const std::filesystem::path& journal,
                        std::optional<std::uint64_t> build, std::optional<std::uint64_t> compat,
                        std::string& listening) {
    const auto exe = sibling("anno_budget_coord");
    REQUIRE(std::filesystem::exists(exe));
    std::vector<std::string> args = {std::to_string(port), "local", "--journal", journal.string()};
    if (build) {
        args.emplace_back("--build-id");
        args.push_back(hex16(*build));
    }
    if (compat) {
        args.emplace_back("--compat-version");
        args.push_back(std::to_string(*compat));
    }
    Child coord = Child::start(exe, args);
    REQUIRE(static_cast<bool>(coord));
    listening = coord.line(15000ms);
    INFO("coordinator on port " << port << " first line: '" << listening << "'");
    REQUIRE(listening.rfind("LISTENING", 0) == 0);
    return coord;
}

/// Quits a probe and waits until it has exited, so that its listening socket is
/// closed before the next child binds the same port. `send` alone is
/// asynchronous: a coordinator started at once lost the race (BIND-FAILED under
/// ASan, and on the Mac, 2026-10-30).
inline void quit_and_wait(Child& child) {
    child.send("q\n");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (child.running() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

Child probe(const std::vector<std::string>& args) {
    const auto exe = sibling("anno_budget_probe");
    REQUIRE(std::filesystem::exists(exe));
    Child child = Child::start(exe, args);
    REQUIRE(static_cast<bool>(child));
    return child;
}

/// The first line of `child` that starts with `prefix`, or "" when none came
/// before the deadline. Lines read on the way are appended to `seen`.
std::string want(Child& child, const std::string& prefix, std::chrono::milliseconds limit,
                 std::string* seen = nullptr) {
    const auto end = std::chrono::steady_clock::now() + limit;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= end) {
            return {};
        }
        const std::string line =
            child.line(std::chrono::duration_cast<std::chrono::milliseconds>(end - now));
        if (line.empty()) {
            return {};
        }
        if (seen != nullptr) {
            *seen += line + " | ";
        }
        if (line.rfind(prefix, 0) == 0) {
            return line;
        }
    }
}

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool eventually(const std::function<bool()>& done, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end) {
            return false;
        }
        std::this_thread::sleep_for(20ms);
    }
    return true;
}

/// Runs a shell command in WSL and returns its output: through wsl.exe on a
/// Windows host, directly on a WSL Linux host.
std::string wsl_capture(const std::string& command) {
#if defined(_WIN32)
    const std::string full = "wsl.exe -d Ubuntu -- sh -c \"" + command + "\" 2>&1";
    FILE* pipe = _popen(full.c_str(), "r");
#else
    const std::string full = command + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
#endif
    std::string out;
    if (pipe == nullptr) {
        return out;
    }
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }
#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return out;
}

/// The first whitespace-free token of `text`, for numbers printed by a command.
std::string token(const std::string& text) {
    std::istringstream words(text);
    std::string word;
    words >> word;
    return word;
}

bool wsl_available() {
#if defined(_WIN32)
    return std::system("wsl.exe -d Ubuntu -- true >nul 2>&1") == 0;
#elif defined(__APPLE__)
    // Darwin has neither WSL nor /proc: the WSL-only cases (the PowerShell meter,
    // /proc descriptor listings) do not apply here, and say so when they skip.
    return false;
#else
    return std::system("true") == 0;
#endif
}

#if defined(_WIN32)
/// The WSL spelling of a path on this Windows host (used for the WSL binary).
std::string as_wsl_path(const std::filesystem::path& path) {
    const std::string text = path.generic_string();
    if (text.size() >= 2 && text[1] == ':') {
        return std::string("/mnt/") +
               static_cast<char>(std::tolower(static_cast<unsigned char>(text[0]))) +
               text.substr(2);
    }
    return text;
}
#endif

/// The journal's open holds as (bytes) values, one per "A" line.
std::vector<std::uint64_t> open_hold_bytes(const std::filesystem::path& journal) {
    std::vector<std::uint64_t> out;
    std::ifstream in(journal);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string kind;
        std::string budget;
        std::string hold;
        std::uint64_t bytes = 0;
        words >> kind >> budget >> hold >> bytes;
        if (kind == "A") {
            out.push_back(bytes);
        }
    }
    return out;
}

bool contains_value(const std::vector<std::uint64_t>& values, std::uint64_t wanted) {
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

} // namespace
