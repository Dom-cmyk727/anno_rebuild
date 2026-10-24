# ANNO 1503 – Analyse der Spieldateien (GOG 2.0.0.5)

Stand 16.10.2026. **Zweite Durchsicht 22.10.2026:** [`game-files-resurvey.md`](game-files-resurvey.md) (Szenario-Skriptsprache, KI-Profile, Regel-Chunks, Online-Hilfe). Quelle: `setup_anno_1503_2.0.0.5.exe` (GOG-ID 1438074791, Inno Setup 5.5.0, nur Englisch), entpackt mit innoextract 1.8 nach `~/Dev/anno1503/game/app`. 6.040 Dateien, ca. 1,5 GB.

**Version-Gate:** Alle Spiel-DLLs und die EXE haben den PE-Zeitstempel **2006-07-21**, Ausnahmen sind `FileGame.dll` und `DrvMgt.dll` (Januar 2004). Es ist also der Königsedition-Stand, 32-bit x86 (Machine 0x14c), gebaut mit MSVC 6 (Linker 6.0). Die History Edition (64-bit) hat andere Binaries. Alle Aussagen hier gelten nur für diese Version.

## 1. Modulkarte (Binaries)
Architektur: COM-artige Module. Jede DLL exportiert nur `Create…Instance` + `Get…DllGuid`. Die Kommunikation läuft über Interface-Vtables (`iAnnoFrame`, `iAnnoNetClient`, …). Fast alle DLLs importieren ausschließlich KERNEL32, der Kern ist also plattformneutraler Code.

| Modul | Größe | Exports | Rolle (aus Exports/Imports abgeleitet) |
|---|---|---|---|
| 1503Startup.exe | 94 KB | – | Launcher, lädt AnnoDisplay + AnnoFrame |
| AnnoFrame.dll | 684 KB | AnnoFrame, FrameGame, Cursor, Event, Music, Statusbar, Highscore, **NetClient, NetServer** | Hauptschleife/UI, **Netzwerk (WS2_32)**, Bink, AVI |
| AnnoDisplay.dll | 344 KB | CreateDisplayInstance | DirectDraw-Renderer (DDRAW: 1 Import), 220 RTTI-Templates für Sprite-Blitter (`tBrush2Screen…`, 555/565, Alpha, Schatten, Z) |
| AnnoWelt.dll | 967 KB | AnnoWelt, GetAnnoInselFileData, **RepairSavegame** | Welt/Inseln – vermutlich Simulationskern |
| AnnoScene.dll | 291 KB | AnnoScene, SaveGame, SceneCache, **AnnoBigBrother** | Szenario/Savegame, Statistik („BIGBROTHER“-Chunk) |
| AnnoGame.dll | 184 KB | **CreateAnnoServerInstance** | Spielserver/Regelwerk (nur KERNEL32) |
| CompLogic.dll / CompLogicClassic.dll | 455/438 KB | CompCreateInstance | KI-Gegner, Addon- vs. Classic-Variante |
| View.dll | 758 KB | ViewAnnoCreateInstance, Bink/Startup-Frames, Waren-Tooltips | View-/UI-Layer |
| Max_Sound.dll | 70 KB | MaxSoundCreateInstance | Sound, vermutlich DirectSound via COM (ole32), nicht verifiziert |
| FileGame.dll | 369 KB | CreateFileGame | **Debug-Build**, PDB-Pfad `E:\Sources\Anno1503\AddOn\Projects\FileGame\Debug\FileGame.pdb` |
| DrvMgt.dll | 24 KB | Setup/Remove | Treiber-/Kopierschutzhelfer (vermutlich) |
| binkw32.dll | 359 KB | RAD Bink 2002 | Video (Drittanbieter) |

Bemerkenswert:
- **Netzwerk ist eigenes UDP über Winsock** (`socket, bind, sendto, recvfrom, select, connect, setsockopt`), kein DirectPlay-Import. Die Community-Annahme „DirectPlay“ ist für diese Version widerlegt. GameSpy-Arcade-Registry-String vorhanden.
- Desync-Erkennung existiert: String „Async aufgetreten! Spiel mu[ss]…“ in AnnoFrame.dll.
- Internes Client/Server-Modell (`AnnoGame` = Server, `AnnoFrame` = NetClient/NetServer). Ob das auch im Einzelspieler genutzt wird, ist zu prüfen. Das wäre ein guter Ansatzpunkt für die Trennung von Simulation und Darstellung.
- RTTI-Klassennamen vorhanden (AnnoFrame 27 Interfaces, View 15, AnnoDisplay 220) → Ghidra-Klassenwiederherstellung möglich. Export: `analysis/pe_symbols.json`.
- MFC42D/MFCO42D/MSVCRTD (Debug-Runtimes) werden mitgeliefert, aber von keiner Spiel-DLL importiert.

## 2. Dateiinventar
| Typ | Anzahl | Größe | Ort | Format |
|---|---|---|---|---|
| .wav | 1.844 | 518 MB | Speech, Samples, Music, Scenes | RIFF; Musik ADPCM (fmt 0x0002) |
| Figuren*.dat | 16 | ~280 MB | Root | Rohe RLE-Sprite-Daten ohne Header, Index in FigurenHeader.dat |
| Anlagen.dat | 1 | 205 MB | Root | Chunk-Format: COLORS (440 ENTRY), ANLAGEN (921 ENTRY), 3× TEXTURES |
| .bik | 41 | 296 MB | Videos, Scenes | Bink (`BIKi`); `Data/Debug.dat` ist ebenfalls ein Bink-Video |
| .sze | 50 | 57 MB | Scenes/* | Szenarien, Chunk-Format, Root `ANNO` (23) oder `ANNOADDON1` (27) |
| .scp | 465 | 47 MB | Scenes/Islands/<Klima> | Inselvorlagen, Chunk-Format + Trailing-Bytes (444 Dateien) |
| .tex/.inc | 1.214/1.213 | 52 MB | ToolGfx, Textures | Chunk-Format: TEXTURE/COLOR bzw. ACCESS |
| .zei | 11 | 0,5 MB | ToolGfx, Textures | Fonts, Root `CHARSET` (HEAD, BODY) |
| Data/*.dat | 12 | klein | Data | **Klartext-INI-artig**: BGruppen (Waren/Preise), Wachstum, Felder, music, statusbar, wegspeed, Highmap, font … |
| Texte.dat | 1 | 46 KB | Root | Klartext, `[ANLAGEN]` `id:Text` |
| .ddf/.jpg | 77/113 | | Scenes/0201SingleAddOn | Bildbeschreibungen (`<IMAGES>`-Tags) + Briefing-Bilder |
| Manual.pdf, help/*.htm | | | | Handbuch, Online-Hilfe |

Szenarien: 0010SingleEndlos, 0011SingleEndlosAddon, 0100Kampagne01, 0200Single, 0201SingleAddOn, **0500MultiEndlos, 0700Multi**, Tutorial, Islands (Klimazonen Polar, Nord, Steppe, Praerie, Dschungel, …).

## 3. Container-Format (validiert)
Max-Design-Chunk: **12 Byte Name (NUL-terminiert, Rest kann Müll sein) + uint32 LE Payload-Größe + Payload**, hierarchisch verschachtelt. Parser: `tools/chunks.py` (generisch, rekursiv; Container = Payload parst exakt in Unterchunks).

Validierung:
- .sze 50/50, .tex 1214/1214, .inc 1213/1213, .zei 11/11, Anlagen.dat 1/1 parsen vollständig.
- .scp 465/465 parsen bis Ende des Root-Chunks; danach Zusatzdaten (Format offen).
- FigurenHeader.dat parst nicht generisch: ENTRY enthält Key-Value-Records (12-Byte-Key + uint32, z. B. `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`) mit abweichendem Header → Amsel-Spec nötig.

Szenario-Chunks (Auszug): SCENE/{HEAD, MAIN, GUID, ALIASNAME, WARETYPE, SKTIMER, MONOPOL2, LIMITS, COMPPROFIL2, ROHSTOFF, SCHATZ, ERZE, BIGBROTHER, AUFTRAGTEXT, SPIELER, INSEL5, INSELLNK}.
Insel-Chunks: INSEL4/5, INFO, HEIGHT, GFXID, GELAENDE, PROD2, FRUCHTBAR, BERGWERK2, OCCUR, ANBAU, DISCOVER, KONTORID, STAEDTE, LOCKED. Versionierte Chunknamen (INSEL4→5, PROD2, MONOPOL2) deuten auf Format-Evolution, die ein Loader abdecken muss.

## 4. Konsequenzen für den Rebuild
1. Asset-Loader zuerst: Chunk-Parser steht. Als Nächstes Figuren-Sprites (RLE) und Paletten, danach Szenario/Insel-Semantik.
2. Spieldaten sind teilweise Klartext (Waren, Wachstum, Wege-Geschwindigkeit) → sofort als Balancing-Quelle nutzbar.
3. Multiplayer wird voraussichtlich neu aufgesetzt, nicht originalgetreu nachgebaut. Das RE von AnnoFrame.dll (NetClient/NetServer, UDP) und dem Async-Pfad liefert dafür nur Input: welche Spieleraktionen übertragen werden (Kommando-Katalog), wie ein Async erkannt wird und welche Systeme nichtdeterministisch sind. Ideen aus der 1602-Community:
   - **Methodik** aus der [1602-Netzwerkanalyse](https://codeberg.org/davidmc971/Anno1602-network-analysis-and-reverse-engineering) (MIT): Paketmitschnitte (pcap), ein Proxy zwischen den Clients (dort in Rust), reproduzierbare Testaufbauten mit Netzwerk-Namespaces bzw. Docker/VNC. Lässt sich auf das 1503-UDP-Protokoll übertragen.
   - **Nachrichtenmodell:** Die dort dokumentierten Pakettypen (Fokus, Pause, Schiff bewegen, Schiff verkaufen) deuten darauf hin, dass Max Design schon bei 1602 einzelne Spieleraktionen überträgt und nicht den Spielzustand. Das stützt einen kommandobasierten Lockstep-Neuentwurf. Die Protokolldetails selbst sind nicht übertragbar (1602: DirectPlay, 1503: eigenes UDP).
   - **Grenzen:** Die übrigen 1602-Rebuilds (mdcii-engine, Anno2018, Benno4j) haben keinen Multiplayer. Für das Netzdesign bleibt OpenTTD die Hauptvorlage: Kommandos auf festen Ticks, RNG-Zustand als Prüfsumme, Desync-Logs und Replay ab Tag 1.
4. **Analyse-Basis 32-bit, Rebuild 64-bit.** Grundlage ist bewusst die Originalversion (GOG, 32-bit), nicht die History Edition. Der Simulationskern liegt vermutlich in AnnoWelt/AnnoGame/AnnoScene (nur KERNEL32-Imports) → Haupt-Ziele für Ghidra/rea. Der Rebuild wird nativ als 64-bit-Anwendung für Windows und Linux gebaut. Folgen:
   - **Kein Hybrid-Betrieb im Zielprozess:** Ein 64-bit-Prozess kann die 32-bit-Original-DLLs nicht laden. Der OpenRCT2-Weg (unfertige Funktionen ins Original-Binary aufrufen) ginge nur in einer separaten 32-bit-Testumgebung. Validierung läuft stattdessen über Differential-Tests: gleiche Eingaben an Original-Funktion (32-bit, Windows/Wine) und Neuimplementierung, Ergebnisse vergleichen.
   - **Dateiformate explizit serialisieren:** Die Original-Formate enthalten 32-bit-Felder und vermutlich Struct-Dumps. Feste Breiten (`uint32_t` usw.) verwenden, kein `memcpy` ganzer Structs, keine Zeiger- oder `long`-Größen im Format.
   - **Rechenergebnisse können abweichen:** MSVC 6 rechnet mit x87-FPU (80-bit intern), x64 mit SSE2. Gleitkomma-Formeln liefern dann leicht andere Werte. Für Determinismus im Multiplayer und Treue zum Original Simulationslogik auf Ganzzahl-/Festkomma-Arithmetik prüfen und wo nötig umstellen.
5. Laufzeittests brauchen x86-Windows (Apple Silicon kann das nicht nativ).

## Lokale Struktur
`~/Dev/anno1503/` – `game/` (entpackt, nicht versionieren), `tools/chunks.py`, `analysis/pe_symbols.json`, `.gitignore` (game/, *.exe, *.bin).

## Corpus notes from the 2026-10-24 snapshot

Taken from [`analysis-2026-10-24.md`](analysis-2026-10-24.md) (an independent read-only analysis of the owner's two installs; SHA-256 over both trees):

- The game folder holds **5,951 files** (1,514,836,147 bytes). The 6,040 of the first survey counted the full installer's extras (DirectX, bonus files) that the game folder never receives. Every per-type count the specs use (1,214 `.tex`, 1,213 `.inc`, 11 `.zei`, 50 `.sze`, 465 `.scp`, 16 Figuren volumes, `Anlagen.dat` 205,470,957 bytes) matches. Tests must not assert the 6,040 total.
- `Speech/` has **1,277** numbered files, not 1,276.
- `ToolGfx/help8.tex` has no `.inc` mate (the 1,214 against 1,213); `ToolGfx/Questlog_Campaign0101.tex` and `questlog_campaign0101.inc` differ only in case, so a pairing by exact stem on Linux misses them while Windows does not. Corpus tests must not require a mate for every `.tex`.
- The leftovers `Scenes/Islands/Nord/N_256x256_0010_1B_Mongolen.scp.bug` (a raw `INSEL4` body behind 16 zero bytes) and `ToolGfx/HelpNormal.zei.ok` stay out of the corpora by the extension filters; keep the filters.
- `ANNO1503_GAME_DIR` must name the folder that contains `Anlagen.dat`. The owner's second copy sits one level down (`E:\Anno 1503 AD\GOG`, byte-identical to `game/app` on all game content, unregistered, pristine); its parent folder makes `anno::test::game_dir` fail with a clear message, not skip. The T15 harness copy (`C:\anno1503-original`, dgVoodoo2 DLLs, caches and saves) is never a test game dir.
- This SKU keeps its settings in the registry (`HKCU\Software\Max-Design\Anno1503`); there is no `1503addw.ini`. A pristine install has no `SaveGame\`; saves exist only in the harness copy and never enter git.
