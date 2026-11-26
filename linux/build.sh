#!/bin/bash

set -euo pipefail

CONFIGURATION="${1:-debug}"

case "$CONFIGURATION" in
    release) BUILD_TYPE=Release ;;
    debug) BUILD_TYPE=Debug ;;
    *)
        echo "Usage: $0 [release|debug]"
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE="svencoop-scout-cmake:i386"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Building $IMAGE (one time)..."
    docker build --platform linux/386 -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
fi

BUILD_DIR="/svencoop/build/scout/$CONFIGURATION"
docker run --rm --platform linux/386 --user "$(id -u):$(id -g)" \
    -v "$REPO_ROOT:/svencoop" -w /svencoop "$IMAGE" /bin/bash -ec '
        cmake -S . -B "$1" -DCMAKE_BUILD_TYPE="$2" -DCMAKE_C_COMPILER=gcc-9 -DCMAKE_CXX_COMPILER=g++-9
        cmake --build "$1" --parallel "$3"
        cmake --install "$1" --prefix "$1/stage"
    ' bash "$BUILD_DIR" "$BUILD_TYPE" "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"

echo "Installed to $REPO_ROOT/build/scout/$CONFIGURATION/stage"
