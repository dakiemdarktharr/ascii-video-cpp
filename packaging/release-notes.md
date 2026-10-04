ASCII Video now defaults to Full HD with fine ASCII characters. Use 4K for small subtitles, or large characters for the classic look. Edge enhancement helps text retain its shape; the output remains pure ASCII art.

- Clear English interface, first-frame preview before conversion, actual-size preview and a new app icon.
- Original sound retained by default (AAC); a mute option is available.
- HD, Full HD and 4K width presets, up to 960 characters per line, higher quality H.264 encoding.
- Docker build, tests, headless conversion and a browser desktop through Docker Compose.
- Automated build, functional tests and installer smoke checks on Windows and Ubuntu.

Download **Setup.exe** for Windows 10/11 x64, or **ascii-video_1.2.2_amd64.deb** for Ubuntu 24.04 x64.

The Windows installer includes Qt, OpenCV and FFmpeg and has no trusted code-signing certificate. Linux installs dependencies through apt. Tiny or blurred text cannot always remain readable in ASCII. Saved video preserves the source's nominal frame rate; variable-frame-rate timing is not preserved exactly.

Runtime manifests, corresponding dependency sources and SHA256 checksums are included with the release. Application source remains MIT; bundled binary distributions include third-party GPL components as described in their notices.
