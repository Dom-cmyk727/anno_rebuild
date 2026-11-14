# History Edition: how the in-game UI scales with the window (UI-HE-1)

**Status:** static RE plus one visual sample, 2026-11-14 (UI lane). Read-only analysis of the HE
binaries with pefile + capstone (an x86-64 build, so Ghidra on this Windows host was not used). A
subagent found the code paths; the UI lane re-disassembled the key sites itself (marked
**re-checked**). No HE bytes, pixels or captures are in git; captures stay in
`E:\anno1503-worktrees\he-ui-captures` (outside the repo).

HE install: Steam "Anno 1503 - History Edition" (`Anno1503.exe`, `AnnoDisplay.dll`, `AnnoFrame.dll`,
`View.dll`, all x86-64). Addresses are **RVAs**; image bases are 0x140000000 (`Anno1503.exe`) and
0x180000000 (the DLLs). Function names come from `AnnoDisplay.dll`'s export table.

The original (GOG 2.0.0.5) has no UI scale: it draws its in-game UI 1:1 at one of five fixed display
modes (`ui.md` §5). Everything below is an **HE addition**. Per the owner's rule (2026-11-14) the
original is the base and HE-only behaviour becomes an option on top of it.

## 1. The setting

- The player's "Interface-Skalierung" (options, 1.0 to 2.0) is the registry value `UI_SCALE` (a
  `%f` string) under `HKCU\Software\Ubisoft\Anno1503HistoryEdition`, next to `SCREEN_WIDTH`,
  `SCREEN_HEIGHT`, `SCREEN_WINDOWED`, `MUSIC_VOLUME`, `SOUND_VOLUME`.
- Read: `Anno1503.exe` 0x17ef (`lea rdx,"UI_SCALE"`), then `RegQueryValueExA`, then `atof`, converted
  to float at 0x1814/0x1818. Default 1.0 (0x1568, constant at 0x587c = 0x3F800000). The exe applies
  no clamp; it stores the value at `AdditionalOptions+0xc` (0x1efc). `tDisplay::GetAdditionalOptions`
  is `AnnoDisplay` 0x189f0.
- Written back with `"%f"` + `RegSetValueExA`: `AnnoDisplay` 0x27ab2..0x27b0c.
- The game reads the setting at start. The menus are full-screen art at a fixed 800 × 600 frame and
  do not follow the setting (`OnResize(false)` uses 800 × 600 at scale 1.0).

## 2. The effective scale: `tDisplay::OnResize(bool inGame)`, `AnnoDisplay` 0x2ccf0

1. W, H = the window's client size (the last `GetClientRect`, stored at 0x5a759), each **clamped up**:
   `W = max(W, 1024)`, `H = max(H, 768)` (0x2cdb4..0x2cdca, **re-checked**).
2. `s = UI_SCALE` (0x2cde0), stored at `tDisplay+0x554` (`GetUIScale`, 0x2cdfa).
3. Step-down loop, 0x2ce11..0x2ce8d (**re-checked**): `uiW = trunc(W * (1/s))`,
   `uiH = trunc(H * (1/s))` (float32, `cvttss2si`); while `uiW < 1024 || uiH < 768`, `s += -0.1f`
   (constant 0xBDCCCCCD at 0xff901c). Once `s <= 1.0` (0x3F800000 at 0xff8df8): `s = 1.0`,
   `uiW × uiH = W × H`.
   So `s` is the largest of `UI_SCALE, UI_SCALE − 0.1, …` that leaves the UI at least 1024 × 768 UI
   pixels. No other rounding or snapping.
4. `UpdateResolution(W, H, uiW, uiH)` (vtable slot 0x1b0, called at 0x2ceba).
5. `UpdateResolutionAndClipping` (0x5a961..0x5aab1) **aspect-fits** the uiW × uiH layer into the
   client, uniformly:
   - height-limited: `k = H / uiH`, `offX = trunc((W − k·uiW + 1) · 0.5)`, `offY = 0`;
   - width-limited: `k = W / uiW`, `offY = trunc((H − k·uiH + 1) · 0.5)`, `offX = 0`.
   `k` is stored as UI scale X and Y (`tDisplay+0x1a8/+0x1ac`, at 0x2d16e/0x2d174), the offsets at
   `+0x1b0/+0x1b4`. Since uiW, uiH are truncations of W/s, H/s, k is s up to a fraction of a pixel and
   the offsets are 0.
6. At `s <= 1.0` there is no separate UI layer (it is hidden at 0x2d1a3..0x2d1b8); `GetDrawBitmapUI`
   (0x2a7c0) returns the game bitmap, so the UI is drawn **1:1** into the game layer of
   `max(W,1024) × max(H,768)`. At `s > 1.0` the UI bitmap is resized to uiW × uiH (0x2d27f).

**Filtering:** bilinear. The sampler made at 0x5e081 (shader setup 0x5dd10, `CreateSamplerState`)
has `Filter = 0x15` (D3D11 `MIN_MAG_MIP_LINEAR`); both renderer shaders (0x5a10e clamp, 0x5a121 wrap)
use it. Which of the two draws the UI layer is open; both are bilinear.

### Predicted factors (offsets 0 in every case)

| Client | UI_SCALE 1.0 | UI_SCALE 1.5 | UI_SCALE 2.0 |
|---|---|---|---|
| 2542 × 1351 | 1.0 (1:1) | s 1.5, UI 1694 × 900, k 1.5006 | s 1.7, UI 1495 × 794, k 1.7003 |
| 1920 × 1080 | 1.0 | s 1.4, UI 1371 × 771, k 1.4004 | s 1.4, UI 1371 × 771, k 1.4004 |
| 1024 × 768 | 1.0 | 1.0 (falls back) | 1.0 (falls back) |

**Visual check:** one sample so far. UI_SCALE 2.0, windowed client 2542 × 1351: the side panel's tab
row matched the GOG chrome at factor **1.696** (bilinear resample, mean abs diff 4.7, tab row
y 298 ≈ 175 × 1.696), right-anchored at the client's right edge and top-anchored. The prediction is
1.7003; the difference is within the measurement's resolution. The samples at 1.0 and 1.5 are still
to be taken.

## 3. Anchoring

- The whole UI surface sits at (offX, offY), (0, 0) in every case above.
- **Side panel**, `View.dll` 0x3b428..0x3b455 (**re-checked**): `x = (Sizex(UI bitmap) < 1024) ? 779.0
  : Sizex − 245` (779.0 at 0xb4f00; `lea ecx,[rax−0xf5]` at 0x3b44c; `Sizex` is the
  `tDrawBitmapDD11` vtable slot 0xb0, `AnnoDisplay` 0x17270). Right-anchored in UI pixels. At
  2542 × 1351, UI_SCALE 2.0: UI x 1250, client x 2125.4 to 2542.
- This is the same rule `ui.md` §5 records for GOG at 1024 × 768 (x = W − 245 = 779), there only
  checked at that one size. GOG's `View.dll` has no `779`/`245` immediate in `.text` and no absolute
  reference to its 779.0 float (0x10033c08), so GOG gets the value some other way, probably from the
  chrome graphic's width. Open.
- A `View.dll` helper at 0x1f3d4 keeps windows inside width `Sizex` and height `min(Sizey, 768)`,
  which suggests pop-ups stay in the top 768 UI rows. Inferred, not proven.

## 4. Mouse clicks

`AnnoFrame.dll` window procedure 0x224b8 reads the UI and game scale/offset through vtable slots
0x160..0x198 (0x22525..0x2259a). For mouse messages (0x22a26..0x22a88, **re-checked**):
`uiX = trunc((x − uiOffX) / uiScaleX)`, `uiY = trunc((y − uiOffY) / uiScaleY)`; game coordinates use
the same form with the game scale and offset. A second path at 0x22c03 subtracts a window origin
first (probably the mouse wheel, whose coordinates are screen-relative; message number not
confirmed).

## 5. What this means for the rebuild

- The faithful base stays the original's 1:1 UI at 1024 × 768 with the right chrome at W − 245.
  HE's rule at `UI_SCALE` 1.0 is exactly that base (1:1, right-anchored) at any window size of at
  least 1024 × 768.
- HE's scale setting is an option on top: step down by 0.1 from the setting until the UI keeps at
  least 1024 × 768 UI pixels; draw the UI layer at uiW × uiH and stretch it bilinearly, uniformly, to
  the client; map clicks back with `trunc((p − off) / k)`.
- The rebuild's own UI scale question (ADR 0004 R3) can take this rule as its evidence; which
  default the rebuild uses is the owner's call.

## Open

- Anchoring of the bottom status bar, the minimap and individual pop-ups in HE (the `−35` sites seen
  are not proven to be the status bar).
- The panel's y origin in code (y = 0 matches the capture).
- Which renderer shader draws the UI layer.
- Context of the `OnResize` call at `AnnoFrame` 0x56b83 (passes the flag through); the call with
  `false` at 0x3d6e0 is the menu path.
- The `View.dll` global float at 0xcb8c8 in the clamp helper.
- `View.dll` 0x3b898 picks 0.78125 (800/1024) when the UI is narrower than 1024; may relate to OQ-535
  (the `8` texture set). Not checked.
- `AnnoDisplay`'s VMProtect sections (`.vmp0..2`) were not examined.
- Visual samples at UI_SCALE 1.0 and 1.5.
