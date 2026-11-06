// Automatic freight authority, control and continuation. No external controller.
#pragma once
#include <string>

#include <anno/sim/apply.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>

namespace anno::sim {
class World;
struct FreightRoutes {
    static ApplyResult assign(World&, const Command&, const AssignFreightRouteCmd&,
                              const InAdvance*);
    static ApplyResult control(World&, const Command&, const ControlFreightRouteCmd&,
                               const InAdvance*);
    static Reject target(const World&, const Figure&, const FreightStop&, bool range);
    static std::string validate(const World&);
    static void manual_takeover(World&, FigureId);
};
} // namespace anno::sim
