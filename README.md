# svencoop_sdk <img src="media/re_sven1.png" width="120" align="right">

Reverse-engineered source code of Sven Co-op (5.26), current Steam build.

This repository contains:
- `client.dll/.so`
- `server.dll/.so` (gamedll)

## Getting started

Initialize some dependencies after cloning:

```sh
git submodule update --init --recursive
```

## Building

### FMOD Ex 4.44.64

Download the **complete FMOD Ex 4.44.64 SDK package** for your platform from the
[ZDoom FMOD downloads](https://zdoom.org/files/fmod/). The client needs the SDK
headers and link library as well as the runtime. Git ignores both local packages.

- Windows: extract the SDK to `external/fmodapi44464win/`. Check for
  `api/inc/fmod.hpp`, `api/lib/fmodex_vc.lib` and `api/fmodex.dll` inside it.
- Linux: place `fmodapi44464linux.tar.gz` in `external/`. CMake extracts the
  headers and i386 library into the build directory.

For a server-only build, pass `-DSVENCOOP_BUILD_CLIENT=OFF` when configuring
CMake. The server can build without FMOD.

### Windows Win32

Install CMake 3.25 or newer, Visual Studio 2019 with the v142 C++ toolset, and
the Windows 10 SDK. Run from the repository root:

```powershell
cmake -S . -B build/windows-win32 -G "Visual Studio 16 2019" -A Win32 -T v142
cmake --build build/windows-win32 --config Release --parallel
cmake --install build/windows-win32 --config Release --prefix build/windows-win32/stage/Release
```

Use `Debug` in place of `Release` in the build and install commands for a debug build.

### Linux i386 (Steam Runtime Scout)

Install Docker with BuildKit support and ensure your user can access its
daemon. Run from the repository root:

```sh
./linux/build.sh debug
./linux/build.sh release
```

The first run builds the Scout i386 image with CMake. Each configuration
installs into `build/scout/<configuration>/stage`.

Each staging directory contains `svencoop/cl_dlls/client.dll` or `client.so`,
`svencoop/dlls/server.dll` or `server.so`, and the FMOD runtime in
`svencoop/`. Use `./linux/deploy.sh debug` or `./linux/deploy.sh release` to
copy the corresponding Linux staging files into your Steam installation.

The previous Visual Studio solution (`projects/vs2010/svencoop.sln`) and
Linux Makefiles (`make -C linux CFG=release`) remain available as manual entry points.

## People involved in development of the project
- [ScriptedSnark](https://github.com/ScriptedSnark)
- [\*(int\*)0 = 0xDEADBEEF](https://github.com/kvstepchenko)

## Special thanks
- [Sw1ft](https://github.com/sw1ft747) - for help in some questions and reversing as well
- anewnick (cdqke) - for RE logo in this README
