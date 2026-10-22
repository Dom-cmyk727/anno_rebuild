# Player profile (`SaveGame/<profile>/user.dat`)

**Status:** partial — layout of the default profile complete; some value meanings open (OQ-111).

## Files covered
- Glob: `SaveGame/*/user.dat`
- Count in GOG 2.0.0.5: 0 shipped (the install has no `SaveGame` folder). The game creates `SaveGame/ANNOnymous/user.dat` (216 bytes) at its first start; `ANNOnymous` is the default profile (registry `HKCU\Software\Max-Design\Anno1503` `AKTPLAYER`). The evidence below is that file, created by the first start in the Wine runner's base copy (2026-10-22).
- Related formats: [`chunk-container.md`](chunk-container.md) (generic chunks: 12-byte NUL-padded name, `u32` size, payload)

## Identification
- Root chunk `USDAT` holding one `PROFIL` container. P: the default profile the game created at its first start.
- Rewritten by the game at once when the in-game Music and Speech volume sliders were moved (two observed changes, Wine runner 2026-10-22, T16); whether every option change writes it is not checked.

## Layout
All integers little-endian. Offsets are relative to each chunk's payload.

| Chunk | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| `USDAT` | 200 | container | the profile file | P |
| `PROFIL` | 184 | container | one profile | P |
| `USGUID` | 16 | bytes | profile GUID; the human player's object holds the same 16 bytes at player `+0x64` | P; running game (T16, `docs/re/population.md` player object) |
| `SONGS` | 72 | 18 × (`u16` song id, `u16` enabled) | the in-game Music tab's playlist and its ticks; ids 2…19, all enabled | P; Music tab shows a ticked list |
| `VOLUME` | 8 | 2 × `i32` | the Music and Speech volume sliders: `0` = slider at the right end (full, the default), `-250` = slider at the left end. Which value is which, and how `-250` maps to DirectSound attenuation | P + experiment (both sliders dragged fully left: `0, 0` → `-250, -250`); order and scale ? OQ-111 |
| `SPEECH` | 4 | `u32` | which advisor speech messages play; the Speech tab lists 11 ticked categories, the default file holds `0xffffffff` | P; bit assignment ? OQ-111 |
| `HELPPLAYER` | 4 | `u32` | `1` in the default file; meaning not traced | P; ? OQ-111 |

## How the tools use it
- `tools/orig/userdat.py` reads the file and sets `VOLUME` to `-250, -250`. The Wine runner does this in every disposable copy before launch so test runs are silent (owner request 2026-10-22). The base copy is never changed, and a fresh run's Music tab shows the slider at its left end, so the game reads the patched value.
- A copy without a profile (a pristine install) gets one only at its first start, at full volume; the runner's driver-level silence still applies (`docs/testing/original-wine.md`).

## Open fields
- `VOLUME` order and scale, `SPEECH` bits, `HELPPLAYER` — OQ-111

## Tests
- No engine parser yet (the rebuild does not read the original's profiles). Reference: `tools/orig/userdat.py`.
