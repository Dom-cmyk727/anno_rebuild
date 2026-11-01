// A recorded developer-tool session and its headless replay (see the header).
#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>

#include <anno/app/dev_session.hpp>
#include <anno/sim/reject.hpp>

namespace anno::app {

namespace {

/// The outcome lines without their origin and number ("turn ..."), in order; nullopt when the
/// text says outcomes were lost.
std::optional<std::vector<std::string>> outcome_bodies(std::string_view text) {
    std::vector<std::string> out;
    std::istringstream in{std::string(text)};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        if (line.rfind("lost ", 0) == 0) {
            return std::nullopt;
        }
        const auto at = line.find(" turn ");
        out.push_back(at == std::string::npos ? line : line.substr(at + 1));
    }
    return out;
}

} // namespace

std::string format_hashes(const std::vector<PlayRunner::TurnHash>& hashes) {
    std::string out;
    for (const auto& t : hashes) {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%llu %016llx\n", static_cast<unsigned long long>(t.turn),
                      static_cast<unsigned long long>(t.hash));
        out += buf;
    }
    return out;
}

formats::Result<std::vector<PlayRunner::TurnHash>, std::string>
parse_hashes(std::string_view text) {
    std::vector<PlayRunner::TurnHash> out;
    std::istringstream in{std::string(text)};
    std::string line;
    std::size_t number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (line.empty()) {
            continue;
        }
        std::istringstream words(line);
        PlayRunner::TurnHash t;
        std::string rest;
        if (!(words >> t.turn >> std::hex >> t.hash) || (words >> rest)) {
            return std::string("line " + std::to_string(number) + ": not '<turn> <hex>': " + line);
        }
        out.push_back(t);
    }
    if (out.empty()) {
        return std::string("no checkpoints");
    }
    return out;
}

std::string format_outcomes(const OutcomeRead& read) {
    std::ostringstream out;
    if (read.lost > 0) {
        out << "lost " << read.lost << " from " << read.lost_from << "\n";
    }
    for (const CommandOutcome& o : read.outcomes) {
        out << (o.id.origin == CommandOrigin::Script ? "script" : "window") << " #" << o.id.n
            << " turn " << o.turn << " " << outcome_state_name(o.state) << " "
            << sim::reject_name(o.status);
        for (const sim::Reject r : o.records) {
            out << " " << sim::reject_name(r);
        }
        out << "\n";
    }
    return out.str();
}

std::string format_invocation(const std::string& map, unsigned speed) {
    return "map " + map + "\nspeed " + std::to_string(speed) + "\n";
}

ReplayReport replay_session(sim::World world, unsigned speed, std::vector<ScriptedCommand> commands,
                            const std::vector<PlayRunner::TurnHash>& hashes,
                            std::optional<std::string_view> outcomes) {
    ReplayReport r;
    if (hashes.empty()) {
        r.message = "no checkpoints";
        return r;
    }
    std::optional<std::vector<std::string>> want_outcomes;
    if (outcomes) {
        want_outcomes = outcome_bodies(*outcomes);
        if (!want_outcomes) {
            r.message = "the session lost outcomes: they cannot be compared";
            return r;
        }
    }
    PlayRunner runner(std::move(world), speed);
    runner.set_script(std::move(commands));
    runner.set_record_hashes(true);
    std::uint64_t last = 0;
    for (const auto& t : hashes) {
        last = std::max(last, t.turn);
    }
    runner.run_n(static_cast<std::uint32_t>(last + 1));
    std::map<std::uint64_t, std::uint64_t> got;
    for (const auto& t : runner.recorded_hashes()) {
        got[t.turn] = t.hash;
    }
    for (const auto& t : hashes) {
        const auto it = got.find(t.turn);
        if (it == got.end()) {
            r.message = "turn " + std::to_string(t.turn) + " was not recorded in the replay";
            return r;
        }
        if (it->second != t.hash) {
            char buf[160];
            std::snprintf(buf, sizeof buf,
                          "turn %llu differs: recorded %016llx, replayed %016llx (%zu earlier "
                          "checkpoints agreed)",
                          static_cast<unsigned long long>(t.turn),
                          static_cast<unsigned long long>(t.hash),
                          static_cast<unsigned long long>(it->second), r.hashes);
            r.message = buf;
            return r;
        }
        ++r.hashes;
    }
    if (want_outcomes) {
        const OutcomeRead read = runner.outcomes_since(1);
        const auto replayed = outcome_bodies(format_outcomes(read));
        if (!replayed) {
            r.message = "the replay lost outcomes";
            return r;
        }
        for (std::size_t i = 0; i < std::max(want_outcomes->size(), replayed->size()); ++i) {
            const std::string a = i < want_outcomes->size() ? (*want_outcomes)[i] : "(none)";
            const std::string b = i < replayed->size() ? (*replayed)[i] : "(none)";
            if (a != b) {
                r.message = "outcome " + std::to_string(i + 1) + " differs: recorded '" + a +
                            "', replayed '" + b + "' (" + std::to_string(r.outcomes) +
                            " earlier outcomes agreed)";
                return r;
            }
            ++r.outcomes;
        }
    }
    r.agree = true;
    r.message = std::to_string(r.hashes) + " checkpoints agree (turns 0 .. " +
                std::to_string(last) + ")" +
                (want_outcomes ? ", " + std::to_string(r.outcomes) + " outcomes agree"
                               : ", outcomes not compared");
    return r;
}

} // namespace anno::app
