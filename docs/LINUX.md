# Linux

Preset `x64-linux-release` uses the registry triplet `x64-linux-release`.

The Carbon registry pin is branch `fe4/core-lane-b-modernized` at `a412b804ced9a381efea7487b982b53d13ca5ab1`. This vcpkg tool reads the default branch when `reference` is omitted, so the branch name is part of the pin. That branch is not registry `main`.

`StdAfx.h` and `tests/StdAfx.h` include `<cstring>`. GCC 15 did not declare `memcpy`, `memset`, `memcmp`, `memmove`, or `strncmp` without it. No public header changed.

OpenVDB stays `10.0.0#1` with `nanovdb-nocuda`. The local closure fit in the remaining disk cap. The local run passed 331 tests. `PngHandler.CanSaveRgPng` stayed disabled.
