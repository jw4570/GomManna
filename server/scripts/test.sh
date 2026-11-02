#!/usr/bin/env bash
set -euo pipefail
server_root="$(cd "$(dirname "$0")/.." && pwd)"
"$server_root/scripts/build.sh"
ctest --test-dir "$server_root/build" --output-on-failure
python3 "$server_root/tests/integration.py" "$@"
