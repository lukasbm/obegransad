#!/usr/bin/env bash
#
# Regenerate the sprite blobs and C++ wrappers from sprites.toml and the .ase
# sources. Thin wrapper around generate.py, kept for the familiar entry point.
#
#   ./convert_all.sh          write data/*.bwb, main/sprites/*.hpp and the
#                             CMake embed list
#   ./convert_all.sh --check  fail if the committed files are stale
set -euo pipefail

cd "$(dirname "$0")"
exec python3 generate.py "$@"
