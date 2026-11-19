# Bounded Windows capture: two raster FP checkpoints

Prepared by WILDLIFE-RASTER-FP-1, 2026-11-19. **NOT_RUN on the Mac.**
For orchestrator coordination only; no Windows task is dispatched here. This
observes the original GOG installation, not HE or the rebuild. It changes no
control word, input, script, registry or saved state. Private dumps stay in cache;
publish only addresses, numeric findings, counts and hashes.

## Input and stop boundary

Use a fresh process of the original GOG game, launch under a 32-bit-aware Windows
debugger BEFORE DLL initialization. Record the executable name/hash, debugger/
OS and module load bases. Required DLL SHA-256:

| Module | SHA-256 |
|---|---|
| AnnoWelt.dll | f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2 |
| AnnoFrame.dll | dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91 |
| AnnoScene.dll | 5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38 |

If an identity differs, stop and report the difference; do not transplant these
offsets into HE or silently choose another executable. Start the normal Tutorial
lesson whose loaded file is Scenes/Tutorial/01Discover01.sze, unchanged. Verify
that path in the normal file-open/load trace; if it cannot be verified, report
that limitation instead of injecting a load or modifying a script. No commands,
building, savegame reload or gameplay tick is needed for the selected observation.

Use deferred module-load breakpoints. Every address below is **module base +
RVA**, NOT an assumption that the preferred base 0x10000000 is the actual base.
If attaching after startup misses checkpoint A, that run cannot establish A;
restart once from a fresh process rather than infer its CW from later output.

## Non-mutating observation

At a debugger stop, record the stopped thread id and its x86 floating context.
Read CW/status/tag/stack from the debugger's saved context; a helper may use
[GetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext)
with floating-point context flags, or the appropriate
[WOW64 context API](https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-wow64getthreadcontext).
The thread must be stopped; querying the running or helper's current thread
does not supply the target context. The
[x86 CONTEXT layout](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context-r2)
contains FloatSave. Do not call target _controlfp, execute injected fnstcw, use
SetThreadContext, edit registers, reset the FPU, or insert a target logging call.
Ordinary debugger stepping/breakpoints may execute ORIGINAL instructions only.

Record full 16-bit CW, then separately decode PC (mask 0x0300), RC (0x0c00)
and exception masks (0x003f). Do not infer RC from PC. Record status separately;
the conversion helper can restore CW without restoring every status bit.

## Stops and the facts they establish

| Stop | Location and selection | Capture / claim |
|---|---|---|
| A0, optional corroboration | Welt +0xb6e12 entry, then +0xb6e23 return | Before/after reset CW + thread. Proves what changed there, not later table/raster state |
| A1, required | Welt +0x7c760, first entry after module initialization begins | Actual table-initialization CW/status/thread; not the callback profile |
| A2, required | Welt +0x7c780, before return after the initializer loop | CW/status/thread and completed table indices 5,6,248 at Welt +0xe2d90 +4*i; store raw f32 bits privately |
| B0, required | Welt +0xa3cc0, first call with [ESP+4]=2 AND return address [ESP]=Welt +0xaba99 | Actual first postload callback thread/CW. Other flags-2/0xff calls are not substituted |
| B1, required | Same selected invocation at Welt +0xa3cce | Confirm arithmetic-entry CW before scales/sums; keep B0 selection/identity |
| B2, small corroboration | That invocation's first bucket at Welt +0xa3e73 before merge; then +0xa3e90 before write | Raw converted EAX, full CW, old/next output byte and bucket. Compute expected output from recorded inputs/profile, not a C++ golden |
| B3, required | Same selected invocation at Welt +0xa40be, before pops/return | Ending CW/status, generation and same raster/table identities; distinguish normal restore from changing profile |

At B0 ECX is the raster. Record raster +4/+8 dimensions, +0xc generation,
+0x10 world pointer, +0x14 byte-plane pointer, and rectangle pointer [ESP+8]
with its four signed endpoints. Verify the world vtable resolves to the standard
Welt table RVA 0xccdd8 (whose +0x1d0 is +0xaba70); record a mismatch and stop
this selected-path claim if not. Keep the plane's prior high bits in any sample
comparison. No copying arbitrary pixels or original scene bytes into git.

At B2 the addressed frame keeps f32 sum at [ESP+0x1c], sx at [ESP+0x14], sy
at [ESP+0x10]. Record these as raw bits, bucket coordinates, sample endpoints/
count and source corner-byte sequence privately if validating the first natural
bucket. At +0xa3e73 EAX is converted; let original +0xa3e73 establish EBP before
reading the old byte at [ESI+EBP]. The +0xa3e90 write value is AL. Do not take a
full map census. Generation B3 should be B0+1 modulo 2^32 for flags 2, including
an empty-loop invocation; do not artificially alter the rectangle to force one.

Record the same three table entries again at B0/B3. Stable bits corroborate no
table rewrite between these observed points, not absence of every possible writer.
If no matching callback occurs for this normal path, preserve that result and
report load path/modules; do not force the callback.

## Discriminating outcomes and interpretation

These are source-model expectations, NOT original observations:

| Profile/input distinction | Expected difference |
|---|---|
| Table nearest precision 53 vs 24 | T[6] bits 0x3e75c28f vs 0x3e75c28e |
| Table precision 53 nearest vs down | T[248] bits 0x411eb852 vs 0x411eb851 |
| Fixed T53/nearest, callback 53 vs 24 nearest, four height-5 samples | Raw conversion 4 vs 5 |
| Fixed T53/nearest, callback 53 nearest vs down, ordered 255,255,255,227 | Raw 248 vs 247; old 0x80 merges to 0x90 vs 0x8f |

Do NOT inject those artificial sample patterns into the original. They explain
why the measured CW/table bits matter and can be reproduced later by the private
model. A natural all-zero bucket agrees under many profiles; it is corroboration,
not a substitute for A/B control-word records. T53/down with C24/up can cancel
to the same raw 248 as T53/nearest with C53/nearest. Thus final byte agreement
alone cannot settle the two checkpoints or a missing thread relationship.

Conclude separately: observed A profile, observed B profile, same/different
thread ids, table bits and any normal-return CW difference. Matching observations
close only this fresh supported Tutorial load; do not generalize to every later
thread, save/load history, trap or graphics-device reinitialization. Keep exact
command/debugger transcript and context/dump SHA-256 privately. Stop after one
successful A/B load (or one diagnosed failure); no broad runtime campaign.
