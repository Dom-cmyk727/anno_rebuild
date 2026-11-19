# WILDLIFE-SPECIES-LOAD-1 — focused WARE source clarification

2026-11-19. Independent primary predicate review during implementation, not a
review/acceptance of the unfinished code or a new owner gameplay decision.
Packet remains [WILDLIFE-SPECIES-LOAD-1](../tasks/wildlife-species-load-1.md), on
corrected release base `901a8467f766825ae9683cb41dee33522a9aff39`.

## Finding and approved boundary

The selected Tutorial climate2 ring contains type131/kind50 with a safe WARE name
not matched by the current143-ordinal goods getter. The original cannot be modeled
by inventing a new goods ordinal, removing this species or changing selection.

`/root/review_docs_sim_dispatch` independently checked actual AnnoGame bytes:
WARE scans ordinals0..142; exhausted path0x10010436–0x10010445 returns1 without
writing descriptor+0x100. Only match0x1001044a writes. Constructor0x100111a8 zeros
ECX and0x10011259 initializes+0x100 to0. Fresh factory0x1000222a ->0x1000641f
->0x10011dd0 ->0x10011190 establishes the constructor value. Root also inspected
the retained aligned lookup listing. The exploratory0x10011230 dump begins inside
an instruction and is not primary evidence; use the aligned constructor entry.

**APPROVED:** generic non-New selector facts for a fresh descriptor may represent
safe unmatched WARE as retaining numeric yield0. Record distinct provenance for
absent record, matched name and unmatched-retained-default. Unique selected ENTRY
and unique WARE plus current unsafe-input checks remain required.

**Not approved by this finding:** reused descriptors or overlays. Original0x10002228
can bypass construction when the descriptor already exists, so retaining its old
value is not necessarily zero. No actual catalogue mutation, unsupported profile
or arbitrary malformed-name semantics are established. The sealed New creation
binder retains its conservative unknown-WARE refusal. Type131 stays in its ring
slot; selector and draw accounting remain unchanged.

Executor may continue the existing S1–S5 scope with legal matched/default controls.
Final implementation, tests and source pin still require normal independent package
review. No state/default/tag/identity/PRNG, World or other-lane change is authorized.
No gate was interrupted or repeated for this focused static check. Original runtime
and HE execution remain NOT_RUN.
