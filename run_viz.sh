#!/usr/bin/env bash
# Wrapper that sets LD_LIBRARY_PATH for the NVIDIA/Mesa GLX vendor libs
# before launching the tsunami_lab_viz binary.
set -e
cd "$(dirname "$0")"
export LD_LIBRARY_PATH=/usr/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec ./build/tsunami_lab_viz "$@"
