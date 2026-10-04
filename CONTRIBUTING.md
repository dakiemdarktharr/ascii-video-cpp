# Contributing

Use C++20, Qt6 Widgets, OpenCV 4.x or 5.x and an FFmpeg executable with libx264 and GIF support.
Keep changes focused and explain the observable behavior and tests in a pull request.

Build a fresh directory and run CTest before submitting:

```sh
cmake -S . -B build-review -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-review --parallel 4
ctest --test-dir build-review --output-on-failure
clang-format --dry-run --Werror include/*.hpp src/*.cpp tests/*.cpp benchmarks/*.cpp
```

The Windows vcpkg configuration needs the toolchain arguments shown in README.
Linux headless runs use QT_QPA_PLATFORM=offscreen (CTest sets this for the test).
Do not commit build output, credentials, personal paths or third-party media without permission.
The original synthetic demo comes from the deterministic C++ generator and is covered by LICENSE.
The separate supplied-clip showcase has its own source notes; its underlying animation is not MIT-licensed.

Keep the frame capacity bounded across decoder, workers and encoder. A reorder map that can grow
while one worker stalls is not an acceptable replacement for the ring.
Do not add a benchmark claim without input dimensions, duration, settings, platform and measured results.
