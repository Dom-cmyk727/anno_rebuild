# ANNO 1503 — known issues of the original (research as of 2026-10-17)

Catalogue of bugs, crashes, design flaws and technical problems reported for the original game and its re-releases, so the rebuild can deal with each one deliberately. Every entry links only to pages that were actually opened for this research. Where a claim rests on a single forum post, a FAQ contributor or a reviewer's remark, the evidence level says so. Nothing here was checked against the binaries yet. Where an issue matters for the sim, the RE work in `docs/re/` should confirm the mechanism before we "fix" anything.

## Method

- **Date:** 2026-10-17.
- **Searched:** German and English web search for patch notes, reviews, forum threads and FAQs. Opened: German press (GameStar, PC Games, Gameswelt, PC-Welt), Anno Union devblog, Steam discussions for the History Edition (app 2820110), the GOG store, GOG forum and gogdb, Wikipedia (de/en), Tim Howgego's Anno 1503 FAQ (on timhowgego.wordpress.com and its full-text mirror on strategygamers.com), GitHub/Lutris/Gist pages for community fixes, and GamePressure.
- **Not reachable or nothing usable:** PCGamingWiki (HTTP 403), WineHQ bug tracker and forum (HTTP 403 / DNS failure), WinFuture news links (redirect to an index page, so not cited). AnnoZone forum threads and r/anno did not appear in the search results. **These are gaps.** A manual pass over AnnoZone (`annozone.de/forum`) is the most valuable follow-up.
- **Version naming:**
  - **Release:** the German release was v1.00 (25 Oct 2002). The first patch (announced as "by 6 Nov 2002") followed, then v1.02 (6 Dec 2002).
  - **Non-German releases** shipped with patches "up to and including 1.04.02 (12 March 2003)" (Howgego).
  - **German "Classic" patch 1.05:** released 25 Sep 2006.
  - **Addon** *Schätze, Monster und Piraten*: 7 Nov 2003 in Germany, May 2004 in other European markets.
  - **GOG "A.D.":** gogdb lists builds "3.00 (gog-5)" dated 2015. A 2019 Wine script expects the installer `setup_anno_1503_2.0.0.5.exe`. Which game build sits inside which GOG installer is open.
  - **History Edition (HE):** Ubisoft Mainz, 2020, 64-bit port. It has been on Steam since 2024.
- **Evidence levels:**
  - **P (patch notes / official):** named in an official patch list, a developer statement or a developer blog.
  - **W (widely reported):** two or more independent sources.
  - **S (single report):** one forum post, one FAQ contributor or one reviewer.
- **Rebuild implication keys:**
  - **FIX:** always fixed in the rebuild.
  - **RULE:** configurable in the `rules` config (faithful default unless stated).
  - **FAITHFUL:** keep as original. Optionally provide a QoL rule.
  - **N/A:** does not apply to a clean-room engine. Kept for completeness.

## Key sources (short names used below)

| Key | URL |
|---|---|
| GW-P1 | https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/news/alle-bugfixes-des-patches-bekannt-69624 |
| GS-P1 | https://www.gamestar.de/artikel/anno-1503-erster-patch,1339452.html |
| PCG-102 | https://www.pcgames.de/Anno-1503-Aufbruch-in-eine-neue-Welt-Spiel-17999/Downloads/Anno-1503-v102-dt-110641/ |
| PCG-105 | https://www.pcgames.de/Anno-1503-Aufbruch-in-eine-neue-Welt-Spiel-17999/Downloads/Patch-behebt-Hardware-Probleme-und-andere-Wehwehchen-508060/ |
| GW-MP | https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/news/arbeiten-am-patch-eingestellt-75721 |
| GS-HIST | https://www.gamestar.de/artikel/anno_historie_die_geschichte_der_aufbau_reihe,2562562,seite2.html |
| GS-INT | https://www.gamestar.de/artikel/anno-entwickler-im-interview,1341187.html |
| AU-HE | https://www.anno-union.com/devblog-anno-1503-history-edition/ |
| GS-HE | https://www.gamestar.de/artikel/anno-history-collection-verbesserungen,3359045.html |
| GW-KL | https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/komplettloesung/komplettloesung-4614/3 |
| GW-TEST | https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/test/anno-1503-aufbruch-in-eine-neue-welt-3754/3 |
| GW-ADDON | https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/test/anno-1503-aufbruch-in-eine-neue-welt-4327 |
| PCG-FB | https://www.pcgames.de/Anno-1503-Aufbruch-in-eine-neue-Welt-Spiel-17999/Specials/Feedback-Ergebnis-Anno-1503-113360/2/ |
| PCG-ED | https://www.pcgames.de/Spiele-Thema-239104/Specials/Editorial-Was-macht-eigentlichAnno-1503-90213/ |
| PCG-KT | https://www.pcgames.de/Anno-1503-Aufbruch-in-eine-neue-Welt-Spiel-17999/Tests/Anno-1503-im-Klassik-Test-618674/ (reader comments) |
| PCG-ADDON | https://www.pcgames.de/Anno-1503-Schaetze-Monster-und-Piraten-Spiel-16616/Tests/Anno-1503-Add-on-252100/ |
| TH-FAQ | https://strategygamers.com/walkthrough/anno-1503-the-new-world (full text of Tim Howgego's FAQ v1.03, 2 Nov 2003, community-contributed, quotes named players) |
| TH-TECH | https://timhowgego.wordpress.com/anno_1503/technical |
| TH-INTRO | https://timhowgego.wordpress.com/anno_1503/introduction |
| GOG-FORUM | https://www.gog.com/forum/anno_series/anno_1503_not_really_working_on_windows10_amd |
| GOG-STORE | https://www.gog.com/en/game/anno_1503_ad |
| WIKI-DE | https://de.wikipedia.org/wiki/Anno_1503 |

Steam threads (History Edition, app 2820110) are cited by full URL inline.

---

## 1. Crashes and stability

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-001 | "Occasional crashes" (unspecified) and a crash when leaving the tutorials. | v1.00, fixed in first patch | P | GW-P1, GS-P1 | FIX: crash-free tutorial exit. Add a headless smoke test that runs every tutorial/scenario to its exit. |
| ISS-002 | Crash when demolishing the Medicus (doctor) house. | ≤1.04, fixed in 1.05 (German Classic only) | P | PCG-105 | FIX: building removal must clean up all references (service radius, walkers). Property test: build/demolish every building type. |
| ISS-003 | Crash when a Kontor's (warehouse's) catchment area extends past the map edge. | ≤1.04, fixed in 1.05 | P | PCG-105 | FIX: clip all radius/area queries to map bounds. Fuzz test with buildings at every map edge. |
| ISS-004 | Hard unit limit ("FIG value") whose overflow crashed the game. The HE removed it. | all classic versions, fixed in HE | P | AU-HE, GS-HE | FIX: no fixed object pools in `sim` (matches the CLAUDE.md "no hardcoded limits" rule). Gameplay unit caps (ISS-061) are separate and data-driven. |
| ISS-005 | Crashes after loading save games and in endless games ("Bugs gab es eine Menge"). A ~1 MB hotfix for "crashes when loading a save / starting a new game" requiring v1.02 is mentioned in the press. Only the PC-Welt reference ("Abstürze verhindern") could be opened. | early German versions | W (reader comments + press mention) | PCG-KT, https://www.pcwelt.de/?p=1135443 | FIX: save loading is a pure parser with validation (round-trip and corpus tests as for every format). Never crash on bad input; report an error instead. |
| ISS-006 | Crashes from trainers, cheats and edited `bgruppen.dat`. "Editing bgruppen.dat … corrupts the game balancing." | all | S (FAQ) | TH-FAQ | FIX: validate data files on load with clear errors. Modding is a goal (PLAN §0.1), so robust data validation is required. |
| ISS-007 | Classic/GOG on modern Windows: "has stopped working" after several days of working fine. No fix posted. | GOG, Win10 | S | GOG-FORUM | N/A as such. Native 64-bit build, no DirectDraw. |
| ISS-008 | HE: crash to desktop after the first campaign cutscene (Win10, GTX 1070). Ubisoft suggested disabling overlays and fullscreen optimisation. | HE | S | https://steamcommunity.com/app/2820110/discussions/1/595147404010872051/ | FIX: video playback must never take the game down. Make cutscenes skippable and fail-soft (skip on decode error). |
| ISS-009 | HE: a fairly advanced multiplayer save crashes on one client while loading. GPU usage spikes to 100% just before the crash. | HE | S | https://steamcommunity.com/app/2820110/discussions/0/595155668457085915/ | FIX: same save loader for SP and MP. Large-save load test in CI. |
| ISS-010 | Unofficial MP (Königsedition + fan patch): crashes on double-click (fixed by a script), random crashes, "had to save about every 5 minutes". | Classic + fan MP | S | https://steamcommunity.com/app/2820110/discussions/0/4333103687421934045 | FIX: see §4. Rebuild autosave should be robust and atomic (write temp file, then rename). |

## 2. Platform, compatibility, resolution and display

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-011 | Overview map (minimap) shown with a white background, or not at all. Cutscene graphics errors on ATI cards. | v1.00, partially fixed in first patch | P + W | GW-P1, GS-P1, GW-KL, TH-TECH | FIX: renderer independent of vendor quirks. Test on AMD/NVIDIA/Intel in CI or manual QA. |
| ISS-012 | Screen flickering when a screensaver or other Windows task activates. The game does not suppress the screensaver, which also caused stutter during long sessions. | v1.00, flicker fixed in first patch | P + S | GW-P1, GW-KL | FIX: inhibit screensaver/sleep while in game (SDL provides this). |
| ISS-013 | Only a fixed set of resolutions. Community patches overwrite one entry of the resolution table inside `AnnoFrame.dll`. Separate widescreen patches exist. The HE added up to 4K with manual UI scaling and borderless mode. | Classic/GOG, fixed in HE | P (HE blog) + W (community patches) | AU-HE, https://github.com/HiPhish/anno-1503-resolution-patcher | FIX: any resolution and aspect ratio, UI scale as a setting, windowed/borderless/fullscreen. Resolution is config, not a table (no-hardcoded-limits rule). |
| ISS-014 | GOG/Classic on Win8/10: heavy lag at anything but the lowest resolution. Workarounds: 16-bit colour, the `/w` window switch, DxWnd with "flip emulation" (also fixes the missing minimap). Under DxWnd it only works after copying the game out of the GOG folder. | GOG on Win8/10 | W (several posters, 2017–2023) | GOG-FORUM | N/A as such. Modern GPU renderer. Target: frame rate independent of resolution. |
| ISS-015 | Wine setups pin the game to one CPU core (`/affinity 1`), force 16-bit colour and need DirectPlay/DirectMusic. This suggests multi-core and colour-depth sensitivity, but no source states the reason. | Classic/GOG under Wine | S (script, no rationale) | https://gist.github.com/fritschy/001139de03e772f40a1067a4882cbac6, https://lutris.net/games/install/4558/view | N/A. The rebuild is native on Linux. The single-core pin is recorded in "Not verified" (R-05). |
| ISS-016 | HE: purple river at 1080p fullscreen, caused by AMD Radeon Image Sharpening. Workaround: disable it. | HE | S (2 posters) | https://steamcommunity.com/app/2820110/discussions/0/4522260579672827419/ | FIX: test water shaders with driver post-processing enabled. Avoid relying on exact palette-index output from the GPU. |
| ISS-017 | HE: window placed on a disconnected or smaller second monitor; confirm button off-screen; cannot revert. HE: after changing display mode, only a taskbar entry and audio remain. Workaround from Ubisoft: delete the config folder or launch with `-windowed`. | HE | W (several threads) | https://steamcommunity.com/app/2820110/discussions/0/84031737849647560/, https://steamcommunity.com/app/2820110/discussions/1/4351116183216453044/ | FIX: validate the saved window position/monitor at startup and fall back to the primary display. Display changes need a timed "keep these settings?" revert. Offer a `--safe-mode` CLI flag that resets video settings. |
| ISS-018 | HE on Steam Deck/Linux: black screen after the Ubisoft launcher. Workaround: kill `Anno1503.exe` so it restarts. Ubisoft: not supported. | HE | W | https://steamcommunity.com/app/2820110/discussions/1/4414173425006098311/ | FIX: native Linux build is a project target. Steam Deck resolution 1280×800 should be a test case. |
| ISS-019 | Some non-German builds report their version as "1.00" or "Unknown". | non-German retail | S (FAQ) | TH-INTRO | FIX: build/version and data-set hash visible in UI and logs (needed for MP compatibility checks anyway). |

## 3. Performance

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-020 | Occasional stutter with larger cities ("bei größeren Städten gelegentliche Ruckler"). | addon, base game | S (review) | GW-ADDON | FIX: performance budget per tick. Benchmark scenario with a maxed-out late-game save. |
| ISS-021 | On huge, developed maps the game "is capable of swallowing 2GHz worth of processing 'power' and still running slowly". | classic | S (FAQ) | TH-INTRO | FIX: sim must scale (spatial indices, no O(n²) scans). Keep a perf regression test. |
| ISS-022 | The multiplayer patch was abandoned partly because a fully built two-player game would have needed ~3 GHz PCs to stay playable. | MP beta 2003/04 | P (publisher statement in press) | GW-MP | FIX: lockstep MP must stay within budget on the slowest peer. Measure ticks/second on a reference late-game save. |
| ISS-023 | Long loading times for new games, saves and autosaves. | classic, also HE | S | TH-FAQ (autosave read-only trick), PCG-KT | FIX: fast saves, autosave in background (snapshot sim state, serialize off-thread). Make the autosave interval and slots configurable. |
| ISS-024 | Stutter on weak PCs. Official advice was to close background apps, go offline and play at 800×600. | v1.00 | S (guide) | GW-KL | N/A (2002 hardware). |

## 4. Multiplayer

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-030 | Multiplayer was promised (box, menu button greyed out) but shipped missing. Patches 1.02 and later did not activate it. | v1.00–1.05, addon | P | GW-TEST, PCG-102, GS-HIST, AU-HE, WIKI-DE | FIX: MP is a first-class goal (net crate). |
| ISS-031 | MP abandoned on 8 Mar 2004 after more than 20 patch versions. Reasons: (1) data transfer problems; (2) hardware demand (~3 GHz for 2 players); (3) incompatibility between different operating systems. The team dissolved in spring 2004. | MP beta | P | GW-MP, GS-HIST, TH-INTRO | FIX: deterministic lockstep: integer/fixed-point sim, no OS-dependent behaviour. The "cross-OS incompatibility" points directly at non-determinism, such as float or x87 state, OS-dependent iteration order or time. This validates the determinism rules in CLAUDE.md. Add CI that runs the same replay on Windows and Linux and compares checksums. |
| ISS-032 | Sunflowers stated that "the high complexity of the game world can lead to so-called 'A-Syncs' during Multiplayer operation". DirectX-based MP did not work, so custom networking was written. | MP beta | P (quoted statement) | TH-INTRO | FIX: per-tick state checksums, desync detection with a dump of the diverging subsystem, resync from snapshot. |
| ISS-033 | Fan MP: building fires spreading cause desyncs ("entirely different development of the game"). Building a fire department early reduced it. | Classic + fan MP | S | https://steamcommunity.com/app/2820110/discussions/0/4333103687421934045 | FIX: disasters (fire, plague, earthquake) must draw only from the sim PRNG and be checksummed. Add a dedicated desync test for spreading fire. |
| ISS-034 | First German release shipped an unsupported `AnnoNetTool.exe` that did not work well. It was removed in 1.04.02. A closed beta with a handful of German players started in late Aug 2003. Plans were 2–8 players with GameSpy matchmaking. | 1.00–1.04 | S (FAQ) | TH-INTRO, TH-FAQ | N/A. Note that matchmaking must not depend on a third-party service that can be shut down. Support direct IP/LAN plus an optional self-hostable lobby. |
| ISS-035 | The lack of MP led to threats of legal action. Players called the later Königsedition re-release with fixes "Softwareabzocke". | 2003/04 | S | TH-INTRO, PCG-KT | N/A (business). |
| ISS-036 | Unofficial fan MP patch (15 Mar 2005, ~123 KB). Features are documented only in its readme. | Classic | P (download page) | https://www.gamepressure.com/download/anno-1503-the-new-world-unofficial-multiplayer-patch/zc1cc5 | Info only. Its readme could be a source on the original net protocol. Open question. |
| ISS-037 | HE MP: runs over Ubisoft Connect (Uplay) with quickmatch and "desync recovery". It has 5 MP scenarios (2 original, 3 new). | HE | P | AU-HE, GS-HE | FIX + RULE: desync recovery is in scope. Scenario/map list is data. |
| ISS-038 | HE MP: no AI/bot players in multiplayer, and no co-op against CPU. Workaround: custom scenarios with NPC players. | HE | W | https://steamcommunity.com/app/2820110/discussions/0/4333103880864337074/, AU-HE (comment) | FIX: AI players allowed in MP. The AI must be deterministic and run inside the sim on every peer. |
| ISS-039 | Multiplayer saves need their original GUID filenames to load. | Classic | S (FAQ) | TH-TECH, TH-FAQ | FIX: save identity stored inside the file, not in the filename. |

## 5. Save games

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-040 | Addon: building and manning a siege tower corrupts the save. After reload, all ships and AI players are gone, and the save cannot be repaired. | addon (version not stated) | S (FAQ) | TH-TECH | FIX: save → load → save must be byte-identical (round-trip test in CI over generated states, including siege towers). Version the save format. RE should find the root cause (likely an unserialized object type). |
| ISS-041 | Scenarios and endless games fail to load fully. Fix: delete `Scenes.cach` and `Islands/InselCache.txt`, which points to stale caches. | v1.00 | S (guide) | GW-KL | FIX: derived caches are keyed by content hash and regenerated automatically, never trusted blindly (derived data lives in `cache/`). |
| ISS-042 | No quicksave in 1503 (F10 does nothing). Saving is disabled in tutorials without any explanation. | Classic + HE | S | https://steamcommunity.com/app/2820110/discussions/0/595158831570614344/ | RULE (QoL): quicksave/quickload hotkeys. Show why saving is disabled. |
| ISS-043 | HE: save reported as successful but the file does not appear (Win11; Ubisoft suspected antivirus or Controlled Folder Access). | HE | S (2 users) | https://steamcommunity.com/app/2820110/discussions/1/4700161870965900817/ | FIX: verify the write, report the real I/O error with the path, and allow a configurable save directory. |
| ISS-044 | The only way to disable autosave is to set `lastgame.sav` read-only. Screenshot numbering restarts per game and overwrites earlier shots; ~30 screenshots per session limit. | Classic | S (FAQ) | TH-TECH | RULE: autosave interval/off in settings. FIX: screenshots use timestamped PNG names. |

## 6. AI behaviour

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-050 | AI builds shipyards on top of the player's ships, leaving them stuck. | v1.00, fixed in first patch | P | GW-P1, GS-P1 | FIX: build placement must check for units/ships on the footprint. |
| ISS-051 | AI rushed newly discovered islands "ohne Rücksicht auf Verluste". Improved in the addon. | base game | S (review) | GW-ADDON | FAITHFUL per data set (base vs addon AI). Expose as AI profile parameters. |
| ISS-052 | AI closes its own city gates (also on reload), so its troops cannot leave. Some AIs never invade because of this. | classic | W (several FAQ contributors) | TH-FAQ | FIX: gate state is part of the AI plan. Reload must not change behaviour (determinism: load must reproduce state exactly). |
| ISS-053 | AI never builds aristocrats. It only trades from its main island. It does nothing at "citizen" level until after the player does. AI preferences change too often, which makes auto-routes with the AI unreliable. | classic | S (FAQ contributors) | TH-FAQ | FAITHFUL by default. RULE: "improved AI" profile. Document the original AI as reference behaviour after RE. |
| ISS-054 | Combat AI is "weak and predictable": it always lands at the same spot, does not move the rally flag, and defenders ignore a lone cannon firing from range. | classic, GOG | W (FAQ + GOG review) | TH-FAQ, TH-INTRO, GOG-STORE | FAITHFUL by default. RULE: improved combat AI. Note the exploit in §12. |
| ISS-055 | AI "forgets to repopulate the hunter's area of influence", so its food supply drops. | classic | S | TH-FAQ | FIX candidate (AI bug, not a rule). Verify via RE. |
| ISS-056 | Diplomacy quirks: AI signs peace and immediately declares war again. Venetians keep trading after you attacked them while your units keep attacking theirs. Tribute is "never paid". | classic | S (FAQ contributors; tribute "reported as bug") | TH-FAQ | FIX tribute if RE confirms it is a bug. Make diplomacy state transitions explicit and logged. |
| ISS-057 | HE single report: a random AI faction "loses its unit limit" and builds huge armies. Unconfirmed by others. | HE/classic | S | https://steamcommunity.com/app/2820110/discussions/0/603035552166454712/ | FIX: unit caps enforced in one sim function for all players. |
| ISS-058 | Free trader bought too few goods. AI build speed in easy endless games was not adapted to the player's progress. | v1.00, changed in first patch | P | GW-P1, GS-P1 | FAITHFUL to the patched values (1.04.02/1.05 data). Rebalance only through rules. |

## 7. Military, combat and pathfinding

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-060 | Pathfinding: scout (Pionier) often stuck on mountains. Scouts and soldiers stuck on rocks or houses (fixed 1.05). Units frozen in place, "a long-standing bug", sometimes unrecoverable. New units stuck inside the barracks. | 1.00–1.04, partly fixed 1.05 | P + W | GW-TEST, GW-ADDON ("teilweise unbrauchbare Wegfindung des Pioniers"), PCG-105, TH-FAQ | FIX: one deterministic grid pathfinder with an "unstick" fallback. Never leave a unit without a valid tile. Spawn tiles are validated. |
| ISS-061 | Unit caps: ground units 100 and ships 40 by default, overridable per scenario. Possible bug letting extra units complete. | classic | S (FAQ) | TH-FAQ | RULE/data: caps come from scenario/config (already planned), not constants. |
| ISS-062 | Ships stuck on island edges (fixed 1.05) or on land. One merchant ship got "stuck inside a warehouse". | ≤1.04 | P + S | PCG-105, TH-FAQ | FIX: water-tile validation for ship positions. Unstick routine. |
| ISS-063 | Ships: improved attack behaviour (1.05). Armed ships fired at floating cargo (fixed 1.05). Ships do not auto-attack archers or towers. Manual attack fires only one cannon. Ship cannons can be loaded only at a shipyard. | ≤1.04 / classic | P + S | PCG-105, TH-FAQ | FIX the 1.05 items. Auto-target behaviour: FAITHFUL plus RULE. Investigate "one cannon" via RE before deciding. |
| ISS-064 | Medics repaired ships (fixed 1.05). Ship repair stops when materials run out and must be restarted manually ("commonly regarded as a bug"). War machines cannot be repaired. | ≤1.04 / classic | P + S | PCG-105, TH-FAQ | FIX: auto-resume repair when materials arrive (RULE if purists object). |
| ISS-065 | Formations: ships drift apart at different speeds. Fleet escort/protect was "removed just prior to the game being finished". Patrol was cut. Protect-object did not work. | classic | S (FAQ) | TH-FAQ | RULE (QoL): restore escort, patrol and matched-speed formations as opt-in features. |
| ISS-066 | Unit balance: pikes/lancers do not beat cavalry. Only crossbowmen/archers gain experience ("I consider this a bug"). Tower range is shorter than cannon/mortar range, which makes towers and walls nearly useless. In-game help stats are mixed up (swordsman "Range: 8"). | classic | S (FAQ contributors) | TH-FAQ | FAITHFUL values from data files. RULE balance preset. FIX the help-text data. |
| ISS-067 | Units cannot cross towers or gates. Only infantry can use wall stairs. Cannot unload several units from a ship at once. Cannot select a single unit type from a group. Military selection is clumsy (a requested fix that did not make release). | classic | S | TH-FAQ, PCG-ED | RULE (QoL): batch unload, type filter in selection. |
| ISS-068 | HE: control groups (Ctrl+1) work only for multiple units. A group of one cannot be created directly. | HE (likely original behaviour) | S | https://steamcommunity.com/app/2820110/discussions/0/4363502064160121499/ | RULE (QoL): allow single-unit groups. |
| ISS-069 | Ship boarding/capturing works only in addon content. Players think the HE removed it. | addon / HE | S | https://steamcommunity.com/app/2820110/discussions/0/595149608084107408/, GW-ADDON, PCG-ADDON | FAITHFUL (data-set dependent). RULE to enable everywhere. Document clearly in the UI. |

## 8. Trade routes, ships and warehouses

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-070 | Automatic trade route with several goods: the ship does not unload anything if one good is already full at the destination. "Considered by most people to be a bug", though one contributor says it was intended. Routes jam when stock builds up. | classic | W (FAQ, several contributors) | TH-FAQ | FAITHFUL default (it may be intended; verify via RE). RULE: per-good partial unload. |
| ISS-071 | Cargo is not unloaded if the destination warehouse is full. | classic | S | TH-FAQ | Same as ISS-070. Show a clear warning in the UI. |
| ISS-072 | Scout could carry too many goods. | v1.00, fixed in first patch | P | GW-P1, GS-P1 | FIX: capacity checks in one place. |
| ISS-073 | Scout's donkey left behind after ship transport. | classic | S | TH-FAQ | FIX: compound units move atomically. |
| ISS-074 | Trade routes target islands, not specific warehouses (ships go to the nearest one). The HE route-point UI confuses players. | HE (likely original) | S | https://steamcommunity.com/app/2820110/discussions/0/595152730766448306/ | RULE (QoL): warehouse-specific stops. |
| ISS-075 | Ship sold, price set to 1, ship bought back for 1 gold. AI buys ships for exactly 4032. | classic | S | TH-FAQ | See §12 (exploit). FIX the price floor. |
| ISS-076 | Pirates keep sending ships after their base is destroyed, and re-settle. | classic | S | TH-FAQ | FAITHFUL unless RE shows a bug. |

## 9. Economy, production, population and taxes

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-080 | Farms/plantations skip fields when harvesting. | ≤1.04, fixed 1.05 | P | PCG-105 | FIX. |
| ISS-081 | Gem mines could not be built directly next to each other. | ≤1.04, changed 1.05 | P | PCG-105 | FAITHFUL to 1.05 behaviour (data). |
| ISS-082 | Weapon production prices, upkeep and output rebalanced. Military costs considered exaggerated at release. Large Shipyard unlock changed (large ship research or 250 merchants). | v1.00 → first patch | P | GW-P1, GS-P1, PCG-ED | FAITHFUL to final patched data. Record which data version we mirror (1.04.02 vs 1.05 vs HE) in `docs/`. |
| ISS-083 | Population of one town limited by an integer overflow above ~65xxx (16-bit). | classic | S (FAQ) | TH-FAQ | FIX: wide integer types in sim (no-hardcoded-limits rule). Verify the 16-bit field in the save format via RE. |
| ISS-084 | Scoreboard "falsified by buffer overflows". | classic | S | TH-FAQ | FIX: wide counters. |
| ISS-085 | Well "drought" bug: wells lose their function after a drought (locally, recovers over time). | classic | S | TH-FAQ | Verify via RE. FIX if it is a bug. |
| ISS-086 | Whaler shows ~50% efficiency but works at 100% (display bug). Productivity display caps at 100% although real effectiveness can be 250–333%. Display is not expressive in the 90–100% range. | classic | S | TH-FAQ | FIX the display (UI only; sim unchanged). |
| ISS-087 | Upgrading to a big mine sometimes yields no production. Mines never deplete although the readme mentions deeper mines. | classic | S | TH-FAQ | Verify via RE. FIX the production bug. Depletion: RULE (off by default). |
| ISS-088 | False shortage messages after downgrading and re-upgrading houses. | classic | S | TH-FAQ | FIX (notification state). |
| ISS-089 | Aristocrats: pathfinding "bug or at least strange behaviour" — they starve walking endlessly between public buildings, and their houses collapse ("a known problem which lots of people are facing"). | classic | S (FAQ contributor) | TH-FAQ | Investigate via RE. Likely FIX (needs-satisfaction search). Keep the original as an opt-in if it is intended. |
| ISS-090 | Citizens sometimes stop going to the next building they need. Market carts cannot pick up goods unless the road meets the building's arrow. Fertility indicator ignores space used by another farm. | classic | S | TH-FAQ | FIX the indicator. Road-arrow rule: FAITHFUL plus clearer UI feedback. |
| ISS-091 | Gallows, courthouse ("not implemented" research), rewards and statues have no effect in the release version. The addon added gallows and courthouse. | 1.0x base | S (FAQ) | TH-FAQ, WIKI-DE | FAITHFUL per data set. Document it. |
| ISS-092 | Market stall income replaces taxes. Finances are opaque until the addon's statistics screen. The tavern and school did not show income or costs (added in first patch). | base | P + S | GW-P1, PCG-ADDON, WIKI-DE | RULE (QoL): full statistics in base mode too. |
| ISS-093 | Cobbled roads were believed to be faster but are not; a developer acknowledged a bug to be fixed. | classic | S | TH-FAQ | Verify in data/RE. FAITHFUL plus RULE. |
| ISS-094 | Natural disasters cannot be influenced by the player. | addon | S (review) | GW-ADDON | FAITHFUL. RULE: disaster frequency/off. |

## 10. UI and quality-of-life gaps

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-100 | Right-hand menu not intuitive. Many unclear icons. Too many clicks (e.g. farm menu). Buildings hard to select (the neighbouring building gets picked). | base | W | GW-TEST, PCG-FB | RULE (QoL): tooltips, hotkeys, better hit-testing. FIX hit-testing (picking by sprite mask). |
| ISS-101 | Difficulty too high and not beginner-friendly (lowest reader scores). Campaign scenarios 1/2 eased by patch. The scenario after returning in debt is still reported as brutal in the HE. | base, HE | P + W | GW-P1, PCG-FB, WIKI-DE, https://steamcommunity.com/app/2820110/discussions/0/506216918922045157/ | FAITHFUL scenarios. RULE: difficulty modifiers (start money, AI aggression). |
| ISS-102 | Lengthy exploration with the scout. | base | S | PCG-FB | RULE (QoL). |
| ISS-103 | Tutorial UI makes it easy to declare war instead of signing a trade agreement. | classic | S | TH-FAQ | FIX: confirmation for war declarations. |
| ISS-104 | Ambient sounds muted at non-normal game speeds (intended). HE: ambient animal sounds too loud or repeated ("escalating"); a community mod exists. | classic, HE | S + S | TH-TECH, https://github.com/Taludas/Anno-1503-HE-Sound-Fix, https://steamcommunity.com/app/2820110/discussions/1/4638240577738023122/ | RULE: separate volume sliders and per-sound cooldown. |
| ISS-105 | Moors have no music. Cause: a wrong path entry in `data/music.dat`. | classic | S (FAQ) | TH-TECH, TH-FAQ | FIX at load time (data-fix layer, documented). |
| ISS-106 | Music played only from the CD. Copying music/video to disk needs ≥ patch 1.01. | v1.00 | S | GW-KL, TH-TECH | N/A (assets read from the install). |
| ISS-107 | HE: optional QoL already added: modern cursors, smooth mouse scrolling, multi-placement of residences, hotkey overview. | HE | P | AU-HE | Treat as a minimum QoL baseline (RULE where it changes gameplay). |
| ISS-108 | HE addon scenarios: narrator voice silent because `MaxSound.dll` hands IMA-ADPCM WAVs to Windows undecoded. Community fix: convert to PCM. HE cutscenes without sound are also reported. | HE | S (detailed root cause) | https://steamcommunity.com/app/2820110/discussions/1/586188051673259602/ | FIX: our audio decoder must support IMA ADPCM WAV (add it to the format corpus test). |
| ISS-109 | Ctrl auto-alignment of buildings reworked. Hotkey N added to jump to the next scout. | first patch | P | GW-P1, GS-P1 | FAITHFUL to patched behaviour. |

## 11. Scenario and campaign bugs

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-110 | Mission could not finish if Peles' warehouse/main market was destroyed before other objectives. The "flee with cannon" mission may not finish in some unpatched or non-German versions. | release versions | S (FAQ) | TH-FAQ | FIX: objective evaluation must be order-independent. Add a scenario test harness. |
| ISS-111 | Losing both mortars in "Toguldur's Stone" makes it unwinnable (archers cannot attack rocks). "Barbarossa's Throne" cannot be restarted without replaying Nova Fora. | classic | S | TH-FAQ | RULE (QoL): soft-lock detection and a warning; per-scenario restart. |
| ISS-112 | "Citizen" endless level missing in the unpatched game. Trapper buildable in Helter-Skelter only in 1.00. | 1.00 vs patched | S | TH-FAQ | Data-version awareness (ISS-082). |
| ISS-113 | Map editor German-only; HE did not update it ("issues with the source code"). | classic, HE | P | AU-HE, GS-HE | FIX: editor is in scope later (moddability). |
| ISS-114 | Some scenario files appear missing (only text files present). | classic | S | TH-FAQ | Info. Our corpus tests will reveal what exists. |

## 12. Exploits (balance notes only)

| ID | Issue | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|
| ISS-120 | Ship price set to 1 and bought back. | S | TH-FAQ | FIX (price floor). Treat as a bug. |
| ISS-121 | A lone cannon firing from range draws no response from defenders. | W | GOG-STORE, TH-FAQ | RULE (improved AI). Faithful by default. |
| ISS-122 | Operating costs deducted in small steps; rounding errors favour the player. | S | TH-FAQ | FAITHFUL (reproduce the original rounding exactly, documented after RE). Note it in the spec. |
| ISS-123 | Save-scum island exploration (reload to pick the best island). | S | TH-FAQ | N/A (player choice). Optional ironman RULE. |
| ISS-124 | Small exploitable bugs listed in PC Games' tips section (not enumerated in the page opened). | S | PCG-ED | Open: find the original tips article. |

## 13. History Edition regressions and limitations

| ID | Issue | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|
| ISS-130 | Three soundtrack tracks removed for licensing. | P | AU-HE | N/A (we read the user's files). Do not assume all tracks exist. |
| ISS-131 | Ubisoft Connect required. Login asked again after every restart. A Steam purchase without a Ubisoft account is a complaint. | S | https://steamcommunity.com/app/2820110/discussions/0/691997670669255879/ | FIX: no account or online requirement for SP or LAN. |
| ISS-132 | History Editions are "sunset": no further patches or bugfixes (community statement, not official). | S | same thread as ISS-131 | Info: the remaining HE issues will not be fixed upstream. |
| ISS-133 | No Polish (no localization exists). Russian requested. | P (developer comment) | AU-HE, Steam PL thread list | RULE: localization as data (Texte.dat plus overrides). |
| ISS-134 | HE DLL mods for Classic do not work. Classic `.dll` hooks are incompatible with the 64-bit port. | W | (see `community-landscape.md`, sources there) | Data-driven modding, no DLL hooks. |
| ISS-135 | Coastline graphics glitch fixed, but kept as an optional "original look". | P | AU-HE, GS-HE | RULE: keep faithful-glitch toggles as cosmetics only. |

## 14. Copy protection and distribution

| ID | Issue | Versions | Evidence | Sources | Rebuild implication |
|---|---|---|---|---|---|
| ISS-140 | CD 2 unreadable on some drives because of an error duplicating the master (Sunflowers confirmed, discs exchanged). Workaround: toggle DMA. The developer cited "~1%" of customers with serious problems (drivers plus the CD track error). | v1.00 German | P + W | GW-P1, GW-KL, GS-INT, TH-TECH | N/A. The rebuild must accept any install layout (CD copy, GOG, HE, Steam). |
| ISS-141 | Game needs the CD to run. | retail | S | TH-TECH | N/A. No disc checks. |
| ISS-142 | Copy protection type (SafeDisc etc.) for 1503: **not confirmed** by any opened source. | — | — | — | See "Not verified" (R-04). |

---

## Not verified / rumours

- **R-01:** Search-engine summaries attributed to "WinFuture" a first *beta* patch (AI build speed, minimap, load stalls) and a 1 MB hotfix for save-load crashes. The WinFuture URLs redirect to an index page, so this is **not verified**. PC-Welt confirms only that "a hotfix to prevent crashes" exists for 1503.
- **R-02:** That patch "1.04" is a distinct public German patch. Only "1.04.02 (12 March 2003)" for non-German versions is documented (TH-INTRO). German 1.03/1.04 changelogs were not found. GameStar says "four patches" fixed many flaws (GS-HIST).
- **R-03:** Desyncs persist in the HE multiplayer despite "desync recovery". This is stated in our own `community-landscape.md`, but none of the threads opened this time confirm it. The only HE MP experience report says "Ubisoft connect works well".
- **R-04:** SafeDisc / StarForce / SecuROM on 1503 retail. One ComputerBase forum remark (seen only in a search snippet, not opened) says the copy protection drew criticism. Not verified.
- **R-05:** Multi-core CPU problems (stutter or crashes on multi-core) and DEP crashes. These are plausible for a 2002 MSVC6 DirectDraw title, and a Wine script pins one core, but no opened source states either for 1503.
- **R-06:** Resolution limit of about 1600×900 in the classic version. This is stated in `community-landscape.md`, but the HiPhish README opened here uses 1600×900 only as an example target. A widescreen fix v2 (GamePressure listing, 1 Mar 2024) exists, but its page was not opened.
- **R-07:** "Venetians teleport goods between warehouse and ship" (FAQ contributor speculation).
- **R-08:** "After killing all natives, all units disappeared but still cost upkeep" (one FAQ report, unverified by the FAQ author).
- **R-09:** HE "control groups" and "auto-route UI" complaints may be original behaviour rather than HE regressions. No comparison with classic was found.

## Follow-ups

1. Manual pass over AnnoZone forum (German bug threads, MP patch history, widescreen patch thread 16090) and Tim Howgego's FAQ section 10.1 bug list (says "more than 70 known or suspected bugs").
2. Read the fan MP patch readme (ISS-036) for hints on the original net protocol.
3. RE tasks implied above (save corruption ISS-040, population width ISS-083, trade unload rule ISS-070, aristocrat routing ISS-089, rounding ISS-122). Add them to `docs/open-questions.md` when they are picked up.
4. Decide and document which data version (1.04.02 / 1.05 / HE) is the "faithful" baseline (ISS-082).
