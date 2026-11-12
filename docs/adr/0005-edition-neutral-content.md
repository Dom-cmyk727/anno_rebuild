# ADR 0005: The game edition a player owns does not matter (GOG or History Edition)

- **Status:** Accepted by the owner, 2026-11-12 (proposal and approval in the session that wrote
  [`docs/orchestration/reviews/2026-11-12-project-analysis.md`](../orchestration/reviews/2026-11-12-project-analysis.md)).
  Not implemented yet.
- **Date:** 2026-11-12
- **Evidence:** [`docs/research/he-vs-gog-comparison.md`](../research/he-vs-gog-comparison.md). The primary evidence
  base stays GOG 2.0.0.5 in `game/app`; History Edition facts are HE build evidence (Steam app 2820110).

## Context

The rebuild reads the original files from each player's own game folder. Future players own the GOG edition, the
History Edition, or both. The comparison of 2026-11-12 found:

- **The rules data is the same.** The History Edition's `Anlagen.addon` is byte for byte the GOG `Anlagen.dat`;
  `BGruppen.dat`, `Wachstum.dat` and `wegspeed.dat` are identical; all 368 figure types have identical entry-level
  gameplay records; all 50 scenarios are byte-identical.
- **A loader written for the GOG layout goes wrong in a History Edition folder in four ways:**
  1. `Anlagen.dat` exists there too, but is an older 909-entry variant. Opening it loads other rules, silently.
  2. `Texte.dat` at the root is German there. `Rules::city_names` is filled from its `[STAEDTE]` section and is part
     of the hashed state, so two players with different language files never share a state hash. That is true on
     one edition too.
  3. The scenarios are under `Languages\Eng\Scenes` and `Languages\Ger\Scenes`, not under `Scenes`.
  4. One island template of GOG is missing (464 against 465), and the link resolver's candidate list is built from
     the folder's content.
- **The graphics files differ in packing.** `FigurenHeader.dat` differs in bytes, so a hash of that file can never
  agree across editions. ADR 0003's handshake line "content hash of `Anlagen.dat`, `FigurenHeader.dat`, the island
  templates and the walk tables" would refuse every mixed game as written.

## Decision

1. **One game-source layer.** One module detects the edition from the folder's layout and maps logical names
   (buildings, figures, needs and growth tables, language text, scenarios, island templates) to files. Every reader
   of original files goes through it; no other code builds a path into the game folder. A folder whose layout it
   does not know is refused with one clear message that names what was looked for.
2. **One reference rule set.** The rules are the add-on rules: the 921-entry buildings file (`Anlagen.dat` in GOG,
   `Anlagen.addon` in the History Edition). The layer confirms the choice by a digest of the *parsed* data, so
   another variant cannot be loaded by accident. The pre-add-on "classic" rules are not supported for now (owner,
   2026-11-12).
3. **The join check compares parsed content, not files.** The content identity of a peer is made of digests the
   sim already computes from parsed data (type registry, rules, configuration, walk tables), the digest of the
   island templates the world uses, and the map or setup digest. Raw file bytes, the edition, graphics, sound and
   language are not part of it. **This replaces the file-based wording of ADR 0003's handshake** ("content hash of
   `Anlagen.dat`, `FigurenHeader.dat`, ..."); the rest of ADR 0003 is untouched.
4. **Language stays out of the simulation state.** A city's name is stored as an index into the name list; each
   player's own text file supplies the words on screen (owner, 2026-11-12). A name a player types is a command
   carrying the text, as any other input. No other language-dependent text may enter the hashed state.
5. **A fixed island-template pool.** A manifest in this repository lists the templates both editions have: the
   relative name and a digest of each, no bytes of the original. The link resolver and every generated or
   multiplayer world take their candidates from the manifest, never from a directory listing. The pool is the 464
   shared templates (owner, 2026-11-12). Original single-player scenarios are loaded as authored and are not
   affected.
6. **Proved by a test on a host that has both installs.** Loading each install through the layer gives equal
   content identities, and a two-peer session with one peer on each folder keeps equal state hashes. A command
   prints the identity per section, so a refused join names the section that differs.

## Consequences

- A mixed GOG / History Edition LAN game is possible without either player changing files.
- Players with different languages can play together, which the current code does not allow on one edition either.
- Point 4 changes the saved state, so it needs a new state identity. It lands together with another change that
  needs one (the wildlife port is the candidate), not alone.
- The History Edition's figure type 375 stalls the reference reader `tools/figuren.py frames`, and eight figure
  types (370-376, 453) differ in non-pixel records. Graphics are outside the lockstep, so this does not block a
  mixed game, but the layer's work includes settling it before a History Edition folder is called supported for
  drawing.
- Other editions (German retail discs, the co-developer's build) are unknown. They are handled by the same rule:
  the layer recognises a layout or refuses it, and the identity command says whether the content matches.

## Not decided here

- Whether the History Edition's five multiplayer scenarios are used as maps or as evidence for the fixed map
  (`docs/research/he-vs-gog-comparison.md` §3.5).
- Mods and the content overlay (PLAN §0.1); they extend the same identity.
