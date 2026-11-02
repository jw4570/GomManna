#!/usr/bin/env bash
set -euo pipefail
server_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$server_root"
exec ./build/gommanna_server
