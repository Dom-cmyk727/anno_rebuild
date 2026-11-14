// Explicit extension payload fields shared by all archives (state.md §9.8).
#pragma once
#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include <anno/sim/archive.hpp>
#include <anno/sim/extension_context.hpp>

namespace anno::sim {
template <class E, class Context> bool extension_is_default(const Context& context) {
    return E::state(context) == E::default_value();
}

template <StateSection Section, class A, class Context>
void extension_fields(A& a, std::string_view name, Context& context) {
    using List = typename std::remove_cvref_t<Context>::template List<Section>;
    const auto where = [&](std::uint16_t tag) {
        return "section " + std::string(name) + ": extension tag " + std::to_string(tag);
    };
    if constexpr (A::kReading) {
        List::visit([&]<class E> { E::state(context) = E::default_value(); });
        std::uint64_t count = 0;
        a.count(count, 6);
        std::uint16_t previous = 0;
        for (std::uint64_t i = 0; i < count && !a.failed(); ++i) {
            std::uint16_t tag = 0;
            std::uint32_t length = 0;
            a.u16(tag);
            a.u32(length);
            if (a.failed())
                break;
            if (i != 0 && tag <= previous) {
                a.fail(where(tag) + " out of order");
                break;
            }
            previous = tag;
            bool known = false;
            List::visit([&]<class E> {
                if (tag != E::tag)
                    return;
                known = true;
                a.scope(ScopeName{E::name}, [&] {
                    if (length > a.remaining()) {
                        a.fail(where(tag) + " payload length exceeds the remaining input");
                        return;
                    }
                    auto payload = a.subarchive(length);
                    E::fields(payload, E::state(context));
                    if (payload.failed()) {
                        a.fail(where(tag) + " payload: " + payload.error());
                    } else if (payload.remaining() != 0) {
                        a.fail(where(tag) + " payload has " + std::to_string(payload.remaining()) +
                               " unconsumed bytes");
                    } else if (extension_is_default<E>(context)) {
                        a.fail(where(tag) + " decodes to default");
                    } else {
                        const auto problem = E::validate(E::state(context));
                        if (!problem.empty())
                            a.fail(where(tag) + ": " + problem);
                    }
                });
            });
            if (!known)
                a.fail("section " + std::string(name) + ": unknown extension tag " +
                       std::to_string(tag));
        }
    } else {
        std::uint64_t count = 0;
        List::visit([&]<class E> { count += extension_is_default<E>(context) ? 0U : 1U; });
        a.count(count, 6);
        List::visit([&]<class E> {
            if (extension_is_default<E>(context))
                return;
            WriteArchive measure;
            E::fields(measure, E::state(context));
            // The writer, like the base field writers, requires valid state.
            // Context validation rejects oversized payloads before save is called.
            assert(measure.data().size() <= std::numeric_limits<std::uint32_t>::max());
            a.u16(E::tag);
            a.u32(static_cast<std::uint32_t>(measure.data().size()));
            a.scope(ScopeName{E::name}, [&] { E::fields(a, E::state(context)); });
        });
        static_cast<void>(where);
    }
}

template <class Context> std::string validate_extension_context(const Context& context) {
    std::string problem;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (
            [&] {
                using List = typename Context::template List<static_cast<StateSection>(I)>;
                List::visit([&]<class E> {
                    if (!problem.empty())
                        return;
                    problem = E::validate(E::state(context));
                    if (problem.empty() && !extension_is_default<E>(context)) {
                        WriteArchive measure;
                        E::fields(measure, E::state(context));
                        if (measure.data().size() > std::numeric_limits<std::uint32_t>::max())
                            problem = "payload exceeds u32 length";
                    }
                    if (!problem.empty())
                        problem = "extension " + std::string(E::name) + ": " + problem;
                });
            }(),
            ...);
    }(std::make_index_sequence<static_cast<std::size_t>(StateSection::Count)>{});
    return problem;
}
} // namespace anno::sim
