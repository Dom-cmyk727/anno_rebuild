// A scripted developer-tool session in the live play window (see the header).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_script.hpp>
#include <anno/app/frame_projection.hpp>
#include <anno/render/picking.hpp>
#include <anno/sim/reject.hpp>

namespace anno::app {

namespace {

std::optional<std::int64_t> integer(const std::string& s) {
    char* end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return v;
}

std::optional<SDL_Keycode> keycode(const std::string& name) {
    if (name == "b") {
        return SDLK_B;
    }
    if (name == "n") {
        return SDLK_N;
    }
    if (name == "p") {
        return SDLK_P;
    }
    if (name == "t") {
        return SDLK_T;
    }
    if (name == "x") {
        return SDLK_X;
    }
    if (name == "esc") {
        return SDLK_ESCAPE;
    }
    if (name == "e") {
        return SDLK_E; // the water style
    }
    if (name == "space") {
        return SDLK_SPACE; // pause
    }
    if (name == "f") {
        return SDLK_F;
    }
    if (name == "u") {
        return SDLK_U;
    }
    if (name == "l") {
        return SDLK_L; // the developer Load adapter, W4-EXPAND-LIVE-1 D1
    }
    if (name == "h") {
        return SDLK_H;
    }
    if (name == "tab") {
        return SDLK_TAB;
    }
    if (name == "enter") {
        return SDLK_RETURN;
    }
    if (name == "lbracket") {
        return SDLK_LEFTBRACKET;
    }
    if (name == "rbracket") {
        return SDLK_RIGHTBRACKET;
    }
    if (name == "minus") {
        return SDLK_MINUS;
    }
    if (name == "plus") {
        return SDLK_PLUS;
    }
    if (name == "pageup") {
        return SDLK_PAGEUP;
    }
    if (name == "pagedown") {
        return SDLK_PAGEDOWN;
    }
    if (name == "f5") {
        return SDLK_F5;
    }
    return std::nullopt;
}

/// Whether the fact `key` of `st` compares `op` to `want`; `have` is the fact's text ("<none>" when
/// the window has no such fact, which compares unequal to everything but `!=`).
bool fact_holds(const DevScriptState& st, const std::string& key, const std::string& op,
                const std::string& want, std::string& have) {
    const auto it = st.facts.find(key);
    const bool present = it != st.facts.end();
    have = present ? it->second : "<none>";
    if (!present) {
        return op == "!=";
    }
    const auto number = [](const std::string& s) -> std::optional<long long> {
        char* end = nullptr;
        const long long v = std::strtoll(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0') {
            return std::nullopt;
        }
        return v;
    };
    const auto a = number(have), b = number(want);
    if (a && b) {
        if (op == "==") {
            return *a == *b;
        }
        if (op == "!=") {
            return *a != *b;
        }
        if (op == ">=") {
            return *a >= *b;
        }
        if (op == "<=") {
            return *a <= *b;
        }
        if (op == ">") {
            return *a > *b;
        }
        if (op == "<") {
            return *a < *b;
        }
        return false;
    }
    if (op == "==") {
        return have == want;
    }
    if (op == "!=") {
        return have != want;
    }
    if (op == "contains") {
        return have.find(want) != std::string::npos;
    }
    return false;
}

void push_key(std::vector<SDL_Event>& out, SDL_Keycode k) {
    SDL_Event e{};
    e.key.which = kScriptDevice;
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.down = true;
    out.push_back(e);
    e.type = SDL_EVENT_KEY_UP;
    e.key.down = false;
    out.push_back(e);
}

void push_motion(std::vector<SDL_Event>& out, std::pair<float, float> at,
                 SDL_MouseButtonFlags held) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.which = kScriptDevice;
    e.motion.x = at.first;
    e.motion.y = at.second;
    e.motion.state = held;
    out.push_back(e);
}

void push_button(std::vector<SDL_Event>& out, std::pair<float, float> at, std::uint8_t button,
                 bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.which = kScriptDevice;
    e.button.button = button;
    e.button.down = down;
    e.button.x = at.first;
    e.button.y = at.second;
    out.push_back(e);
}

/// A count of the occupancy index by name; none for an unknown name.
std::optional<std::int64_t> work_count(const PlacementKnowledge& k, const std::string& name) {
    if (name == "rebuilds") {
        return static_cast<std::int64_t>(k.rebuilds());
    }
    if (name == "deltas") {
        return static_cast<std::int64_t>(k.deltas_applied());
    }
    if (name == "tiles") {
        return static_cast<std::int64_t>(k.tiles_enumerated());
    }
    if (name == "foreign") {
        return static_cast<std::int64_t>(k.foreign_deltas());
    }
    if (name == "off_batches") {
        return static_cast<std::int64_t>(k.batches_skipped_disabled());
    }
    return std::nullopt;
}

/// The cheapest of the tool's types that costs gold and is not a road (the sim does
/// not build roads yet: Unsupported); none when there is none.
std::optional<std::int64_t> paid_type_id(const DevTool& tool) {
    std::optional<std::int64_t> best;
    std::uint64_t gold = 0;
    for (const sim::TypeId id : tool.setup().types) {
        const sim::BuildingType* type =
            tool.setup().registry ? tool.setup().registry->find(id) : nullptr;
        if (type != nullptr && type->cost_gold > 0 && (type->flags_word & sim::kRoadFlag) == 0 &&
            (!best || type->cost_gold < gold)) {
            best = static_cast<std::int64_t>(id.v);
            gold = type->cost_gold;
        }
    }
    return best;
}

std::string outcome_text(const std::optional<CommandOutcome>& o) {
    if (!o) {
        return "none";
    }
    std::string s = "#" + std::to_string(o->id.n) + " " + outcome_state_name(o->state) + " turn " +
                    std::to_string(o->turn);
    if (o->state == CommandOutcome::State::Refused) {
        s += " (" + std::string(sim::reject_name(o->status)) + ")";
    }
    return s;
}

/// The number of arguments each verb takes (a negative count: at least that many).
int arity(const std::string& verb) {
    if (verb == "look" || verb == "pan" || verb == "lookw" || verb == "sail" || verb == "rdown" ||
        verb == "rup" || verb == "click-land" || verb == "hover-land" || verb == "lpan") {
        return 2;
    }
    if (verb == "pick-ship" || verb == "lookship") {
        return 1;
    }
    if (verb == "focus-lost") {
        return 0;
    }
    if (verb == "key" || verb == "select" || verb == "rotate" || verb == "wheel" ||
        verb == "checkpoint" || verb == "screenshot" || verb == "hold" || verb == "release" ||
        verb == "foreign" || verb == "fail" || verb == "pass") {
        return 1;
    }
    if (verb == "note") {
        return -1;
    }
    if (verb == "click" || verb == "click_px") {
        return 2;
    }
    if (verb == "local") {
        return -2;
    }
    if (verb == "resize") {
        return 2;
    }
    if (verb == "drag") {
        return 4;
    }
    if (verb == "wait" || verb == "expect") {
        return -1;
    }
    if (verb == "quit") {
        return 0;
    }
    return -100;
}

/// FNV-1a, 64 bit, over `bytes`: the script identity's hash (an accidental-mismatch check, not a
/// security measure; tools/test/viewer_session.py computes the same value from the script file).
std::uint64_t fnv1a64(std::string_view bytes) {
    std::uint64_t h = 14695981039346656037ULL;
    for (const char c : bytes) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace

std::string DevScript::canonical_text(const std::vector<Step>& steps) {
    std::string text;
    for (const Step& s : steps) {
        text += std::to_string(s.line) + " " + s.verb;
        for (const std::string& a : s.args) {
            text += " " + a;
        }
        text += '\n';
    }
    return text;
}

std::string DevScript::identity_line(const std::vector<Step>& steps) {
    char hash[17];
    std::snprintf(hash, sizeof hash, "%016llx",
                  static_cast<unsigned long long>(fnv1a64(canonical_text(steps))));
    return std::string(kIdentityPrefix) + " steps=" + std::to_string(steps.size()) +
           " fnv1a64=" + hash;
}

bool foreign_input(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return e.key.which != kScriptDevice;
    case SDL_EVENT_MOUSE_MOTION:
        return e.motion.which != kScriptDevice;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return e.button.which != kScriptDevice;
    case SDL_EVENT_MOUSE_WHEEL:
        return e.wheel.which != kScriptDevice;
    default:
        return false;
    }
}

formats::Result<DevScript, std::string> DevScript::parse(std::string_view text) {
    DevScript script;
    std::istringstream in{std::string(text)};
    std::string line;
    std::size_t number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream words(line);
        Step s;
        s.line = number;
        if (!(words >> s.verb)) {
            continue;
        }
        std::string w;
        while (words >> w) {
            s.args.push_back(w);
        }
        if (s.verb == "expect" && s.args.size() >= 2 &&
            (s.args[0] == "title" || s.args[0] == "title-lacks")) {
            // The rest of the line is the text (spaces kept as one).
            std::string rest;
            for (std::size_t i = 1; i < s.args.size(); ++i) {
                rest += (i > 1 ? " " : "") + s.args[i];
            }
            s.args = {s.args[0], rest};
        }
        if (s.verb == "expect" && s.args.size() >= 4 && s.args[0] == "fact") {
            // Same pattern as title/title-lacks above: a fact's value can itself contain a space
            // (DescribePayload's status lines join several "key=value" pairs with one), and
            // `contains` without it can accidentally match a longer number sharing the same
            // prefix ("residents=5" also matching "residents=50" — W4-SETTLEMENT-1 W7 review,
            // finding 9). Join everything after the operator into one value, so a script can pin
            // the token boundary (`contains residents=5 stock=`) instead of just a substring.
            std::string rest;
            for (std::size_t i = 3; i < s.args.size(); ++i) {
                rest += (i > 3 ? " " : "") + s.args[i];
            }
            s.args = {s.args[0], s.args[1], s.args[2], rest};
        }
        const int n = arity(s.verb);
        const auto have = static_cast<int>(s.args.size());
        if (n == -100 || (n >= 0 && have != n) || (n < 0 && have < -n)) {
            return std::string("line " + std::to_string(number) + ": '" + line + "' is not a step");
        }
        script.steps_.push_back(std::move(s));
    }
    script.log_.push_back(identity_line(script.steps_));
    return script;
}

void DevScript::fail(const Step& s, const std::string& why) {
    failed_ = true;
    log_.push_back("FAIL line " + std::to_string(s.line) + " (" + s.verb + "): " + why);
}

std::optional<std::pair<double, double>>
DevScript::scene_point(const PointerFrame& frame, const DevTool& tool, sim::TileXY tile) {
    if (!frame.scene) {
        return std::nullopt;
    }
    const render::Scene& scene = *frame.scene;
    std::optional<std::size_t> index;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        if (scene.islands[k].x == tool.setup().island.x0 &&
            scene.islands[k].y == tool.setup().island.y0) {
            index = k;
        }
    }
    if (!index) {
        return std::nullopt;
    }
    const auto rounding = placement_rounding(scene, *index, frame.group, frame.geometry.tile_w);
    const double h =
        scene.ground_height(static_cast<float>(tile.x) + 0.5F, static_cast<float>(tile.y) + 0.5F)
            .value_or(0.0F);
    const auto [cx, cy] = render::corner_point(tile.x + 0.5, tile.y + 0.5, h, frame.geometry);
    return std::pair{cx + rounding.first, cy + rounding.second};
}

std::optional<std::pair<float, float>> DevScript::window_point(const PointerFrame& frame,
                                                               const DevTool& tool,
                                                               sim::TileXY tile, float density) {
    const auto at = scene_point(frame, tool, tile);
    if (!at || !(density > 0.0F)) {
        return std::nullopt;
    }
    const double px = (at->first - frame.map.x0) * frame.map.scale;
    const double py = (at->second - frame.map.y0) * frame.map.scale;
    if (!(px >= 0.0) || !(py >= 0.0) || !(px < frame.drawable_w) || !(py < frame.drawable_h)) {
        return std::nullopt;
    }
    return std::pair{static_cast<float>(px / density), static_cast<float>(py / density)};
}

DevScriptAction DevScript::step(const DevScriptState& st) {
    DevScriptAction act;
    while (!done()) {
        if (!act.events.empty() || act.look || act.look_world || act.screenshot ||
            act.hold != DevScriptAction::Hold::None || act.stage != DevScriptAction::Stage::None ||
            act.cpu_fail_set != 0 || act.cpu_fail_clear != 0 || act.resize ||
            act.foreign != DevScriptAction::Foreign::None ||
            act.local != DevScriptAction::Local::None) {
            // One input step per call: the window handles it before the next step (a wait or a
            // check included) looks at what it shows.
            break;
        }
        const Step& s = steps_[next_];
        const auto arg = [&s](std::size_t i) { return integer(s.args[i]); };
        auto tile_arg = [&](std::size_t i) -> std::optional<sim::TileXY> {
            const auto x = arg(i), y = arg(i + 1);
            if (!x || !y) {
                return std::nullopt;
            }
            return sim::TileXY{static_cast<std::int32_t>(*x), static_cast<std::int32_t>(*y)};
        };
        auto point_of = [&](sim::TileXY t) -> std::optional<std::pair<float, float>> {
            if (st.frame == nullptr || st.tool == nullptr) {
                return std::nullopt;
            }
            return window_point(*st.frame, *st.tool, t, st.density);
        };
        if (s.verb == "wait") {
            if (!wait_ms_) {
                wait_ms_ = st.now_ms;
                wait_frames_ = st.frames;
                wait_turn_ = st.facts_turn;
            }
            bool over = false;
            const std::string& what = s.args[0];
            if (what == "frames" && s.args.size() == 2 && arg(1)) {
                over = st.frames >= *wait_frames_ + static_cast<std::uint64_t>(*arg(1));
            } else if (what == "turns" && s.args.size() == 2 && arg(1)) {
                // Gated on facts_turn, not st.snapshot->turn: a fact-reading step right after this
                // wait (as `opening_controls.script` does, `key h` then `wait turns 3` then
                // `expect fact tracked.1.state == stopped`) must see the turn this wait reports as
                // reached, not a live turn the facts have not caught up to yet (the race this
                // field exists to close; see DevScriptState::facts_turn).
                over = st.facts_turn >= *wait_turn_ + static_cast<std::uint64_t>(*arg(1));
            } else if (what == "ms" && s.args.size() == 2 && arg(1)) {
                over = st.now_ms >= *wait_ms_ + static_cast<double>(*arg(1));
            } else if (what == "outcome") {
                over = st.latest && st.latest->state != CommandOutcome::State::Queued;
            } else if (what == "drawn") {
                over = st.latest && st.latest->state != CommandOutcome::State::Queued &&
                       (st.latest->state != CommandOutcome::State::Applied ||
                        !visibility_waiting(st.visibility));
            } else if (what == "visibility" && s.args.size() == 2) {
                over = st.latest && st.latest->state == CommandOutcome::State::Applied &&
                       s.args[1] == visibility_name(st.visibility);
            } else if (what == "fact" && (s.args.size() == 4 || s.args.size() == 6)) {
                std::string have;
                over = fact_holds(st, s.args[1], s.args[2], s.args[3], have);
            } else if (what == "blocked") {
                if (!st.stage_holds_available) {
                    fail(s, "no chunk gate in this session");
                    break;
                }
                over = st.chunks_blocked > 0;
            } else if (what == "held") {
                if (!st.hold_available || st.hold_expired) {
                    fail(s, st.hold_available ? "the hold's watchdog ended a wait"
                                              : "no publication hold in this session");
                    break;
                }
                over = st.held;
            } else {
                fail(s, "unknown wait");
                break;
            }
            if (!over) {
                // `wait fact K OP V within MS`: a longer limit for a voyage (the default 30 s is
                // for a stalled session; a ship that sails five hundred tiles needs minutes).
                double limit = DevScript::kWaitLimitMs;
                if (what == "fact" && s.args.size() == 6 && s.args[4] == "within" && arg(5) &&
                    *arg(5) > 0) {
                    limit = static_cast<double>(*arg(5));
                }
                if (what != "ms" && st.now_ms > *wait_ms_ + limit) {
                    std::string seen;
                    if (what == "fact") {
                        std::string have;
                        fact_holds(st, s.args[1], s.args[2], s.args[3], have);
                        seen = ", " + s.args[1] + " is '" + have + "'";
                    }
                    fail(s, "not over after " + std::to_string(static_cast<int>(limit)) +
                                " ms (outcome " + outcome_text(st.latest) + ", visibility " +
                                visibility_name(st.visibility) + seen + ")");
                }
                break;
            }
            wait_ms_.reset();
            ++next_;
            continue;
        }
        if (s.verb == "look") {
            const auto t = tile_arg(0);
            if (!t) {
                fail(s, "not a tile");
                break;
            }
            act.look = *t;
        } else if (s.verb == "key") {
            const auto k = keycode(s.args[0]);
            if (!k) {
                fail(s, "unknown key");
                break;
            }
            push_key(act.events, *k);
        } else if (s.verb == "select" || s.verb == "rotate") {
            auto want = arg(0);
            if (s.verb == "select" && s.args[0] == "paid" && st.tool != nullptr) {
                want = paid_type_id(*st.tool);
            }
            if (!want || st.tool == nullptr) {
                fail(s, "no number or no tool, or no paid type");
                break;
            }
            const bool there = s.verb == "select" ? st.tool->input().selected() &&
                                                        st.tool->input().selected()->v ==
                                                            static_cast<std::uint32_t>(*want)
                                                  : st.tool->input().rotation() == *want;
            if (!there) {
                const std::size_t limit = s.verb == "select" ? st.tool->setup().types.size() : 4;
                if (presses_ >= limit) {
                    fail(s, "not reached after " + std::to_string(presses_) + " presses");
                    break;
                }
                ++presses_;
                push_key(act.events, s.verb == "select" ? SDLK_N : SDLK_T);
                break; // looked at again after the window handled it
            }
            presses_ = 0;
        } else if (s.verb == "click" || s.verb == "drag") {
            const auto from = tile_arg(0);
            const auto to = s.verb == "drag" ? tile_arg(2) : from;
            const auto a = from ? point_of(*from) : std::nullopt;
            const auto b = to ? point_of(*to) : std::nullopt;
            if (!a || !b) {
                fail(s, "the tile is not drawn in the window");
                break;
            }
            push_motion(act.events, *a, 0);
            push_button(act.events, *a, SDL_BUTTON_LEFT, true);
            constexpr int kSteps = 8;
            for (int i = 1; i <= kSteps && s.verb == "drag"; ++i) {
                const float f = static_cast<float>(i) / kSteps;
                push_motion(
                    act.events,
                    {a->first + (b->first - a->first) * f, a->second + (b->second - a->second) * f},
                    SDL_BUTTON_LMASK);
            }
            push_button(act.events, *b, SDL_BUTTON_LEFT, false);
        } else if (s.verb == "click_px") {
            // UI.2 C4: a raw drawable-pixel click (construction_handle's own coordinate space,
            // data/ui/'s 1024 x 768 reference), not a world tile - `click`'s window_point
            // projection does not apply here.
            const auto x = arg(0), y = arg(1);
            if (!x || !y) {
                fail(s, "not a pixel position");
                break;
            }
            const std::pair<float, float> at{static_cast<float>(*x), static_cast<float>(*y)};
            push_motion(act.events, at, 0);
            push_button(act.events, at, SDL_BUTTON_LEFT, true);
            push_button(act.events, at, SDL_BUTTON_LEFT, false);
        } else if (s.verb == "lookw") {
            const auto t = tile_arg(0);
            if (!t) {
                fail(s, "not a tile");
                break;
            }
            act.look_world = std::pair{t->x + 0.5, t->y + 0.5};
        } else if (s.verb == "lookship") {
            const auto x = st.facts.find("ship." + s.args[0] + ".x");
            const auto y = st.facts.find("ship." + s.args[0] + ".y");
            if (x == st.facts.end() || y == st.facts.end()) {
                fail(s, "no such ship in the view");
                break;
            }
            act.look_world =
                std::pair{std::atof(x->second.c_str()) + 0.5, std::atof(y->second.c_str()) + 0.5};
        } else if (s.verb == "sail" || s.verb == "rdown" || s.verb == "rup" ||
                   s.verb == "click-land" || s.verb == "hover-land") {
            const auto t = tile_arg(0);
            std::optional<std::pair<double, double>> px;
            if (t && st.frame != nullptr) {
                px = (s.verb == "click-land" || s.verb == "hover-land")
                         ? ground_pixel(*st.frame, *t)
                         : sea_pixel(*st.frame, t->x + 0.5, t->y + 0.5);
            }
            if (!px || !(st.density > 0.0F)) {
                fail(s, "the tile is not drawn in the window");
                break;
            }
            const std::pair<float, float> at{static_cast<float>(px->first / st.density),
                                             static_cast<float>(px->second / st.density)};
            push_motion(act.events, at, 0);
            const bool land = s.verb == "click-land";
            const std::uint8_t button = land ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
            if (s.verb == "sail" || land || s.verb == "rdown") {
                push_button(act.events, at, button, true);
            }
            if (s.verb == "sail" || land || s.verb == "rup") {
                push_button(act.events, at, button, false);
            }
        } else if (s.verb == "pick-ship") {
            const auto id = arg(0);
            const auto at =
                id && st.ship_point ? st.ship_point(static_cast<std::uint64_t>(*id)) : std::nullopt;
            if (!at) {
                fail(s, "that ship is not drawn in the window");
                break;
            }
            push_motion(act.events, *at, 0);
            push_button(act.events, *at, SDL_BUTTON_LEFT, true);
            push_button(act.events, *at, SDL_BUTTON_LEFT, false);
        } else if (s.verb == "lpan") {
            const auto dx = arg(0), dy = arg(1);
            if (!dx || !dy) {
                fail(s, "not a distance");
                break;
            }
            const std::pair<float, float> c{static_cast<float>(st.window_w) / 2.0F,
                                            static_cast<float>(st.window_h) / 2.0F};
            const std::pair<float, float> e{c.first + static_cast<float>(*dx),
                                            c.second + static_cast<float>(*dy)};
            push_motion(act.events, c, 0);
            push_button(act.events, c, SDL_BUTTON_LEFT, true);
            push_motion(act.events, e, SDL_BUTTON_LMASK);
            push_button(act.events, e, SDL_BUTTON_LEFT, false);
        } else if (s.verb == "focus-lost") {
            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            act.events.push_back(e);
        } else if (s.verb == "pan") {
            const auto dx = arg(0), dy = arg(1);
            if (!dx || !dy) {
                fail(s, "not a distance");
                break;
            }
            const std::pair<float, float> c{static_cast<float>(st.window_w) / 2.0F,
                                            static_cast<float>(st.window_h) / 2.0F};
            push_motion(act.events, c, 0);
            push_button(act.events, c, SDL_BUTTON_RIGHT, true);
            push_motion(act.events,
                        {c.first + static_cast<float>(*dx), c.second + static_cast<float>(*dy)},
                        SDL_BUTTON_RMASK);
            push_button(act.events,
                        {c.first + static_cast<float>(*dx), c.second + static_cast<float>(*dy)},
                        SDL_BUTTON_RIGHT, false);
        } else if (s.verb == "wheel") {
            const auto n = arg(0);
            if (!n) {
                fail(s, "not a number");
                break;
            }
            SDL_Event e{};
            e.type = SDL_EVENT_MOUSE_WHEEL;
            e.wheel.which = kScriptDevice;
            e.wheel.y = static_cast<float>(*n);
            e.wheel.mouse_x = static_cast<float>(st.window_w) / 2.0F;
            e.wheel.mouse_y = static_cast<float>(st.window_h) / 2.0F;
            act.events.push_back(e);
        } else if (s.verb == "fail" || s.verb == "pass") {
            const std::string& n = s.args[0];
            const std::uint8_t bit = n == "update" ? 1 : n == "submit" ? 2 : n == "present" ? 4 : 0;
            if (bit == 0 || !st.cpu_fail_available) {
                fail(s, bit == 0
                            ? "only update, submit or present can fail"
                            : "no CPU delivery seam in this session (it needs the CPU window)");
                break;
            }
            (s.verb == "fail" ? act.cpu_fail_set : act.cpu_fail_clear) = bit;
        } else if (s.verb == "resize") {
            const auto rw = arg(0), rh = arg(1);
            if (!rw || !rh || *rw <= 0 || *rh <= 0) {
                fail(s, "not a size");
                break;
            }
            act.resize = std::pair{static_cast<int>(*rw), static_cast<int>(*rh)};
        } else if ((s.verb == "hold" || s.verb == "release") &&
                   (s.args[0] == "chunks" || s.args[0] == "uploads" || s.args[0] == "presents")) {
            if (!st.stage_holds_available) {
                fail(s, "no stage holds in this session (they need the GPU window)");
                break;
            }
            const bool arm = s.verb == "hold";
            using Stage = DevScriptAction::Stage;
            act.stage = s.args[0] == "chunks" ? (arm ? Stage::ArmChunks : Stage::ReleaseChunks)
                        : s.args[0] == "uploads"
                            ? (arm ? Stage::ArmUploads : Stage::ReleaseUploads)
                            : (arm ? Stage::FailPresents : Stage::PassPresents);
        } else if (s.verb == "hold" || s.verb == "release") {
            if (s.args[0] != "publication" || !st.hold_available) {
                fail(s, s.args[0] != "publication" ? "only 'publication' can be held"
                                                   : "no publication hold in this session");
                break;
            }
            act.hold =
                s.verb == "hold" ? DevScriptAction::Hold::Arm : DevScriptAction::Hold::Release;
        } else if (s.verb == "local") {
            const bool build = s.args[0] == "build";
            const std::size_t need = build ? 4 : 3;
            const auto x = s.args.size() == need ? arg(need - 2) : std::nullopt;
            const auto y = s.args.size() == need ? arg(need - 1) : std::nullopt;
            const auto type =
                build && s.args.size() == need ? arg(1) : std::optional<std::int64_t>{0};
            if ((!build && s.args[0] != "demolish") || !x || !y || !type || st.tool == nullptr ||
                !st.tool->setup().city) {
                fail(s, "'local build TYPE X Y' or 'local demolish X Y' for the tool's city");
                break;
            }
            act.local = build ? DevScriptAction::Local::Build : DevScriptAction::Local::Demolish;
            act.local_type = sim::TypeId{static_cast<std::uint32_t>(*type)};
            act.local_at =
                sim::TileXY{static_cast<std::int32_t>(*x), static_cast<std::int32_t>(*y)};
        } else if (s.verb == "foreign") {
            if ((s.args[0] != "build" && s.args[0] != "demolish") || st.tool == nullptr ||
                !st.tool->setup().foreign) {
                fail(s, "'foreign build|demolish' needs another player's city on another island");
                break;
            }
            act.foreign = s.args[0] == "build" ? DevScriptAction::Foreign::Build
                                               : DevScriptAction::Foreign::Demolish;
        } else if (s.verb == "note" && s.args[0] == "fact" && s.args.size() == 2) {
            const auto it = st.facts.find(s.args[1]);
            if (it == st.facts.end()) {
                fail(s, "no such fact: " + s.args[1]);
                break;
            }
            noted_facts_[s.args[1]] = it->second;
        } else if (s.verb == "note") {
            if (s.args[0] == "frames" && s.args.size() == 1) {
                noted_frames_ = st.frames;
            } else if (s.args[0] == "money" && s.args.size() == 1 && st.snapshot) {
                noted_money_ = st.snapshot->money;
            } else if (s.args[0] == "work" && s.args.size() == 2 && st.knowledge != nullptr &&
                       work_count(*st.knowledge, s.args[1])) {
                noted_work_[s.args[1]] = *work_count(*st.knowledge, s.args[1]);
            } else {
                fail(s, "'note money' needs a snapshot, 'note work NAME' the tool's index");
                break;
            }
        } else if (s.verb == "expect") {
            if (s.args[0] == "fact" && s.args.size() == 3 && s.args[2] == "same") {
                const auto noted = noted_facts_.find(s.args[1]);
                const auto now = st.facts.find(s.args[1]);
                if (noted == noted_facts_.end() || now == st.facts.end() ||
                    noted->second != now->second) {
                    fail(s, s.args[1] + " is '" +
                                (now == st.facts.end() ? std::string("<none>") : now->second) +
                                "', it was '" +
                                (noted == noted_facts_.end() ? std::string("<not noted>")
                                                             : noted->second) +
                                "'");
                    break;
                }
            } else if (s.args[0] == "fact" && s.args.size() == 4) {
                std::string have;
                if (!fact_holds(st, s.args[1], s.args[2], s.args[3], have)) {
                    fail(s,
                         s.args[1] + " is '" + have + "', expected " + s.args[2] + " " + s.args[3]);
                    break;
                }
            } else if (s.args[0] == "title" && s.args.size() == 2) {
                if (st.title.find(s.args[1]) == std::string::npos) {
                    fail(s, "the title is '" + st.title + "'");
                    break;
                }
            } else if (s.args[0] == "title-lacks" && s.args.size() == 2) {
                if (st.title.find(s.args[1]) != std::string::npos) {
                    fail(s, "the title is '" + st.title + "'");
                    break;
                }
            } else if (s.args[0] == "held" && s.args.size() == 2) {
                const std::string& n = s.args[1];
                const bool in_effect = n == "chunks"        ? st.chunks_armed
                                       : n == "uploads"     ? st.uploads_held
                                       : n == "presents"    ? st.presents_failing
                                       : n == "publication" ? st.held
                                                            : false;
                if (!in_effect) {
                    fail(s, "'" + n + "' is not held now");
                    break;
                }
            } else if (s.args[0] == "frames" && s.args.size() == 3 && s.args[1] == "at-most") {
                const auto limit = integer(s.args[2]);
                if (!noted_frames_ || !limit) {
                    fail(s, "'expect frames at-most N' needs 'note frames' first and a number");
                    break;
                }
                const auto drawn = static_cast<std::int64_t>(st.frames - *noted_frames_);
                if (drawn > *limit) {
                    fail(s, std::to_string(drawn) + " frames since the note, at most " +
                                std::to_string(*limit) + " wanted");
                    break;
                }
            } else if (s.args[0] == "failing" && s.args.size() == 2) {
                const std::string& n = s.args[1];
                const std::uint8_t bit = n == "update"    ? 1
                                         : n == "submit"  ? 2
                                         : n == "present" ? 4
                                                          : 0;
                if (bit == 0 || (st.cpu_failing & bit) == 0) {
                    fail(s, "'" + n + "' is not failing now");
                    break;
                }
            } else if (s.args[0] == "published" && s.args.size() == 1) {
                const bool shown =
                    st.latest && (st.latest->state == CommandOutcome::State::Applied ||
                                  st.latest->state == CommandOutcome::State::Refused);
                if (shown && (!st.snapshot || st.latest->seq >= st.snapshot->outcomes_next)) {
                    fail(s, "the window shows " + outcome_text(st.latest) + " (sequence " +
                                std::to_string(st.latest->seq) +
                                ") beside a snapshot that follows outcomes below " +
                                std::to_string(st.snapshot ? st.snapshot->outcomes_next : 0));
                    break;
                }
            } else if (s.args[0] == "money" && s.args.size() == 2) {
                if (!noted_money_ || !st.snapshot) {
                    fail(s, "no noted money or no snapshot");
                    break;
                }
                const std::int64_t now = st.snapshot->money;
                const bool ok = s.args[1] == "same"   ? now == *noted_money_
                                : s.args[1] == "less" ? now < *noted_money_
                                                      : false;
                if (!ok) {
                    fail(s, "money is " + std::to_string(now) + ", noted " +
                                std::to_string(*noted_money_));
                    break;
                }
            } else if (s.args[0] == "work" && (s.args.size() == 3 || s.args.size() == 4)) {
                if (st.knowledge == nullptr) {
                    fail(s, "no occupancy index");
                    break;
                }
                const std::string& name = s.args[1];
                const auto have = work_count(*st.knowledge, name);
                if (!have) {
                    fail(s, "unknown count '" + name + "'");
                    break;
                }
                bool ok = false;
                std::string wanted;
                if (s.args.size() == 3) {
                    const auto noted = noted_work_.find(name);
                    if (noted == noted_work_.end() ||
                        (s.args[2] != "same" && s.args[2] != "grew")) {
                        fail(s, "'expect work NAME same|grew' needs 'note work NAME' first");
                        break;
                    }
                    ok = s.args[2] == "same" ? *have == noted->second : *have > noted->second;
                    wanted = s.args[2] + " (noted " + std::to_string(noted->second) + ")";
                } else {
                    const auto want = integer(s.args[3]);
                    const std::string& op = s.args[2];
                    if (!want || (op != "==" && op != "<=" && op != ">=")) {
                        fail(s, "not a number or an unknown comparison");
                        break;
                    }
                    ok = op == "==" ? *have == *want : op == "<=" ? *have <= *want : *have >= *want;
                    wanted = op + " " + std::to_string(*want);
                }
                if (!ok) {
                    fail(s, name + " is " + std::to_string(*have) + ", wanted " + wanted);
                    break;
                }
            } else if (s.args[0] == "occupied" && s.args.size() == 4) {
                const auto x = integer(s.args[1]), y = integer(s.args[2]);
                if (st.knowledge == nullptr || !x || !y ||
                    (s.args[3] != "yes" && s.args[3] != "no")) {
                    fail(s, "no occupancy index, or not a tile and yes|no");
                    break;
                }
                const bool occ = st.knowledge->occupied(
                    sim::TileXY{static_cast<std::int32_t>(*x), static_cast<std::int32_t>(*y)});
                if (occ != (s.args[3] == "yes")) {
                    fail(s, std::string("tile ") + s.args[1] + "," + s.args[2] + " is " +
                                (occ ? "occupied" : "free") + " in the index");
                    break;
                }
            } else if (s.args[0] == "visibility" && s.args.size() == 2) {
                if (s.args[1] != visibility_name(st.visibility)) {
                    fail(s, std::string("the latest command's change is at ") +
                                visibility_name(st.visibility));
                    break;
                }
            } else if (s.args[0] == "outcome" && s.args.size() == 2 && s.args[1] == "none") {
                if (st.latest) {
                    fail(s, "the latest outcome is " + outcome_text(st.latest));
                    break;
                }
            } else if (s.args[0] == "outcome" && s.args.size() >= 2) {
                const bool state_ok =
                    st.latest && std::string(outcome_state_name(st.latest->state)) == s.args[1];
                const bool reason_ok =
                    s.args.size() < 3 ||
                    (st.latest && std::string(sim::reject_name(st.latest->status)) == s.args[2]);
                if (!state_ok || !reason_ok) {
                    fail(s, "the latest outcome is " + outcome_text(st.latest));
                    break;
                }
            } else {
                fail(s, "unknown expectation");
                break;
            }
        } else if (s.verb == "checkpoint") {
            log_.push_back(
                "checkpoint " + s.args[0] + ": turn " +
                std::to_string(st.snapshot ? st.snapshot->turn : 0) + ", frames " +
                std::to_string(st.frames) + ", outcome " + outcome_text(st.latest) +
                ", visibility " + visibility_name(st.visibility) + ", ms " +
                std::to_string(static_cast<long long>(st.now_ms)) + ", generation " +
                std::to_string(st.scene_generation) + ", surface " +
                std::to_string(st.surface_epoch) +
                (st.stage_holds_available
                     ? ", chunks_blocked " + std::to_string(st.chunks_blocked) +
                           (st.uploads_held ? ", uploads held" : "") +
                           (st.presents_failing ? ", presents failing" : "")
                     : std::string()) +
                (st.knowledge != nullptr
                     ? ", occupancy rebuilds " + std::to_string(st.knowledge->rebuilds()) +
                           " deltas " + std::to_string(st.knowledge->deltas_applied()) + " tiles " +
                           std::to_string(st.knowledge->tiles_enumerated()) + " foreign " +
                           std::to_string(st.knowledge->foreign_deltas()) + " off_batches " +
                           std::to_string(st.knowledge->batches_skipped_disabled())
                     : std::string()) +
                (st.hold_available ? std::string(", hold ") + (st.held ? "held" : "free")
                                   : std::string()) +
                ", title: " + st.title);
        } else if (s.verb == "screenshot") {
            act.screenshot = s.args[0];
        } else if (s.verb == "quit") {
            act.quit = true;
        }
        std::string ran = "ran line " + std::to_string(s.line) + ": " + s.verb;
        for (const std::string& a : s.args) {
            ran += " " + a;
        }
        log_.push_back(std::move(ran));
        ++next_;
        if (act.quit) {
            break;
        }
    }
    return act;
}

} // namespace anno::app
