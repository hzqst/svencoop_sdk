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

### Visual Studio 2019

Install `v142` (VS2019) toolset, Windows 10 SDK and open:

```text
projects/vs2010/svencoop.sln
```

Select `Win32` and `Debug` or `Release`, then build the solution.

### Linux (Steam Runtime)

Install Docker and make sure your user can access its daemon. Then, from repo root, build client and server inside Valve's Scout SDK:

```sh
./linux/build.sh debug
./linux/build.sh release
```

## People involved in development of the project
- [ScriptedSnark](https://github.com/ScriptedSnark)
- [\*(int\*)0 = 0xDEADBEEF](https://github.com/kvstepchenko)

## Special thanks
- [Sw1ft](https://github.com/sw1ft747) - for help in some questions and reversing as well
- anewnick (cdqke) - for RE logo in this README
