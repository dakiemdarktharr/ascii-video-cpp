# Windows installer

The NSIS installer targets Windows 10/11 x64 and installs for the current user. It bundles
Qt plugins, the recursive PE-import closure of the application and FFmpeg, and license notices.
It does not change the system PATH. Uninstall deletes its own enumerated files, leaving user files.
Close the app before upgrading or uninstalling.

## Build

Use MSYS2 MINGW64 with the dependencies in the root README, plus:

```sh
pacman -S --needed mingw-w64-x86_64-python mingw-w64-x86_64-nsis
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
python scripts/package_windows.py --build-dir build-release --output-dir build-package --prefix "$(cygpath -m /mingw64)"
```

The output directory must not already exist, preventing stale runtime files from entering a release.
Python is used only for packaging. Installed users need neither Python nor MSYS2.

Test from PowerShell (set the prefix to your MSYS2 installation):

```powershell
.\scripts\test_installer.ps1 -PackageDir build-package -BuildDir build-release -MingwPrefix C:\msys64\mingw64
```

The smoke test requires no existing registered installation of the app. It installs into a fresh
folder with spaces, removes the development PATH, opens the native Qt window, and runs the full
conversion/download/export suite against installed dependencies. It then uninstalls and verifies
that a user-owned marker file survives. Start-Process hides test helper windows.

## Release assets and sources

Upload `Setup.exe`, `SHA256SUMS.txt`, `runtime-manifest.json`, `application-source.zip` and
all `dependency-sources-*.zip` files together to the versioned GitHub release. Executables and
source archives belong in Releases, not Git history. The checksums cover each release asset.

The application source remains MIT-licensed. The Windows binary distribution includes the
GPL-3.0-or-later MSYS2 FFmpeg build and is distributed under GPL-3.0-or-later. Qt is dynamically
linked; compatible replacement DLLs and debugging modifications are permitted. Installed notices
include component licenses and exact source URLs. Dependency source archives include upstream
sources and the MSYS2 build recipes/patches for the versions actually bundled. Application source
is captured from tracked and nonignored project files at packaging time; build folders are ignored.

`--skip-source-download` is only for CI installation tests, whose installers are not published.
A public release must include the source archives. The installed manifest records source archive
SHA-256 values in full release builds. A download failure aborts release packaging.

The binary is currently unsigned. A checksum detects transfer changes but is not a publisher signature.