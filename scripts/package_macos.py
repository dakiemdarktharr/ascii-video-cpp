"""Bundle Qt/OpenCV/FFmpeg and build a macOS DMG, with dependency provenance."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile


def run(*args):
    print("Running:", str(args[0]), flush=True)
    try:
        return subprocess.check_output([str(a) for a in args], text=True, stdin=subprocess.DEVNULL, timeout=600)
    except subprocess.CalledProcessError as error:
        print(error.output, flush=True)
        raise


def checksum(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def dependencies(path):
    return [line.strip().split(" (", 1)[0] for line in run("otool", "-L", path).splitlines()[1:]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--skip-source-download", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    bundle = output / "ASCII Video.app"
    shutil.copytree(args.build_dir / "ascii-video-cpp.app", bundle)
    executable = bundle / "Contents/MacOS/ascii-video-cpp"
    ffmpeg = bundle / "Contents/MacOS/ffmpeg"
    ffmpeg_source = Path(shutil.which("ffmpeg")).resolve()
    shutil.copy2(ffmpeg_source, ffmpeg)
    # Discover provenance before replacing the original Homebrew install names.
    cellar = Path(run("brew", "--cellar").strip()).resolve()
    owners, visited = set(), set()
    pending = [executable, ffmpeg_source]
    while pending:
        path = pending.pop().resolve()
        if path in visited:
            continue
        visited.add(path)
        if path.is_relative_to(cellar):
            owners.add(path.relative_to(cellar).parts[0])
        for reference in dependencies(path):
            dependency = Path(reference)
            if dependency.is_file() and dependency.resolve().is_relative_to(cellar):
                pending.append(dependency)
    qt = Path(run("brew", "--prefix", "qt").strip())
    for resolved in (qt.resolve(), (qt / "lib/QtCore.framework").resolve()):
        if resolved.is_relative_to(cellar):
            owners.add(resolved.relative_to(cellar).parts[0])
    architecture = run("uname", "-m").strip()
    resources = bundle / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    shutil.copy2(root / "LICENSE", resources / "LICENSE.txt")
    shutil.copy2(root / "assets/fonts/LICENSE-DejaVu.txt", resources / "LICENSE-DejaVu.txt")
    plugin_root = Path(run(qt / "bin/qmake", "-query", "QT_INSTALL_PLUGINS").strip())
    plugins = []
    for group, names in {"platforms": ["qcocoa", "qoffscreen"],
                         "styles": ["qmacstyle"], "imageformats": ["qjpeg", "qgif", "qico"]}.items():
        for name in names:
            source = plugin_root / group / ("lib" + name + ".dylib")
            target = bundle / "Contents/PlugIns" / group / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            plugins.append(target)
    # Deploy exactly the plugins used by this Widgets app, including offscreen smoke tests.
    # Unused SVG/PDF/virtual-keyboard plugins can require absent optional Qt modules.
    run(qt / "bin/macdeployqt", bundle, "-always-overwrite", "-no-plugins", "-verbose=1",
        f"-executable={ffmpeg}", *(f"-executable={plugin}" for plugin in plugins))
    # Qt handles frameworks/plugins; dylibbundler closes the other native dependencies.
    native = [executable, ffmpeg]
    for path in bundle.rglob("*"):
        if path.is_file() and not path.is_symlink() and "Mach-O" in run("file", "-b", path):
            if path not in native:
                native.append(path)
    arguments = ["dylibbundler", "-b", "-cd", "-of", "-ns", "-d", bundle / "Contents/Libraries",
                 "-p", "@executable_path/../Libraries/", "-i", "/System/Library",
                 "-i", bundle / "Contents/Frameworks", "-s", bundle / "Contents/Frameworks",
                 "-s", Path(run("brew", "--prefix").strip()) / "lib"]
    for path in native:
        arguments += ["-x", path]
    run(*arguments)
    for path in bundle.rglob("*"):
        if path.is_file() and not path.is_symlink() and "Mach-O" in run("file", "-b", path):
            for reference in dependencies(path):
                if reference.startswith(("/opt/homebrew/", "/usr/local/")):
                    raise RuntimeError(f"Unbundled dependency: {path}: {reference}")
    # Plugins deployed by macdeployqt may bring additional native libraries.
    prefix = Path(run("brew", "--prefix").strip())
    for path in bundle.rglob("*"):
        if path.is_file() and not path.is_symlink():
            original = prefix / "lib" / path.name
            if original.is_file() and original.resolve().is_relative_to(cellar):
                owners.add(original.resolve().relative_to(cellar).parts[0])
    records = json.loads(run("brew", "info", "--json=v2", *sorted(owners)))["formulae"]
    for record in records:
        recipe = Path(run("brew", "--prefix", record["name"]).strip()) / ".brew" / (record["name"] + ".rb")
        if recipe.is_file():
            shutil.copy2(recipe, resources / recipe.name)
        else:
            (resources / (record["name"] + "-formula.rb")).write_text(run("brew", "cat", record["name"]))
    (resources / "runtime-manifest.json").write_text(json.dumps(records, indent=2) + "\n")
    shutil.copy2(resources / "runtime-manifest.json", output / f"macos-{architecture}-runtime-manifest.json")
    (resources / "THIRD-PARTY-NOTICES.txt").write_text(
        "Application source: MIT (LICENSE.txt). Bundled FFmpeg and its dependencies include GPL code.\n"
        "This binary distribution is under GPL-3.0-or-later, without warranty. Qt is dynamically linked.\n"
        "You may replace compatible libraries and debug your modifications. Exact upstream source URLs,\n"
        "licenses, versions and Homebrew build recipes are included here. Corresponding source archives\n"
        "are distributed with the DMG in the GitHub release.\n")
    run("codesign", "--force", "--deep", "--sign", "-", bundle)
    run("codesign", "--verify", "--deep", "--strict", bundle)
    if not args.skip_source_download:
        sources = output / "sources"
        sources.mkdir()

        def fetch(record):
            stable = record["urls"]["stable"]
            if not stable:
                raise RuntimeError(f"No stable sources for {record['name']}")
            destination = sources / (record["name"] + "-" + Path(stable["url"].split("?", 1)[0]).name)
            for attempt in range(3):
                try:
                    with urllib.request.urlopen(stable["url"], timeout=60) as response, destination.open("wb") as target:
                        shutil.copyfileobj(response, target)
                    if stable.get("checksum") and checksum(destination) != stable["checksum"]:
                        raise RuntimeError(f"Source checksum mismatch: {record['name']}")
                    return
                except Exception:
                    if attempt == 2:
                        raise
        with ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(fetch, records))
        part, size, archive = 0, 0, None
        try:
            for path in sorted(sources.iterdir()):
                if archive is None or size + path.stat().st_size > 1_800_000_000:
                    if archive:
                        archive.close()
                    part += 1
                    size = 0
                    archive = zipfile.ZipFile(output / f"macos-{architecture}-dependency-sources-{part}.zip", "w", zipfile.ZIP_STORED)
                    for recipe in resources.glob("*.rb"):
                        archive.write(recipe, recipe.name)
                    archive.write(resources / "runtime-manifest.json", "runtime-manifest.json")
                archive.write(path, path.name)
                size += path.stat().st_size
        finally:
            if archive:
                archive.close()
    # Drag the app onto Applications in the disk image.
    image_root = output / "image"
    image_root.mkdir()
    shutil.copytree(bundle, image_root / bundle.name, symlinks=True)
    (image_root / "Applications").symlink_to("/Applications")
    architecture = run("uname", "-m").strip()
    dmg = output / f"ASCII-Video-macOS-{architecture}.dmg"
    run("hdiutil", "create", "-volname", "ASCII Video", "-srcfolder", image_root,
        "-ov", "-format", "UDZO", dmg)
    print(dmg)


if __name__ == "__main__":
    main()
