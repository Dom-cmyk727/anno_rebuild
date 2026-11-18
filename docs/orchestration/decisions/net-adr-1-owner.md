# NET-ADR-1: owner choices D-9 and D-10

**Proposed choices, not approvals — 2026-11-18.** Source baseline `b5c0ada2`.
This page prepares [ADR 0003](../../adr/0003-network-transport.md#0-current-contract-reconciliation-net-adr-1-2026-11-18);
answers remain blank until the owner decides. D-9/D-10 are the bundles in
[remaining work §7](../remaining-work-2026-11-16.md#7-decisions-for-the-owner), not
historical D9/D10 in [lan-endless §7](../../plan/lan-endless.md#7-the-owners-decisions-2026-10-26-taken-with-the-co-developer).

Already decided: own UDP with a transport seam; ship-start with players choosing where to
found; peace, shared visibility and no alliances
([policy](../../sim/shared-visibility.md)). These are not reopened. The fixed map (D-1) and
living defaults (D-2) remain separate owner choices. No map is selected or default enabled here.

| Choice and options | Recommendation, tradeoff and what is blocked | Owner answer |
|---|---|---|
| **D-9a: ADR approval.** Approve the reconciled proposal, or return named parts for correction. | Approve only after reviewing unresolved model boundaries in ADR §0/§8 and the choices below. Own UDP is already accepted; approval would authorize the protocol contract, not claim that any C++ network works. Production protocol work awaits that disposition. | ___ |
| **D-9b: UDP port.** Choose an explicit project default, or require an explicit host/client setting with no default. | Require an explicit setting initially; avoids inventing an original port (OQ-250), at the cost of manual configuration/discovery setup. Windows socket startup is blocked until a port is supplied; headless work is not. | ___ |
| **D-9c: operational limits.** Approve ADR §10/§8.7 proposed configurable budgets as initial test inputs, request alternatives, or defer production values to measurements. | Use them as labelled test inputs and defer production adoption until real saves/links are measured. In particular, 17,130 fragments has no headroom over the historical largest save; 1,200 turn bytes is not a universal bound. Production admission/deadline policy remains blocked; synthetic 300/400/500 ms comparisons choose no timer. | ___ |
| **D-10a: Venetians/natives.** Include their faithful behavior in the MVP, or omit these computer participants until their subsystem exists. | Omit initially; avoids pretending CompLogic exists, but narrows the original world's activity. Historical D8 says on; the later no-AI MVP explicitly asks this question. Inclusion blocks on faithful computer-player behavior, not merely importing their records. | ___ |
| **D-10b: dropped human without AI.** Keep the seat/world intact with no new commands and allow reconnect; pause until reconnect/owner action; or end the session after the chosen grace. | Keep the seat idle with reconnect entitlement, preserving other humans' progress; its economy may continue without player commands. This replaces historical D9's AI takeover only if approved. No command bot or abandoned-seat takeover is implied. Disconnect/expiry and end-of-session policy await the answer and bounded reconnect work. | ___ |
| **D-10c: host migration.** Deliver inside the MVP, or retain it as a required milestone immediately afterward and explicitly narrow MVP acceptance. | Include inside MVP, preserving the owner's robust-session goal; costs the ownership/turn-history recovery and partition tests in ADR §9. Deferral makes early play depend on the host staying. Overall requirement is retained whichever timing is chosen. | ___ |
| **D-10d: fish on the selected LAN map.** Enable faithful fisher rules explicitly for that setup, or leave them off and label the economic limitation. | Enable for a reviewed setup once its integration and chosen map are proved; needs setup propagation and a real-map canary, not inserted catchable fish. Current World supports header tag 1, runtime fish fill and the 810/3978 fisher path; CLI enables it only for plain `--play`. The pinned `ship_start` loader/install API does not forward ExtensionContext. An accepted branch-only setup seam is not yet main. Selected-map/ship-start integration remains a dependency; HE picture parity is not claimed. | ___ |
| **D-10e: construction stage.** Retain current authoritative availability enforcement, or specify a separately reviewed opt-in bypass with its semantic identity. | Retain enforcement; preserves original scenario permissions/stage/research alternatives. `World::construction_eligibility` already gates BuildCmd in `apply.cpp`; unknown script context refuses. Bypass is a behavior change, not a missing-helper fix, and would require an explicit design under ADR 0006. LAN acceptance depends on its starting setup being able to progress under the selected rules. | ___ |

Implementation facts above come from `game_setup.hpp`, `extension_context.hpp`,
`build_availability.cpp`, `apply.cpp`, `cli.cpp` and `sim_load/ship_start.hpp/.cpp`
at the pinned baseline; [ADR §0.1](../../adr/0003-network-transport.md#01-compatibility-components-and-implemented-seams)
distinguishes existing identity components from missing network comparisons. Excluding language
and graphics is accepted [ADR 0005](../../adr/0005-edition-neutral-content.md) policy, with
localized city names still an implementation dependency. [ADR 0006](../../adr/0006-save-sections.md)
requires matching recognized schemas/active rules and refusal of unknown tags; no permissive
compatibility option is proposed here.

After answers: the orchestrator records approval/disposition and sizes one headless protocol
packet. No answer is inferred from silence, this recommendation, or a green docs CI run.
