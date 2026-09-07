"""Build a self-contained MINGW64 NSIS installer; never copy DLLs from Windows."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import urllib.request
import zipfile


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True, errors="replace")


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, default=Path("C:/msys64/mingw64"))
    parser.add_argument("--version", default="1.1.0")
    parser.add_argument("--skip-source-download", action="store_true",
                        help="CI smoke builds only; do not publish these installers")
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        parser.error("version must be major.minor.patch")
    root = Path(__file__).resolve().parents[1]
    prefix = args.prefix.resolve()
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    stage = output / "app"
    stage.mkdir()
    tools = prefix / "bin"
    objdump = tools / "objdump.exe"
    packages = {}
    owners = {}
    msys = prefix.parent
    for entry in (msys / "var/lib/pacman/local").iterdir():
        if not (entry / "desc").exists():
            continue
        desc = (entry / "desc").read_text(encoding="utf-8")
        fields = dict(re.findall(r"%([^%]+)%\n(.*?)(?:\n\n|$)", desc, re.S))
        name = fields["NAME"]
        packages[name] = fields
        for f in (entry / "files").read_text(encoding="utf-8").splitlines():
            if f and not f.endswith("/") and not f.startswith("%"):
                owners[str((msys / f).resolve()).casefold()] = name

    queue = []
    shipped = set()
    seen = set()
    by_name = {p.name.casefold(): p for p in tools.glob("*.dll")}
    system = {p.name.casefold() for p in (Path(os.environ["SystemRoot"]) / "System32").glob("*.dll")}

    def copy(source, relative):
        source = source.resolve()
        destination = stage / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        queue.append(source)
        owner = owners.get(str(source).casefold())
        if owner:
            shipped.add(owner)

    copy(build / "ascii-video-cpp.exe", "ascii-video-cpp.exe")
    copy(tools / "ffmpeg.exe", "ffmpeg.exe")
    for group, names in {"platforms": ["qwindows", "qoffscreen"],
                         "imageformats": ["qjpeg", "qgif", "qico"]}.items():
        for name in names:
            copy(prefix / "share/qt6/plugins" / group / (name + ".dll"),
                 Path("plugins") / group / (name + ".dll"))
    for plugin in tools.glob("opencv_videoio_ffmpeg*.dll"):
        copy(plugin, plugin.name)
    while queue:
        source = queue.pop()
        if source in seen:
            continue
        seen.add(source)
        for dll in re.findall(r"DLL Name:\s*(\S+)", run(objdump, "-p", source)):
            key = dll.casefold()
            if key in by_name:
                if not (stage / by_name[key].name).exists():
                    copy(by_name[key], by_name[key].name)
            elif key not in system and not key.startswith(("api-ms-", "ext-ms-")):
                raise RuntimeError(f"Unresolved dependency {dll} imported by {source.name}")
    (stage / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=plugins\n", encoding="utf-8")
    shutil.copy2(root / "LICENSE", stage / "LICENSE.txt")
    shutil.copy2(root / "README.md", stage / "README.md")
    license_dir = stage / "licenses"
    license_dir.mkdir()
    common = msys / "usr/share/licenses/common"
    if common.exists():
        shutil.copytree(common, license_dir / "common")
    records = []
    for name in sorted(shipped):
        fields = packages[name]
        base, version = fields.get("BASE", name), fields["VERSION"]
        source_name = f"{base}-{version}.src.tar.zst"
        record = {"package": name, "version": version, "license": fields.get("LICENSE", ""),
                  "homepage": fields.get("URL", ""), "source_archive": source_name,
                  "source_url": f"https://repo.msys2.org/mingw/sources/{source_name}"}
        records.append(record)
        short = name.removeprefix("mingw-w64-x86_64-")
        folder = prefix / "share/licenses" / short
        if folder.exists():
            shutil.copytree(folder, license_dir / short)
    # Some MSYS2 packages use shared license texts rather than ship their own copy.
    for filename in ["GPL-3.0-only.txt", "LGPL-3.0-only.txt", "GPL-2.0-only.txt"]:
        source = prefix / "share/licenses/qt6-base" / filename
        if source.exists():
            shutil.copy2(source, license_dir / filename)
    release_url = f"https://github.com/dakiemdarktharr/ascii-video-cpp/releases/tag/v{args.version}"
    notice = ("ASCII Video C++ Windows distribution\n\n"
              "Application source is MIT-licensed; see LICENSE.txt. The bundled FFmpeg and\n"
              "its linked libraries include GPL-3.0-or-later code. This combined binary\n"
              "distribution is provided under GPL-3.0-or-later, without warranty. Qt is\n"
              "dynamically linked; users may replace compatible library DLLs and debug\n"
              "their modifications. Third-party copyright/license notices are in licenses/.\n\n"
              "Exact dependency versions, licenses and upstream source archive URLs are\n"
              "recorded in runtime-manifest.json. Corresponding source packages (including\n"
              "MSYS2 build recipes/patches) and application source are available alongside\n"
              "Setup.exe in the release's source archives:\n" + release_url + "\n")
    (stage / "THIRD-PARTY-NOTICES.txt").write_text(notice, encoding="utf-8")
    if not args.skip_source_download:
        source_dir = output / "sources"
        source_dir.mkdir()
        unique = {r["source_archive"]: r for r in records}
        def fetch(record):
            target = source_dir / record["source_archive"]
            print("Source:", record["source_archive"], flush=True)
            for attempt in range(3):
                try:
                    with urllib.request.urlopen(record["source_url"], timeout=60) as response, target.open("wb") as destination:
                        shutil.copyfileobj(response, destination)
                    return record["source_archive"], digest(target)
                except Exception:
                    if attempt == 2:
                        raise
        with ThreadPoolExecutor(max_workers=6) as pool:
            hashes = dict(pool.map(fetch, unique.values()))
        for record in records:
            record["source_sha256"] = hashes[record["source_archive"]]
        # Include the exact application sources used, including pending packaging edits.
        with zipfile.ZipFile(output / "application-source.zip", "w", zipfile.ZIP_DEFLATED) as archive:
            files = run("git", "-C", root, "ls-files", "--cached", "--others", "--exclude-standard").splitlines()
            for f in sorted(set(files)):
                if (root / f).is_file():
                    archive.write(root / f, "ascii-video-cpp/" + f)
        # Each GitHub release asset must remain below 2 GiB.
        part, size, archive = 0, 0, None
        try:
            for path in sorted(source_dir.iterdir()):
                if archive is None or size + path.stat().st_size > 1_800_000_000:
                    if archive:
                        archive.close()
                    part += 1
                    size = 0
                    archive = zipfile.ZipFile(output / f"dependency-sources-{part}.zip", "w", zipfile.ZIP_STORED)
                archive.write(path, path.name)
                size += path.stat().st_size
        finally:
            if archive:
                archive.close()
    (stage / "runtime-manifest.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(stage / "runtime-manifest.json", output / "runtime-manifest.json")
    # Enumerate owned files, so uninstall never recursively removes users' own exports.
    lines = []
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            relative = str(path.relative_to(stage)).replace("/", "\\")
            lines.append(f'  Delete "$INSTDIR\\{relative}"')
    directories = sorted((p for p in stage.rglob("*") if p.is_dir()), key=lambda p: len(p.parts), reverse=True)
    lines += [f'  RMDir "$INSTDIR\\{str(p.relative_to(stage))}"' for p in directories]
    (output / "uninstall-files.nsh").write_text("\n".join(lines) + "\n", encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = str(tools) + os.pathsep + env.get("PATH", "")
    subprocess.run([str(tools / "makensis.exe"), "-V2", f"-DVERSION={args.version}",
                    f"-DPAYLOAD={stage}", f"-DOUTPUT={output}", str(root / "packaging/windows.nsi")],
                   env=env, check=True)
    assets = [output / "Setup.exe", output / "runtime-manifest.json"] + sorted(output.glob("*-source*.zip"))
    (output / "SHA256SUMS.txt").write_text("".join(
        digest(p) + "  " + p.name + "\n" for p in assets), encoding="ascii")
    print(f"Packaged {len(seen)} PE files, {len(records)} runtime packages; {output / 'Setup.exe'}")


if __name__ == "__main__":
    main()