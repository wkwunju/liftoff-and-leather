#!/usr/bin/env bash
# Build the static web bundle into dist/.
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v emcc >/dev/null 2>&1; then
  if [[ -f /opt/emsdk/emsdk_env.sh ]]; then
    # shellcheck disable=SC1091
    source /opt/emsdk/emsdk_env.sh
  elif [[ -f "${HOME}/emsdk/emsdk_env.sh" ]]; then
    # shellcheck disable=SC1091
    source "${HOME}/emsdk/emsdk_env.sh"
  elif [[ -f "${EMSDK:-}/emsdk_env.sh" ]]; then
    # shellcheck disable=SC1091
    source "${EMSDK}/emsdk_env.sh"
  else
    echo "emcc not found. Install the Emscripten SDK and source emsdk_env.sh." >&2
    echo "https://emscripten.org/docs/getting_started/downloads.html" >&2
    exit 1
  fi
fi

RAY=third_party/raylib
if [[ ! -f "${RAY}/src/raylib.h" ]]; then
  mkdir -p third_party
  git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "${RAY}"
fi

make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
echo
echo "Static bundle:"
echo "  dist/index.html"
echo "  dist/index.js"
echo "  dist/index.wasm"
echo
echo "Serve it with:  python3 -m http.server 8747 --directory dist"
