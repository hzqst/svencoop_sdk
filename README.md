# svencoop_sdk <img src="media/re_sven1.png" width="120" align="right">

Reverse-engineered source code of Sven Co-op (5.26), current Steam build.

This repository contains:
- `client.dll` — client code (5.26)
- `server.dll` — server code (5.26)

## Getting started

Initialize some dependencies after cloning:

```sh
git submodule update --init --recursive
```

## Building

### Visual Studio 2019

Install `v142` toolset, Windows 10 SDK and open:

```text
projects/vs2010/svencoop.sln
```

Select `Win32` and `Debug` or `Release`, then build the solution.

## People involved in development of the project
- [ScriptedSnark](https://github.com/ScriptedSnark)
- [\*(int\*)0 = 0xDEADBEEF](https://github.com/kvstepchenko)

## Special thanks
- [Sw1ft](https://github.com/sw1ft747) - for help in some questions and reversing as well