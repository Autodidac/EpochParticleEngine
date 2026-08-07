#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIGURATION="${1:-Release}"
if [[ $# -gt 0 ]]; then shift; fi
"$ROOT/build.sh" "$CONFIGURATION" --skip-tests
exec "$ROOT/out/build/ninja-vulkan-${CONFIGURATION,,}/EpochParticleLab" "$@"
