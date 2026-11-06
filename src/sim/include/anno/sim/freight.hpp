// Automatic freight authority, control and continuation. No external controller.
#pragma once
#include <string>

#include <anno/sim/apply.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>

namespace anno::sim {
class World;
struct TickContext;
struct Effects;
enum class FreightSignal : std::uint8_t;
struct FreightRoutes {
    static ApplyResult assign(World&, const Command&, const AssignFreightRouteCmd&,
                              const InAdvance*);
    static ApplyResult control(World&, const Command&, const ControlFreightRouteCmd&,
                               const InAdvance*);
    static Reject target(const World&, const Figure&, const FreightStop&, bool range);
    static std::string validate(const World&);
    static void visit(World&, const TickContext&, Figure&, Effects&, std::uint32_t& plans_started);
    static void rebuild_indexes(World&);
    static void manual_takeover(World&, FigureId, const InAdvance* = nullptr);

  private:
    static void observe(World&, const Figure&, FreightSignal, Effects*, const InAdvance*,
                        Reject = Reject::None, std::uint64_t amount = 0, std::uint64_t order = 0);
};
} // namespace anno::sim
