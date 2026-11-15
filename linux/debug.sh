#!/bin/bash

# debug script for Linux platform
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export SVENCOOP_ROOT="$HOME/.local/share/Steam/steamapps/common/Sven Co-op"
export LD_LIBRARY_PATH="$SVENCOOP_ROOT:$LD_LIBRARY_PATH"
export SteamEnv=1 # Fix the annoying locale error.

cd "$SVENCOOP_ROOT" || exit 1

exec ~/.steam/bin/steam-runtime/run.sh gdb \
    -ex 'set breakpoint pending on' \
    -ex "set substitute-path /svencoop $REPO_ROOT" \
    --args "$SVENCOOP_ROOT/svencoop_linux" -steam -window -w 1600 -h 900 -dev -gl -game svencoop
