#!/usr/bin/env bash
set -euo pipefail
server_root="$(cd "$(dirname "$0")/.." && pwd)"
args=()
if command -v brew >/dev/null 2>&1; then
  pg_prefix="$(brew --prefix postgresql@17 2>/dev/null || true)"
  ssl_prefix="$(brew --prefix openssl 2>/dev/null || true)"
  if [[ -d "$pg_prefix" ]]; then args+=("-DPostgreSQL_ROOT=$pg_prefix"); fi
  if [[ -d "$ssl_prefix" ]]; then args+=("-DOPENSSL_ROOT_DIR=$ssl_prefix"); fi
fi
cmake -S "$server_root" -B "$server_root/build" -DCMAKE_BUILD_TYPE=Debug "${args[@]}" "$@"
cmake --build "$server_root/build" --parallel 3
