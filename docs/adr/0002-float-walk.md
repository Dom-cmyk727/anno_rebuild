# ADR 0002: The float walk

- **Status:** Proposed
- **Date:** 2026-10-27

The owner decided on 2026-10-24 to reproduce the original walk bit for bit (OQ-162). This document is the model. Stage 2 does not start until the owner accepts it. It adds no code.

The sequence it emulates is `docs/re/transport.md` §4. The control word that sequence runs under is §4.1. One point is still open: OQ-195, whether `DDRAW.dll` changes the word after `SetCooperativeLevel` when the game passes neither FPU flag. The model below is the word the game's own startup writes. If OQ-195 resolves the other way, the mantissa width changes before any code is written, and this ADR is revised first.

## Decision

The walk lives in `src/sim` as small-integer soft-float. No native `float` or `double` enters the simulation state or a hashed path.

A value kept on the x87 stack is an 80-bit register with the precision control set to 53-bit mantissa and the x87 exponent range (15-bit exponent). Rounding is to nearest, ties to even. A store to a figure field rounds that register to binary32 and keeps the bits in a `u32`. A bit copy (`mov` of a dword, including the integer 0 written as remaining and the bits of 1.0 written as a one-tile amount) copies the bits and does not round.

The operations are add, subtract, multiply, divide, compare, and the two conversions. Compare reports the four x87 conditions the walk tests: equal-or-unordered (C3), less-than (C0), and less-or-equal-or-unordered (C0 or C3). The conversions are `__ftol`: rounding toward zero to a 64-bit integer, then the low 32 bits, and the ambient rounding restored afterwards. They are the two tile truncations in `0x10057ae0`. The integrator has none.

This is not binary32 arithmetic with a binary32 exponent. A product of two binary32 values is exact at 53 bits, so a multiply that is stored at once matches a binary32 multiply. A division that the next operation consumes before any store does not. The cut step's carried time (`time − remaining / rate`, stored once) and the piece installer's update of the running position (`+0x58` / `+0x5c` / `+0x60` from an unstored quotient) are that case. Soft-float that rounded every operation to binary32 would miss them.

Figure fields that hold a float result stay `u32` bit patterns: position `+0x24` / `+0x28` / `+0x2c`, velocity `+0x30` / `+0x34` / `+0x38`, remaining `+0x3c`, rate `+0x40`, and the running position `+0x58` / `+0x5c` / `+0x60`. The stamp stays a `u16`. The lint that forbids a native float in hashed state does not change.

The height `0x10084e10` leaves in st0 is an input of the walk, not a formula this ADR fixes (OQ-196). Trip time does not read it. The `{direction, count}` pairs stay OQ-161. Stage 2 can take those pairs from the path the sim already has and still test the arithmetic below; it cannot claim a bit-exact route until OQ-161 is closed.

## Rejected

**Native float under `/fp:strict` and `-ffp-contract=off`.** x87 and SSE do not agree on precision or on exponent range, and the two compilers do not offer one mode that is the 53-bit x87 register with a binary32 store. The state lint forbids a native float in anything that is hashed. A strict flag would not make a `float` member the right width anyway: the carried time is wider than binary32 until the store.

**Binary32 for every operation, including the ones the game leaves on the stack.** It matches the stored products and the orthogonal bit copies. It does not match the cut step or the running-position update. Those are the operations a trip's later ticks consume.

**Keeping the integer walk as the faithful model.** It is what the sim does today, and it stays until stage 2. It is an approximation: Q24.8 waypoints, progress in units of 2^-24 tile, a speed rounded once at load. OQ-162 already records that a trip can differ by rounding. The owner's decision was the original sequence, not this approximation.

## Stream and goldens

Stage 2 bumps the sim stream. Position, velocity, remaining, rate, and the running position move from the integer walk's fields to the `u32` bit patterns above. Every golden that contains a figure is re-recorded. The bump is one commit, after the soft-float tests pass and before the goldens change, so a mismatch is the walk and not a silent edit of an old file.

## Test plan

Tests may use host IEEE float. The simulation does not.

1. **Each operation against the host.** At least 10^8 random operands per operation, plus the edges: both signed zeros, subnormal inputs and results, overflow to infinity, underflow, a quiet NaN on each side of a compare, and ties at the binary32 and at the 53-bit boundary (a mantissa that is exactly halfway). The host side runs at binary64, which is wider than 53 bits, and rounds to 53-bit mantissa and to binary32 with the same rules. A mismatch prints the two bit patterns and stops. The conversion test includes values just below and just above an integer, negative values (toward zero, not floor), and a value that does not fit in a signed 32-bit integer.
2. **The walk against a hand-derived trace.** One figure, a route of one orthogonal segment and one diagonal segment, a tick that cuts a segment and a tick that does not. The trace lists every rounding point in §4.3 and §4.6 and the expected `u32` after each store. The height input is a chosen bit pattern, so OQ-196 does not block this test. The pair list is chosen, so OQ-161 does not block it either.
3. **Later, the original's positions.** T30 step 7's reader supplies figure positions from the running original. That comparison waits on OQ-161 and OQ-196, and on a recorded control word if OQ-195 is still open. It is not a gate for the arithmetic in (1) and (2).

## Performance

`bench_sim` reports the walk's share of a tick before the change and after it, on the same scene and the same tick count (`docs/perf/README.md`). Stage 2 does not land without those two numbers. No budget is invented here.
