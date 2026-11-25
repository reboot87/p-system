# tools/diagnostics — manual / ad-hoc diagnostics

These sources are **not** part of the automated test suite: they are manual,
interactive diagnostics used during development to observe window animation,
frame capture and gallery draw-call behaviour. They are not built by CMake and
are not registered as ctest cases.

To use one, add it to a build target by hand (or compile directly), start a
**running `airecord.exe`** first when a diagnostic looks up the `AireRecordWnd`
window, and interpret the printed numbers yourself — most of these tools print
measurements (black-pixel %, frame diff %, op counts) rather than a
pass/fail verdict that a machine can assert on.

## Window-animation / frame-forensics tools

These require a running `airecord.exe` window (`FindWindowW(L"AireRecordWnd")`).

- `test_flash_screen.cpp` — screen-level (real user-visible) near-black
  coverage during the maximize/restore animation. Startup: `airecord.exe`
  running and visible.
- `test_maximize.cpp` — client-area frame capture during the maximize/restore
  animation, reports worst-frame black %. Startup: `airecord.exe` running.
- `test_max_frames.cpp` — captures frames to BMP while sending SC_MAXIMIZE.
  Startup: `airecord.exe` running.
- `test_restore_frames.cpp` — captures frames to BMP while sending SC_RESTORE.
  Startup: `airecord.exe` running.
- `test_scroll_frames.cpp` — captures window-region frames to BMP while
  sending `WM_VSCROLL` to the clipboard list. Startup: `airecord.exe` running,
  on the clipboard page.
- `test_scroll_diff.cpp` — analyses the captured `scroll_*.bmp` frames
  (written by `test_scroll_frames`) for inter-frame churn. Run *after*
  `test_scroll_frames` has produced frames in the temp dir.

## Gallery draw-call tools

These are platform-free (ShellCore + a recording IPainter) but write a small
draw-call dump to `gallery_page_drawcalls_*.txt` for manual diffing; they are
kept here because their output is a human inspection artifact, not an
assertion.

- `test_gallery_page_regression.cpp` — captures enabled vs disabled gallery
  page draw calls to `gallery_page_drawcalls_enabled/disabled.txt`.
- `test_gallery_shellcore_drawcalls.cpp` — prints the ShellCore draw op count
  (asserts an expected count of 13, so it can fail) and lists the first ops.

## TextBox lab drivers (Windows only)

Cross-process drivers for the `airui_textbox_lab.exe` repro harness
(`examples/TextBoxLab.cpp`, window class `AirUiTextBoxLab`). Built as
console targets on WIN32 (`labtest` / `menuclick` / `boxrect` / `pixhash`),
not registered in ctest — `textbox_lab_suite.ps1` drives them end to end:

- `labtest.c` — sends real `WM_*`/`EM_*` messages to the lab's child boxes
  (ids 3001..3007): `info`/`rect`/`type`/`key`/`focus`/`clear`/`settext`/
  `setsel`/`getsel`/`getraw`/`dblclick`/`drag`/`rclick`. Text is read via
  `getraw` (`WM_GETTEXT`) and selection via `getsel` (`EM_GETSEL`) —
  `GetWindowTextW` returns empty across processes. `settext` sends a full
  `WM_SETTEXT` string (the deterministic programmatic-set path, e.g. for
  maxLength clipping). `rclick` **blocks** while the self-drawn edit menu is
  open (its modal loop), so the suite runs it in the background. `key` posts
  a raw `WM_KEYDOWN`, which the proc's `GetKeyState(VK_CONTROL)`-based
  Ctrl/Shift test cannot see — so the Ctrl+Z/Ctrl+Y undo/redo shortcuts are
  driven through the **edit menu** (the menu's 撤销/重做 call the same
  `TextBox::Undo()/Redo()`).
- `menuclick.c` — finds the self-drawn popup menu window
  (`AirUiOverlayWnd`) and clicks one of its items with `PostMessage` (NOT
  `SendMessage`: the modal loop lives in the `rclick` process, so a
  same-call `SendMessage` would flip the `done` flag on the wrong thread's
  stack and the loop would never wake). Takes `<itemIndex> <pixelY>`: the
  edit menu mixes row heights (item rows 36px, the separator 6px), so the
  click point is a measured client y, not an even split. Current edit menu
  (撤销/重做 / --- / 复制/剪切/粘贴, 188px window = 186 client): row centers
  0 撤销 y=18, 1 重做 y=54, 2 separator y=75, 3 复制 y=96, 4 剪切 y=132,
  5 粘贴 y=168. Waits up to 3 s for the menu window to be destroyed.
- `boxrect.c` — prints the screen rects of the `AirUITextBox` children of a
  given shell hwnd (screenshot-crop helper).
- `pixhash.c` — fingerprints the pixels of a screen rectangle (BitBlt +
  FNV-1a 64). T5 uses it to assert that a password box (• mask) and its
  plaintext twin with the same text render *differently*, and that the same
  rect hashes identically on a second pass (stability control).

Run the full suite (T1 double-click word select / T2 drag-select /
T3 right-click copy-paste round trip / T4 undo-redo through the edit menu /
T5 maxLength + password) with:

```
cmake --build build --config Release `
    --target airui_textbox_lab labtest menuclick boxrect pixhash
powershell -ExecutionPolicy Bypass -File tools\diagnostics\textbox_lab_suite.ps1
```

Exit code 0 = all five scenarios pass; the script kills all
`airui_textbox_lab`/`labtest` processes when it is done.