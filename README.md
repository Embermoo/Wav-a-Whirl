# Wav-a-Whirl 1.1 — stages 1–4

Requires C++17, CMake 3.16+, and Qt 6.2+ Widgets and Multimedia.
Qt 5 is no longer supported by this build. The existing development kit is Qt 6.11.

Build with a Qt-compatible compiler:
    cmake -S . -B build -DCMAKE_PREFIX_PATH=<your-Qt-kit>
    cmake --build build

The source now uses Qt Multimedia instead of Windows PlaySound.
Qt Multimedia supplies the asynchronous playback backend:
https://doc.qt.io/qt-6/qmediaplayer.html

Behavior:
- The Premium button is a joke: it displays a free-premium congratulations message.
  All features remain free; the button does not change playback or settings.
- Audio files directly inside the selected folder; Unicode paths and mixed-case extensions supported.
- Scans WAV, MP3, FLAC, Ogg (.ogg/.oga), Opus, M4A, AAC, and AIFF (.aif/.aiff).
  Actual decoding depends on the file's codec and the installed Qt media backend.
- Volume adjusts live from 0% (silent) to 100%, including while paused.
  The slider uses a quadratic curve for finer quiet-level control, starts at 100%,
  and is remembered across launches.
- Folder, duration, delay range, volume, Spoiler mode, balancing preference, and
  window geometry are saved on normal exit using Qt QSettings in the current
  user's platform settings location. Playback always starts idle. Missing saved
  folders are cleared with a log message; restored numeric values are clamped.
- Controls use Qt layouts rather than fixed coordinates. The log grows with the
  window, and minimum dimensions follow text and control size hints. The selected
  folder is shown in a read-only field that supports long paths.
- Optional "Balance clip volume" is off on first launch and chosen before Play.
  It analyzes the folder before playback and reduces louder clips toward its
  quietest non-silent clip, with a maximum target of -24 dBFS RMS. Files below
  -100 dBFS RMS are excluded from target selection. A very quiet clip can lower
  the whole folder's output; the volume slider still controls overall level.
  It never boosts quiet clips or silence. This is approximate RMS
  balancing, not LUFS/EBU R128 normalization; speech, music, and clips with long
  silences can still sound different. The volume slider multiplies this adjustment.
- Analysis uses Qt AudioDecoder, streams buffers without storing decoded files,
  and caches up to 256 results in memory by path, size, and modification time.
  No original file is written. First playback may wait briefly; analysis time does
  not consume session time. Pause prevents playback starting; Hard Stop cancels it.
  Analysis errors or a 15-second timeout fall back to original levels with a log
  message. Changing a file's contents without changing its size or timestamp may
  require restarting the app to clear cached analysis.
- First clip starts immediately after optional folder analysis; the selected delay follows the end of each clip.
- Pause freezes playback, the inter-clip delay, and the session clock.
- Resume continues the paused clip/delay.
- Hard Stop and session expiration stop the current clip immediately.
- Spoiler mode applies to subsequent playback/error log entries.
- Zero-second delays allow back-to-back clips (with backend loading overhead).
- Failed clips wait at least two seconds before another selection.
- The countdown appears only in the bottom status bar, keeping the log clear.
- Folder and duration/delay controls are locked during a session.
- File and whole-second delay selections use Qt's system-seeded random generator
  with standard C++ uniform integer distributions. Both delay endpoints are
  included; equal min/max delays and single-file folders work, and repeats are allowed.

Validation:
    cmake -S . -B build -DWAV_BUILD_TESTS=ON
    cmake --build build
    ctest --test-dir build --output-on-failure

Tests additionally require Qt Test, an available media backend, and audio output.
Tests mute playback, create temporary silent WAV fixtures, and play the generated
two-second silent samples in tests/audio (MP3, FLAC, Vorbis/Ogg, Opus, AAC/M4A,
raw AAC, and AIFF). These fixtures were encoded with FFmpeg for testing only;
the application has no new dependency. Volume and playback control tests pass
on Windows with Qt 6.11's FFmpeg backend. Balancing tests cover relative levels,
silence, cache invalidation, decode failure, all encoded fixture formats, pause,
stop, and live slider interaction. No additional application dependencies were added.
Windows compilation and playback regression checks were run with Qt 6.11/MinGW.
Linux and macOS builds, audible playback, layout, and deployment still need native
verification. Package Qt platform and multimedia plugins and the backend's
runtime libraries with releases (for example, windeployqt on Windows and
macdeployqt on macOS). The build directory alone is not a distributable release.

Original source files are preserved in stage1-backup.

Release preparation:
- Windows test ZIP: dist/Wav-a-Whirl-1.1-windows-x64-test.zip.
  Extract the entire folder and launch Wav-a-Whirl.exe.
- Repackage with scripts/package-windows.ps1 after a Release build. Choose a new
  OutputName for each candidate; the script refuses to overwrite existing output.
- .github/workflows/desktop-builds.yml compiles the app and test target on Linux
  and macOS with Qt 6.11. It will run on push/PR or manual dispatch once this
  source is in a GitHub repository. Check the GitHub Actions tab for results. These checks do
  not claim native playback verification or produce portable
  Linux/macOS installers.
- release-notes/TESTING.md is the native tester checklist.
- Copyright (c) 2026 Embermoo. Source is licensed under GNU GPL version 3 only (GPL-3.0-only); see LICENSE. Before a public binary release, complete
  the dependency license/source notices. The local package is a test candidate,
  not a signed installer or a finished public release.
