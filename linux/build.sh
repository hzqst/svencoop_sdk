#!/bin/bash

# build script for Linux platform
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
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE="svencoop-scout-sdk:i386"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Building $IMAGE (one time)..."
    docker build --platform linux/386 -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
fi

docker run --rm --user "$(id -u):$(id -g)" -v "$REPO_ROOT":/svencoop -w /svencoop "$IMAGE" /bin/bash -lc "
set -e
make -C linux CFG=$CONFIGURATION
"

echo "DONE"
