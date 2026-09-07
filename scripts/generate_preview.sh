#!/usr/bin/env bash
set -euo pipefail
build_dir="${1:-build}"
assets_dir="${2:-assets}"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$build_dir/ascii-benchmark" --demo "$assets_dir"
