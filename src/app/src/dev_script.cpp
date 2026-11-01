// A scripted developer-tool session in the live play window (see the header).
#include <cmath>
#include <cstdlib>
#include <sstream>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_script.hpp>
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
    return std::nullopt;
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
    if (verb == "look" || verb == "pan") {
        return 2;
    }
    if (verb == "key" || verb == "select" || verb == "rotate" || verb == "wheel" ||
        verb == "checkpoint" || verb == "screenshot") {
        return 1;
    }
    if (verb == "click") {
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

} // namespace

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
        if (s.verb == "expect" && s.args.size() >= 2 && s.args[0] == "title") {
            // The rest of the line is the text (spaces kept as one).
            std::string rest;
            for (std::size_t i = 1; i < s.args.size(); ++i) {
                rest += (i > 1 ? " " : "") + s.args[i];
            }
            s.args = {"title", rest};
        }
        const int n = arity(s.verb);
        const auto have = static_cast<int>(s.args.size());
        if (n == -100 || (n >= 0 && have != n) || (n < 0 && have < -n)) {
            return std::string("line " + std::to_string(number) + ": '" + line + "' is not a step");
        }
        script.steps_.push_back(std::move(s));
    }
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
        if (!act.events.empty() || act.look || act.screenshot) {
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
                wait_turn_ = st.snapshot ? st.snapshot->turn : 0;
            }
            bool over = false;
            const std::string& what = s.args[0];
            if (what == "frames" && s.args.size() == 2 && arg(1)) {
                over = st.frames >= *wait_frames_ + static_cast<std::uint64_t>(*arg(1));
            } else if (what == "turns" && s.args.size() == 2 && arg(1)) {
                over = st.snapshot &&
                       st.snapshot->turn >= *wait_turn_ + static_cast<std::uint64_t>(*arg(1));
            } else if (what == "ms" && s.args.size() == 2 && arg(1)) {
                over = st.now_ms >= *wait_ms_ + static_cast<double>(*arg(1));
            } else if (what == "outcome") {
                over = st.latest && st.latest->state != CommandOutcome::State::Queued;
            } else if (what == "drawn") {
                over = st.latest && st.latest->state != CommandOutcome::State::Queued &&
                       drawn_note(st.latest, st.followed_turn, st.pending).empty() &&
                       st.pending == 0;
            } else {
                fail(s, "unknown wait");
                break;
            }
            if (!over) {
                if (what != "ms" && st.now_ms > *wait_ms_ + DevScript::kWaitLimitMs) {
                    fail(s, "not over after " +
                                std::to_string(static_cast<int>(DevScript::kWaitLimitMs)) +
                                " ms (outcome " + outcome_text(st.latest) + ", pending " +
                                std::to_string(st.pending) + ")");
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
            const auto want = arg(0);
            if (!want || st.tool == nullptr) {
                fail(s, "no number or no tool");
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
        } else if (s.verb == "expect") {
            if (s.args[0] == "title" && s.args.size() == 2) {
                if (st.title.find(s.args[1]) == std::string::npos) {
                    fail(s, "the title is '" + st.title + "'");
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
            log_.push_back("checkpoint " + s.args[0] + ": turn " +
                           std::to_string(st.snapshot ? st.snapshot->turn : 0) + ", frames " +
                           std::to_string(st.frames) + ", outcome " + outcome_text(st.latest) +
                           ", pending " + std::to_string(st.pending) + ", title: " + st.title);
        } else if (s.verb == "screenshot") {
            act.screenshot = s.args[0];
        } else if (s.verb == "quit") {
            act.quit = true;
        }
        log_.push_back("ran line " + std::to_string(s.line) + ": " + s.verb +
                       (s.args.empty() ? "" : " " + s.args[0]));
        ++next_;
        if (act.quit) {
            break;
        }
    }
    return act;
}

} // namespace anno::app
