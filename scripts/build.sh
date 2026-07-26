#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m compileall -q linuxpict
python3 -m unittest discover -s tests
echo "LinuxPict build checks passed."
