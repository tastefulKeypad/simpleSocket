## Building examples with CMake
[![Build Linux](https://github.com/tastefulKeypad/simpleFramework/actions/workflows/linux-build-examples.yml/badge.svg)](https://github.com/tastefulKeypad/simpleFramework/actions/workflows/linux-build-examples.yml/badge.svg)
[![Build Windows](https://github.com/tastefulKeypad/simpleFramework/actions/workflows/windows-build-examples.yml/badge.svg)](https://github.com/tastefulKeypad/simpleFramework/actions/workflows/windows-build-examples.yml/badge.svg)
<br>In order to build examples from project's root directory, run:
```
cmake -B build && cmake --build build
```

> [!NOTE]
> Some examples use 'ncurses' library to implement a simple TUI application
> <br>To build & run those examples under **Windows** you must use ***MSYS2*** or some other POSIX-like environment with ***MinGW-w64*** toolchain installed

