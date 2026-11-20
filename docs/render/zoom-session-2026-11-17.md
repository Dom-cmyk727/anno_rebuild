# The owner's zoom recording after ZOOM-SMOOTH-3 (2026-11-17)

**What it checks.** The first owner recording after ZOOM-SMOOTH-1, -2 and -3 landed, compared with the baseline
of 2026-11-14 (`zoom-session-2026-11-14.md`). The owner's acceptance rule (2026-11-13): no late islands and no
dissolves while zooming, with the viewer's not-ready and dissolve counters at 0.

## The run

- **Build:** the candidate `4dfdd9b7`, which contains ZOOM-SMOOTH-3 (`945c3216`), MSVC Release, from
  `anno1503-main-gamefile-run`.
- **Viewer:** `drive_manual.py`: `--play data/maps/lan5.txt --water enhanced`, maximised to 2560 × 1369,
  RTX 4090 (Vulkan), `ANNO_VIEW_HITCH_MS=6`, with stderr stamped with clock time.
- **Runs:**
  - **Run 1** (08:23-08:35): no recording; the owner zoomed for about 40 s at the end.
  - **Run 2** (08:41-08:44): the owner zoomed by hand with narration, then moved and resized the window. It
    was recorded with OBS as a display capture at 2560 × 1440 and 120 fps, 54.55 s:
    `E:\1503_test_videos\rendering_01.mp4`.
- **Sync:** video time 0 is 08:43:19.427, the file's end time minus its duration.
- **Narration:** in English, transcribed locally with faster-whisper (`medium`, int8 on the CPU).

## The narration, matched to the log and the frames

| Video time | The owner | What the log and frames show |
|---|---|---|
| 11.0-15.6 s | "Still some rendering on the island, just the island." | See the first zoom-in, below. |
| 18.5-23.6 s | "If I zoom out really fast, you can see islands, they are already right there." | At 22.8 s, the full map shows with every island at once. No late islands: 0 not-ready frames in every zoom-out window. |
| 24.4-26.7 s | "This is working really, really well, I have to say." | |
| 27.1-31.4 s | "Now we are, did you see that? Down here?" | See the lake, below. |
| 34.2-38.4 s | "Other than that, it's just a little bit of rendering." | |
| 39.2-47.2 s | "I'm now moving the window, holding and moving." | The window was 1280 × 800 from 08:43:58 to 08:44:06. During the held drag the viewer kept drawing at 64-72 fps, with frame gaps of 16-24 ms (ZOOM-SMOOTH-2's draw inside the modal loop). The switch to the smaller size cost one 90 ms gap (one frame busy 9.5 ms). |
| 52.2 s | "Stopping the recording." | |

**The first zoom-in** (video 0.9-2.9 s, the log window ending 08:43:22.336):
- 56 of 946 frames drew a chunk that was not built yet, **up to 59 at once, at the closest zoom (group 0)**;
- 317 chunks dissolved in over a stand-in (up to 27 at once).

It is the only window in either run with more than one not-ready frame. Later visits to the same levels drew
0-1 not-ready frames. In the frames this shows as late detail on the island, as the owner says.

**The lake** ("Down here?", video 28.2-28.6 s):
- At the end of a zoom-in to group 0, the small lake at the bottom left shows a lighter, hazy patch in its left
  half from about 28.32 s. It turns into the final dark water by about 28.6 s, as the camera comes to rest.
- At 31.4 s the owner's pointer rests on that lake.
- The log window (video 26.9-28.9 s) has 0 not-ready frames, 122 dissolves (up to 13 at once) and 2 new wave
  windows.
- So it is a dissolve or a wave-window switch (RC3 in the baseline), not a missing chunk. The frames don't tell
  the two apart.
- The evidence is the crop sheets `crop_bl_28.png` and `crop_bl_native.png`.

## The counters, with the baseline

| | 2026-11-14 owner run | 2026-11-17 run 2 (53 s) | 2026-11-17 run 1 (40 s of zooming) |
|---|---|---|---|
| Not-ready frames per 2 s window | 2-27 % in every zooming window | 56 frames in the first window; 1 in one other; 0 elsewhere | 1 frame in one window; 0 elsewhere |
| Most chunks missing at once | 64 | 59 (first zoom-in only) | 6 |
| Dissolves per 2 s window | 48-366 | 1-317 (621 while zooming) | 4-225 (1,250 while zooming) |
| Frame rate while zooming | 393-450 fps | 443-500 fps | 413-500 fps |
| Worst busy frame while zooming | 10-14 ms | 4.1 ms | 9.9 ms |

**Against the rule:**
- **Late islands:** gone. Not-ready frames are gone too, except on the first visit to the closest zoom.
- **Dissolves:** not 0 in any zooming window, so the zoom does not pass the owner's rule yet. RENDER-DIRECT
  (ADR 0007) removes stand-ins and dissolves by drawing sprites directly from resident atlas pages; RD-1b
  builds those pages.

## The frame scan

Every pair of neighbouring frames (6,545) was compared in 64 px blocks on the client area, scaled to a quarter.
- **While the camera stood still** (mean change below 1.0 for 7 frames): no sharp local change from the
  renderer. The only hits are the mouse pointer travelling along the bottom right at 53.4 s and 54.1 s.
- **A one-frame change over the whole picture at 28.692 s** (mean 2.8, even across the frame) falls on an
  encoder keyframe (keyframes every 2.083 s). It is the video encoding, not the renderer.
- So the late content is all in the motion, as in the baseline.

## Other observations

- **Run 1** had stalls of 216-896 ms at 08:34:01-08:34:08, before any zooming. The time went to window events
  and to waiting for the display, not to rendering. That fits OBS starting or the window taking focus.
- **The other video of the morning,** `ui_6.mp4` (08:35:40, 1080p, 60 fps), is the UI lane's test of
  UI-TABS-1 stage B, not this check.

## Evidence (outside git: frames of the original's art are not published)

`E:\anno1503-worktrees\zoom-session-2026-11-17\`:
- `owner-run\` (run 1) and `owner-run-2\` (run 2): `stderr.log`, `driver.log`, `invocation.txt`.
- `owner-run-2\video\`:
  - `frames.csv`: the per-frame scan;
  - `still.txt`;
  - `narration.txt` and `narration_words.txt`: with word times;
  - `sheet_*.png`, `crop_bl_*.png`;
  - the `ev28\` frame pair.
- `tools\`: the scripts:
  - `zoom_log_summary.py`: the readiness and hitch timeline from `stderr.log`;
  - `scan_video.py` and `scan_still.py`: the frame scan;
  - `sheet.py`, `crop_sheet.py` and `frame_pair.py`: the picture sheets;
  - `transcribe.py` and `transcribe_words.py`: the narration, with the venv in `_whisper\`.
