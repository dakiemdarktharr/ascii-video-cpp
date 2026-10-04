# Version 1.2.0 validation

Checks on 2026-10-05 (Asia/Saigon), Windows 11 x64. The previous version's record follows below.

- Release build with GCC 15.2.0, Qt 6.10.1, OpenCV 4.13.0; compiler warnings treated as errors.
- 16 Qt Test entries passed (14 behavior cases plus initialization/cleanup).
- Subtitle regression compares the rendered stroke envelope with a synthetic 1920×1080 subtitle
  image and requires at least 1.25× the overlap of the old 100-column default, plus an absolute
  overlap above 0.45. This allows OpenCV 4/5 resampling differences without accepting worse output.
- Fine output dimensions verified at 1920×1080 and 3840×2160; out-of-range density rejected.
- Video/audio conversion and mute verified by requiring an audio stream with FFmpeg.
- Existing tests verify decoding errors, missing codecs, nominal FPS, frame ordering, downloads,
  exports, GIF budgets, bounded queue occupancy, cancellation and responsive UI actions.
- New UI checks verify resolution presets and a first-frame preview before creating the full output.
- Real CLI 4K conversions saved a 3840×2160 PNG and a 24-frame H.264 MP4 at 24 FPS with AAC audio. Missing output and invalid columns report errors.
- All 16 tests passed against packaged DLLs, Qt plugins and FFmpeg with a system-only PATH.
- Local NSIS packaging produced a self-contained installer. The installation smoke test declined
  to overwrite an existing v1.1.0 user installation; installation/uninstallation is tested on CI runners.
- clang-format, Python compilation and shell syntax checks passed. The generated UI screenshot was reviewed.

[Installer preflight on GitHub Actions](https://github.com/dakiemdarktharr/ascii-video-cpp/actions/runs/37240529683)
passed all five jobs: Windows, Ubuntu, both Mac architectures and Docker. Windows installation/uninstallation,
the installed DEB and both isolated Mac runtimes passed. All native platforms passed the functional tests.
Docker tests build and run the test/runtime/gui targets, including an actual conversion and browser-desktop health.
The release workflow repeats these checks with full dependency sources before publishing. Source availability
was also checked for all 98 unique Windows dependency archives before release. Failed runs remain available
in the repository Actions tab for diagnosis.
The local machine does not have Docker or macOS, so those checks run on GitHub-hosted runners.

The audit found and fixed loss of audio, insufficient default sampling, missing first-frame previews,
platform font differences, macOS signedness warnings and a deprecated assertion in newer Qt Test.
Mac packaging now relocates every bundled import, removes duplicate runtime search paths and signs nested
libraries before the app. Full-length audio processing has no fixed two-minute timeout and remains cancellable.
ASCII retains the visual shape of text; it cannot guarantee recovery of tiny/blurred text. Variable frame
rate timestamps are represented by nominal FPS. Native file-picker gestures and third-party player
playback are not covered by automated controller tests. Installers have no trusted code-signing certificate.

---

# Validation record

Local checks on 2026-09-08 (UTC+07), Windows 11 x64.

## Environment

- GCC/MinGW 15.2.0, CMake 4.3.0, Ninja 1.13.2.
- Qt 6.10.1, OpenCV 4.13.0, FFmpeg 8.1 from MSYS2 MINGW64.
- AMD Ryzen AI 5 340: 6 cores, 12 logical processors.
- Release build, 4 conversion workers, software libx264 encoding with 2 encoder threads.
- clang-format 22.1.2; formatting check passed.

## Build and functional checks

The project was configured in a new `build-clean` directory, then compiled with
`CMAKE_COMPILE_WARNING_AS_ERROR=ON`. The latest code was rebuilt and retested after
checking the rendered desktop window. No compiler warnings were emitted.

CTest passed its test executable: **14 Qt Test entries passed, 0 failed, 0 skipped**
(includes initialization and cleanup; 12 behavior test functions).
The checks cover mapping/adjustments, aspect ratio, fixed-pitch fonts, ANSI output,
image/video decoding, corrupt/missing/zero-byte input, a nonempty container with no
video frames, missing encoder, output reopening, fractional FPS, frame order,
both downloads, both profile exports, GIF size/duration, bounded ring occupancy and cancellation.

The order test identifies each decoded output frame by its nearest rendered reference
among a 16-frame fixture with changing brightness and a moving stripe. It allows H.264
luminance error while requiring the correct frame index.

Qt Test exercised the desktop conversion buttons and image/video import, download and export
controller paths. Its event-loop heartbeat kept ticking. A Qt-rendered screenshot was inspected;
a font fallback problem was found and fixed. Native file-picker interaction, drag/drop gestures
and playback in third-party video players were not manually tested.

Image and video demo inputs were also converted with the clean-build benchmark executable.
Output PNGs reopened in tests, and FFprobe/FFmpeg verified demo containers and decoded the GIF.
The scripts passed PowerShell/Bash syntax checks. Local Markdown asset paths were checked for existence.

## Demo assets

| File | Bytes | Verified content |
| --- | ---: | --- |
| assets/demo-source.png | 62,131 | Original deterministic animation's first frame |
| assets/preview.png | 7,991 | Rendered ASCII poster |
| assets/demo-ascii.mp4 | 440,211 | H.264, 640×368, 24 FPS, 144 frames, 6 seconds |
| assets/demo.gif | 2,207,654 | 60 frames, 6 seconds |

The 640×360 source rounds to 23 character rows at 80 columns, hence a 640×368 ASCII output.
The four synthetic assets above were generated by `benchmarks/main.cpp`; they use no external input.

## Measured example

[Raw timing JSON](benchmarks/windows-example.json) records one conversion of
`assets/demo-ascii.mp4` (640×368, 144 frames) with 100 columns, 4 workers, 8 ring slots.

| Stage | Milliseconds |
| --- | ---: |
| Decode/probe | 141.77 |
| Grayscale/resize (sum across workers) | 216.63 |
| ASCII rendering (sum across workers) | 1,012.61 |
| Glyph atlas initialization | 159.82 |
| Encode/write/finalize | 1,057.04 |
| Total wall time | 1,345.98 |

Measured processing rate: **106.99 FPS** for this single run. Stage times overlap;
the encode path occupies about 79% of wall time in this example, indicating it is the
main observed throughput constraint. This includes pipe backpressure and file finalization,
not only codec CPU time. This is a measurement of a small synthetic input, not a promised speed
on arbitrary video. No hardware acceleration was used.

## Memory checks

The unit test processed 16 and 320 frames using 3 ring slots; both stayed within capacity.
It also exported a 1-second GIF from the longer result and checked its decoded duration.

A separate process-tree working-set sample compared repeated copies of the final demo.
Both runs used the same dimensions, 100 columns, 4 workers and 8 ring slots:

| Input duration | Frames | Samples | Converter sampled peak | FFmpeg sampled peak |
| --- | ---: | ---: | ---: | ---: |
| 24 seconds | 576 | 7 | 70.42 MiB | 83.03 MiB |
| 120 seconds | 2,880 | 17 | 85.80 MiB | 98.64 MiB |

Five times as many frames did not produce five times the retained memory. The ring stayed at
8 slots in both runs. A 32 MiB growth allowance for each process passes these measurements.
The sampler requests 200 ms intervals, but CIM queries add overhead; these are sampled working-set
peaks, not allocator high-water marks. Codec caches/OS paging can vary. This does not prove
all decoders are leak-free or replace a long-duration stress test on production media.

Reproduce with `scripts/measure_memory.ps1`, then compare the CSV files with:

```powershell
.\tests\check_memory.ps1 -ShortCsv build/memory-final-24.csv -LongCsv build/memory-final-120.csv
```

## Publication and platform status

- Published repository: [dakiemdarktharr/ascii-video-cpp](https://github.com/dakiemdarktharr/ascii-video-cpp), branch `main`.
- Initial release commit `77f9216af8b772b03e76fe18e6f1f8ccb1770995` was pushed successfully.
- Publication was authenticated as `dakiemdarktharr`; no credentials are stored in the repository.
- Remote Windows/Ubuntu build results are available in [GitHub Actions](https://github.com/dakiemdarktharr/ascii-video-cpp/actions/workflows/build.yml).
- Ubuntu, macOS and the MSVC/vcpkg build path were not executed locally; WSL is not installed.
- No API keys, tokens or personal absolute paths are intended in tracked files. Build output,
  temporary exports and measurements containing machine paths are ignored.
- Hardware acceleration, audio passthrough, VFR timestamp preservation, a playback timeline,
  and ZIP export inside the app are not implemented. Windows installer validation is recorded below.

See [publication commands](PUBLISHING.md). The README describes the implemented behavior and
links to live CI results rather than assuming a workflow succeeded.

## Supplied README clip

The separate `assets/readme-demo/` showcase was converted from the owner's supplied video
with the clean-build application, 96 columns and 4 workers. It does not replace the synthetic
fixtures or the benchmark above. FFmpeg fully decoded all three outputs without errors:

| File | Bytes | Verified content |
| --- | ---: | --- |
| ascii-video.mp4 | 7,071,039 | H.264, 768×576, 24 FPS, 798 frames, 33.25 seconds, silent |
| preview.gif | 2,175,718 | 576×432, 48 frames, 6.01 seconds |
| poster.png | 180,180 | 640×480 PNG |

The GIF samples the first 6 seconds at 8 FPS; GIF's centisecond timing rounds its measured
playback duration to 6.01 seconds. The poster was visually inspected. Source notes in the
asset directory distinguish the supplied animation from MIT-licensed synthetic media.

## Windows installer 1.1.0

A new Release build in `build-installer-check` passed configuration, compilation with warnings
as errors, and all 14 Qt Test entries. NSIS 3.11 packages 151 PE files from 99 runtime packages.
The bundle contains Qt's Windows/offscreen/image plugins and FFmpeg; no development PATH is needed.

`scripts/test_installer.ps1` passed on Windows 11 x64:

- Silent per-user installation into a path containing spaces; Start Menu shortcut and uninstall registration.
- Installed GUI initialization, version command and graceful window close with only Windows on PATH.
- No modules loaded from the MSYS2 development prefix.
- All 14 Qt Test entries against installed DLLs/FFmpeg, covering image/video conversion, downloads and profile exports.
- Silent uninstall removed application files, shortcut and registry entry while preserving a user-owned file.

The initial smoke test incorrectly used MainWindowHandle for a hidden window. It now finds the
Qt window by process ID/title and sends WM_CLOSE; the corrected end-to-end test passed.
The interactive installer wizard was not manually clicked through. The installer is unsigned.
NSIS emits warning 9000 for the requested generic filename Setup.exe (Windows compatibility shims);
C++ compilation emitted no warnings. Windows/Ubuntu CI also runs for each packaging change, with
Windows additionally building and testing an installer. See the live Actions run for its result.
