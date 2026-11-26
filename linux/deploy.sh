#!/bin/bash

# deployment script for Linux platform
set -e

CONFIGURATION="${1:-debug}"

case "$CONFIGURATION" in
    release|debug)
        ;;
    *)
        echo "Usage: $0 [release|debug]"
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$SCRIPT_DIR/../build/scout/$CONFIGURATION/stage/svencoop"

SVENCOOP_DIR="$HOME/.local/share/Steam/steamapps/common/Sven Co-op"

for artifact in cl_dlls/client.so dlls/server.so libfmodex.so; do
    if [[ ! -f "$STAGE_DIR/$artifact" ]]; then
        echo "Missing $STAGE_DIR/$artifact. Run ./linux/build.sh $CONFIGURATION first." >&2
        exit 1
    fi
done

cp "$STAGE_DIR/cl_dlls/client.so" "$SVENCOOP_DIR/svencoop/cl_dlls/"
cp "$STAGE_DIR/dlls/server.so" "$SVENCOOP_DIR/svencoop/dlls/"
cp "$STAGE_DIR/libfmodex.so" "$SVENCOOP_DIR/svencoop/"
