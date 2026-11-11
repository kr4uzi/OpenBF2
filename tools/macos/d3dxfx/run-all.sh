#!/bin/sh
# [CHOSEN, not recovered] Reproducible offline D3DX / MojoShader experiment.
set -eu
cd "$(dirname "$0")/../../.."
exec python3 tools/macos/d3dxfx/pipeline.py "$@"
