# ANNO 1503 – Community-Landschaft (Stand 16.10.2026)

Web-Recherche zu Mods, Tools, Codebases und Reverse-Engineering-Wissen. Repos ließen sich teils nicht direkt öffnen; wo nur Archiv- oder Forenbelege existieren, ist das vermerkt.

## Kernaussagen
- Es gibt **keine brauchbare Open-Source-Reimplementierung** von ANNO 1503. Das Feld ist frei.
- Wertvollstes Vorwissen: **Amsel** (millimarg) – XML-Formatspezifikationen der 1503-Binärdateien. Repo inzwischen nicht mehr öffentlich, Docs-Bundle aber auf AnnoZone (Datei 9287). → Autor kontaktieren.
- Zweitwertvollstes: **DatEditor/Muto1503** (Azukee) – Anlagen.dat-Parser aus IDA-Pseudocode + MinHook-DLL, die die Originalfunktion `parseAnlageReal` hookt. GitHub-Repo weg, Kopie bei Software Heritage.
- Original-Multiplayer wurde **nie fertig** (Patch am 08.03.2004 eingestellt). Nur die History Edition (Ubisoft Mainz, 25.06.2020, Steam ab 22.04.2024) hat MP über Ubisoft Connect mit „desync recovery“ – Desyncs bleiben laut Spielern.
- Netcode-Vorbild: einzige öffentliche Paketanalyse ist für **Anno 1602** (DirectPlay, Codeberg).

## 1. Code-Projekte zu ANNO 1503
| Projekt | Inhalt | Stand / Lizenz | Relevanz |
|---|---|---|---|
| [Azukee/Open1503](https://github.com/Azukee/Open1503) | C++-Stub: OpenStartup, OpenFrame (AnnoFrame.dll), OpenDisplay (DDraw7-Blitter) | 22 Commits Dez 2022–Jan 2023, keine Lizenz; von GitHub entfernt, Archiv bei Software Heritage (2024-09-30) | Gering – nur Hinweis auf DLL-Aufteilung des Originals |
| [Azukee/DatEditor](https://github.com/Azukee/DatEditor) („Muto1503“) | C#-Reader/Writer Anlagen.dat (inkl. ADDON-Chunks), Klassen nach Originalnamen (TAnno, TAnlage*, THaus*), MinHook-Injector-DLL | 39 Commits bis 05/2024; entfernt, Archiv bei Software Heritage (2026-01-21) | **Hoch** – Datenlayout + Hooking-Muster |
| Amsel – gitlab.com/millimarg/amsel → gitlab.com/anno-urbis/amsel ([Thread](https://www.annozone.de/forum/thread/17729-millimargs-modding-tools/)) | Python-Lib: Formate als XML-Spec, Laden/Export/Rebuild, Mods als XML-Changesets (.a2x.gz); laut Autor kompatibel mit allen Versionen inkl. HE | Seit 03/2024; Repo + Docs aktuell hinter GitLab-Login. [Docs-Bundle](https://www.annozone.de/file-download/9287/) | **Sehr hoch** |
| A₂X Graphics Decoder ([AnnoPool 1090](https://www.annozone.de/annopool/file/1090-a%E2%82%82x-anno-1503-graphics-decoder/)) | Extrahiert alle Grafiken via FigurenHeader.dat; HE: Figuren.dat/.addon | GPLv3, v1.0.0 03/2024 | Asset-Pipeline |
| A₂E Explorer / Legacy ([AnnoPool 1092](https://www.annozone.de/annopool/file/1092-a%E2%82%82e-legacy-anno-1503-file-explorer/)) | GUI-Browser für Spieldateien | 2024 | Formatexploration |
| [HiPhish/anno-1503-resolution-patcher](https://github.com/HiPhish/anno-1503-resolution-patcher) | Patcht Auflösungstabelle in AnnoFrame.dll per Byte-Pattern, passt Texte.dat an | MIT-0, Python | Zeigt hardcodierte Auflösungen (Grenze ca. 1600×900) |
| [OrderLightXander/anno-1503-easy-player-boost](https://github.com/OrderLightXander/anno-1503-easy-player-boost) | Binärpatch Anlagen.dat/.classic/.addon + BGruppen.dat (Produktion, Radius, Wachstum) | 2026, ungetestet, keine Lizenz | Feldpositionen für HE |
| [Taludas/Anno-1503-HE-Sound-Fix](https://github.com/Taludas/Anno-1503-HE-Sound-Fix) | Leisere Ambient-Sounds via `Languages/`-Override | 12/2024 | Gering |
| [Wine-Gist (fritschy)](https://gist.github.com/fritschy/001139de03e772f40a1067a4882cbac6) | Wine: winxp, directplay, directmusic, 16-bit, 1 CPU-Kern | 2019 | Laufzeitabhängigkeiten |
| [igor-240340/IsometricAnno](https://github.com/igor-240340/IsometricAnno) | Unity-Spielzeugklon | 2024 | Keine |
| [anno1491ad (SourceForge)](https://sourceforge.net/projects/anno1491ad/) | Java-Semi-Klon | tot seit 2013 | Keine |

## 2. Binär-Tools & Mods (AnnoZone/AnnoPool)
- drkohler: [AnlagenProjekt](https://www.annozone.de/annopool/file/1076-anlagenprojekt/), [MapEditor 64-bit](https://www.annozone.de/annopool/file/1075-anno1503-mapeditor/), [GFX Extractor](https://www.annozone.de/annopool/file/1065-gfx-extractor-64bit-v2/) (Felder.tex), „Bodenreform“-Texturen – nur Binaries.
- millimarg (älter): Figuren-Decoder, zeidec, XML-Pack/Unpack (2015), [Widescreen-Patch](https://www.annozone.de/forum/thread/16090-widescreen-patch/).
- AD1503 „Venedig“-Mod (Admiral Drake, 2010/11; HE-Testbuild 2020 unfertig) – deaktiviert KI durch Löschen der CompLogic-DLLs.
- Weitere: Dickerbaer CG-Mod, Skriptbuch 2.0, AnnoNetTool, Tex-Encoder, Ship/Soldier Tweaker. Übersicht: [Tim Howgego](https://timhowgego.wordpress.com/anno_1503/editing/), [AnnoPool 1503-Tools](https://www.annozone.de/annopool/category-file-list/77-anno-1503-tools/) (46 Dateien).
- Laufender Thread: [[WIP] Dokumentation und Modding-System](https://www.annozone.de/forum/thread/16658-wip-dokumentation-und-modding-system-fuer-anno-1503/) (letzter Beitrag 09/2026, nicht vollständig gelesen).

## 3. Dokumentiertes Technikwissen
- **Dateien 1503:** Figuren.dat/FigurenHeader.dat/FigurenNNN.dat, Figuren.addon (HE), Anlagen.dat/.classic/.addon, BGruppen.dat (Text, `Id`-Blöcke), Texte.dat (Text), Felder.tex, .zei (Fonts), .sze (Szenarien), .cty, Scenes.cach, .bik (Bink). Die Formate .bsh/.cod/.gad/.szs sind **1602**, nicht 1503 belegt.
- **Figuren-Header:** Höhe, Breite, Palette-ID, Tag, Mittelpunkt, Offset, Länge; erste 4 Bytes Big-Endian; Paletten „PCX“ ohne Header, 4-Byte-Einträge; Schatten als BSH_ALPHA.
- **Szenario-Header:** `ANNOADDON1` vs. `ANNO` schaltet Addon-Inhalte.
- **Engine:** DirectDraw 7 (DX 8.1), Bink, GameSpy (abgeschaltet). Registry `HKCU\Software\Max-Design\Anno1503`, `MULTIPLAYER=1` blendet MP-Button ein, `SCREENMODE` 0–4.
- **HE:** 64-bit-Port, bis 4K, UI-Skalierung, FIG-Einheitenlimit behoben, Editor nicht aktualisiert („issues with the source code“); DLL-Mods für Classic laufen dort nicht.

## 4. Warum der Original-MP scheiterte ([Worthplaying 2004](https://worthplaying.com/article/2004/3/8/news/15699-anno-1503-multiplayer-patch-gets-canned/), [Gameswelt](https://www.gameswelt.de/anno-1503-aufbruch-in-eine-neue-welt/news/arbeiten-am-patch-eingestellt-75721))
- >20 Patch-Builds; Asyncs nicht immer erkannt → Auto-Resync griff nicht.
- DirectPlay für 6–8 Spieler ungeeignet → Eigenlösung (laut Worthplaying).
- <15 fps → mehr Sync-Fehler; Late-Game 2 Spieler brauchte ~3 GHz.
- Mischbetrieb Win98/ME/2000/XP → Desyncs.
- Spielerberichte (HE): Desync bei Feuerausbreitung, großflächigem Roden/Pflanzen, Crash AnnoFrame.dll c0000005, Doppelklick-Crash.
- Implikation: deterministischer Lockstep mit State-Hashing ist Pflicht; Brandsimulation und Massenaktionen sind bekannte Nichtdeterminismus-Hotspots.

## 5. Anno-1602-Projekte (übertragbare Muster)
- [siredmar/mdcii-engine](https://github.com/siredmar/mdcii-engine) / [roybaer/mdcii-engine](https://github.com/roybaer/mdcii-engine) – C++/SDL2, GPL-2.0+, BSH/COD/GAM-Tools.
- [cmfcmf/Anno2018](https://github.com/cmfcmf/Anno2018) – TypeScript, Browser, 107★.
- [stwe/Benno4j](https://github.com/stwe/Benno4j), [stwe/MDCII](https://github.com/stwe/MDCII).
- [Green-Sky/anno16_docs](https://github.com/Green-Sky/anno16_docs) – Format-Wissensbasis.
- [davidmc971/Anno1602-network-analysis](https://codeberg.org/davidmc971/Anno1602-network-analysis-and-reverse-engineering) – MIT, DirectPlay-Pakete, Rust-Proxy, pcaps (aktiv bis 07/2026).

## 6. rea (morluto/rea) – Einordnung
- MCP-Server/CLI, der Ghidra/IDA/Hopper/Rizin für Agenten ansteuert; kein eigener Decompiler. MIT. Schnell wachsend, Breaking Changes in Minor-Versionen → Version pinnen.
- **Windows-Host:** Ghidra-Pfad akzeptiert nur 64-bit PE, lehnt 32-bit x86 und DLLs ab → für 1503 (32-bit, DLL-lastig) **Linux/macOS-Host** nutzen.
- Showcase DX-Ball (1996): 55 Funktionen rekonstruiert, nicht spielbar. Taugt für Einzelfunktionen mit Differential-Tests, nicht für Komplett-Rebuild.
- Achtung: zwei npm-Pakete (`rea-agents` vs. `@morluto/rea` 0.2.1) – offizielles prüfen.

## 7. Lehren aus vergleichbaren Projekten
- **OpenRCT2:** inkrementell, ruft anfangs Original-EXE für fehlende Funktionen → immer spielbar.
- **Julius/Augustus:** treuer Kern (savegame-kompatibel) getrennt von QoL-Fork.
- **Return to the Roots:** C++-Rewrite mit Original-DATA/GFX – nächstes Analogon.
- **OpenTTD:** Command-basiertes Lockstep, RNG-State als Checksumme, Desync-Logs/Replay/Bisect ab Tag 1.
- **openage:** Warnung vor Over-Engineering vor Spielbarkeit.
- Assets: Original-Installation (GOG/HE) voraussetzen, nichts ausliefern.

## Offene Punkte
- millimarg wegen Amsel-Specs kontaktieren; Docs-Bundle 9287 laden.
- DatEditor/Open1503 aus Software Heritage ziehen.
- WIP-Thread 16658 vollständig lesen.
