// Compile-time extension schema; no runtime registration (state.md §9.8).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

namespace anno::sim {
// This order is the fixed wire framing, not a gameplay/content limit.
enum class StateSection : std::uint8_t {
    Header,
    ClockRng,
    Settings,
    Ids,
    Players,
    Islands,
    Figures,
    Events,
    TimersMonopoly,
    Script,
    Count
};
struct ExtensionSchemaEntry {
    StateSection section;
    std::uint16_t tag;
    std::string_view name;
    friend bool operator==(const ExtensionSchemaEntry&, const ExtensionSchemaEntry&) = default;
};

template <bool TestRange, class... E> consteval bool valid_extension_list() {
    constexpr std::array<std::uint16_t, sizeof...(E)> tags{E::tag...};
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (tags[i] == 0 || (tags[i] >= 0xf000) != TestRange || (i != 0 && tags[i] <= tags[i - 1]))
            return false;
    }
    return true;
}
template <bool TestRange, class... E> struct BasicExtensionList {
    static_assert(valid_extension_list<TestRange, E...>(),
                  "extension tags must be nonzero, strictly sorted and in their assigned range");
    static constexpr std::size_t size = sizeof...(E);
    template <class F> static constexpr void visit(F&& f) { (f.template operator()<E>(), ...); }
};
template <class... E> using ExtensionList = BasicExtensionList<false, E...>;
template <class... E> using TestExtensionList = BasicExtensionList<true, E...>;

struct ExtensionContext {
    using Lists = std::tuple<ExtensionList<>, ExtensionList<>, ExtensionList<>, ExtensionList<>,
                             ExtensionList<>, ExtensionList<>, ExtensionList<>, ExtensionList<>,
                             ExtensionList<>, ExtensionList<>>;
    template <StateSection S> using List = std::tuple_element_t<static_cast<std::size_t>(S), Lists>;
};

template <class Context, std::size_t... I>
consteval auto extension_schema_impl(std::index_sequence<I...>) {
    std::array<ExtensionSchemaEntry,
               (Context::template List<static_cast<StateSection>(I)>::size + ... + 0)>
        out{};
    std::size_t at = 0;
    (
        [&] {
            using List = typename Context::template List<static_cast<StateSection>(I)>;
            List::visit(
                [&]<class E> { out[at++] = {static_cast<StateSection>(I), E::tag, E::name}; });
        }(),
        ...);
    return out;
}
template <class Context> consteval auto extension_schema() {
    return extension_schema_impl<Context>(
        std::make_index_sequence<static_cast<std::size_t>(StateSection::Count)>{});
}
inline constexpr auto kKnownExtensions = extension_schema<ExtensionContext>();
static_assert(
    [] {
        for (const auto& entry : kKnownExtensions)
            if (entry.tag == 0 || entry.tag >= 0xf000)
                return false;
        return true;
    }(),
    "production schema may not expose a test-range tag");
} // namespace anno::sim
