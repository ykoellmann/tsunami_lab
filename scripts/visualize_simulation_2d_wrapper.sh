#!/usr/bin/env bash
xvfb-run -s "-screen 0 1920x1080x24 +extension GLX" pvpython "$(dirname "$0")/visualize_simulation_2d.py" "$@"