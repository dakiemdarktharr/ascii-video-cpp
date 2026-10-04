# Packaging and releases

`Build and test` builds on Windows x64, Ubuntu 24.04 x64, macOS 15 arm64 and macOS 15 Intel.
Each platform runs the functional tests and packages an installer; artifacts from ordinary CI builds are
smoke builds without downloaded dependency sources and must not be published as releases.
Docker CI builds and runs the test/runtime/gui targets, verifies a real conversion and the browser desktop.

## Windows

```sh
python scripts/package_windows.py --build-dir build --output-dir build-package
```

Run in MINGW64 after building. The output folder must not already exist. The script recursively resolves
PE imports, bundles FFmpeg and Qt plugins, records dependency versions/licenses, downloads corresponding
MSYS2 source packages and build recipes, then produces `Setup.exe`, source archives and checksums.
The installer is per-user; uninstall enumerates its own files and preserves user-created exports.

```powershell
./scripts/test_installer.ps1 -PackageDir build-package -BuildDir build
```

The test installs to a new folder, launches the GUI, checks imported module paths, runs functional tests
with the installed runtime and a system-only PATH, then uninstalls while preserving a user-owned marker.
It refuses to run if the app is already registered on the machine. Run this only on a disposable test installation.

## Ubuntu

Configure with `-DCMAKE_INSTALL_PREFIX=/usr`, build, then run `cpack -G DEB` in the build folder.
CPack uses dpkg-shlibdeps to resolve Qt/OpenCV runtime dependencies; FFmpeg, fonts and Qt platform plugins
are explicit dependencies. Install the resulting DEB with apt so dependencies are fetched automatically.
This package targets Ubuntu 24.04 x64. Other distributions can build from source or use Docker.

## macOS

```sh
python3 scripts/package_macos.py --build-dir build --output-dir build-package
```

Requires Homebrew Qt, OpenCV, FFmpeg and dylibbundler. Qt's macdeployqt deploys frameworks/plugins;
dylibbundler resolves native libraries for both the app and FFmpeg. The script rejects leaked Homebrew install
names, adds an ad-hoc signature, writes runtime provenance, licenses and source recipes, downloads corresponding
sources and formula patches with checksum verification,
and creates a DMG with an Applications shortcut. CI verifies signatures and the DMG, then converts a video using
only the bundled runtime and a system-only PATH. Intel and Apple Silicon builds are separate.
Production Developer ID signing/notarization requires the project's own Apple credentials; these are not configured.

## Publish

Keep CMake, vcpkg, installer test and script versions in sync. After reviewing and testing the commit, push a
matching version tag (`v1.2.0` for this release). `Publish installers` validates the tag, rebuilds and tests every
platform, downloads corresponding dependency sources, computes SHA256 checksums and publishes all artifacts
only when every job succeeds. Release notes are in `packaging/release-notes.md`.

Application source is MIT. Windows/macOS distributions include GPL FFmpeg components and their notices,
source archives, dependency versions and build recipes. Libraries remain dynamically linked. Do not omit the
corresponding sources when redistributing the installer bundles.
