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
BUILD_DIR="$SCRIPT_DIR/$CONFIGURATION"

SVENCOOP_DIR="$HOME/.local/share/Steam/steamapps/common/Sven Co-op"

echo "Starting Linux deployment script..."
cp "$BUILD_DIR/client.so" "$SVENCOOP_DIR/svencoop/cl_dlls/"
cp "$BUILD_DIR/server.so" "$SVENCOOP_DIR/svencoop/dlls/"

echo "DONE"
