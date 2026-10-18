# Decoded assets (RGBA images)

How original pixel data becomes 8-bit RGBA images, as implemented by `anno_assets` (C++ `src/assets`; Rust reference `crates/assets`). The format layouts are in `docs/formats/`; this file holds the mapping rules and the corpus checks. Images are rows top to bottom, 4 bytes per pixel `R, G, B, A`.

## Rules

| Source | Rule | Evidence |
|---|---|---|
| `.tex` texture | opaque index `i` → `[R, G, B, 255]` from the texture's palette (`HEAD.palette_id`); RLE gap → `[0, 0, 0, 0]`. `HEAD.color_key` is not applied (OQ-021). A missing palette is an error. | `docs/formats/tex.md`, `docs/formats/palette.md` |
| Figure frame (`Figuren*.dat`) | covered pixel → colour of the entry's own colour table `Frame::palette`, alpha from the 2-bit level of `A` formats (64, 128, 191, 255), else 255; `BSH_ALPHA` shadow pixel → `[0, 0, 0, 255]`; uncovered → `[0, 0, 0, 0]`. The `z` byte is not applied. | `docs/formats/figuren.md`; levels: `AnnoDisplay.dll` 0x10018890, 0x100193f0, 0x100199a0 |
| Building frame (`Anlagen.dat`) | as figures, but the colour table is the global table with id `entry.palette_ids[frame.palette]` (id 0 never matches), and pixels written past the frame width are kept by widening the image (OQ-025). | `docs/formats/anlagen.md` |
| Player colours | figures whose `BEWOHNTYPE` is one of 17 types (`ERKUNDER` … `ESEL`, `FLAGGE`) are decoded with every palette colour passed through the green test and recolour rule. 24-bit palette colours are the reference (owner decision 2026-10-17); no 16-bit quantisation. | `docs/render/iso.md`, "Player colours"; `docs/re/annodisplay.md` §8 |
| Units of a scenario | sprite set 0 of the unit's figure, frame `attr mod n` per zoom group, the same frame of the set's `SHADOW`; colour from the holding `SPIELER` block via the start-of-game assignment. | `docs/render/iso.md`, "Ships", "Soldiers"; `docs/re/annoscene.md` §7, §9 |
| Island ground (top-down inspection) | `scale × scale` pixels per tile from `Felder.tex` via `GroundCell::texel`; texture 0 → water colour, unknown texture → magenta. Not the isometric view. | `docs/formats/insel.md`, "Ground" |

## Corpus check (GOG 2.0.0.5 files)

`src/assets/tests/assets_corpus_test.cpp` decodes the sets below and folds every image into one FNV-1a 64 digest per set: per image `u16 width LE`, `u16 height LE`, the RGBA bytes; a failed image adds `FF FF FF FF`. The Rust reference produced the same digests (`cargo run --release -p anno-assets --example digest -- <game_dir>`, `crates/assets/examples/digest.rs`; 2026-10-17, re-run 2026-10-18), so both implementations are pixel-identical on the whole corpus. Any change to a rule above must change a digest; update it here and in the test together, with the reason.

| Set (order) | Images | Failed | Digest |
|---|---|---|---|
| every texture of every `.tex` file (paths sorted; a file that does not parse adds the ASCII bytes `NOPARSE`) | 5,176 | 0 | `d4bba222e9fbf012` |
| every frame of every `Anlagen.dat` sprite set, then its shadow (file order) | 28,899 | 0 | `4d53061488caa2b0` |
| every figure frame (sets and shadows, file order); a frame of a player-coloured type is followed directly by its decode in colour 2 `(255, 68, 0)` | 943,446 | 0 | `7ea22e33731cbd26` |
| ships and soldiers of every `.sze` (sorted paths; per unit: `u8` kind 0 = ship, 1 = soldier, `u16` figure, `u32` x and y float bits, then for each zoom group's frame and then each shadow frame `u32` offset + image, or the ASCII bytes `none`; then ASCII `\|`) | 14,883 (2,644 units) | 0 | `bb8c007fcecc115c` |
| ground of every island of `Scenes/0200Single/*.sze`, 4 px per tile | 99 | 0 | `f3d27f9beafdc758` |

Other corpus facts asserted there: 69 player-coloured figure entries (including 701, 718, 1543; no ship, not the thief 723); the green test hits 48 colours of the archer's (701) first table and 30 of `Anlagen.dat` table 109; at game start 130 colour slots are assigned over all scenarios, no human slot is removed, no human or computer player stays without a colour; `Single_24.sze` keeps its eight preset colours.
