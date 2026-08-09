## Building examples
UNIX: 
```
g++ <EXAMPLE.cpp> -o <OUT_NAME> -Isrc -lncurses

// To build a TUI example that uses ncurses:
g++ <EXAMPLE.cpp> -o <OUT_NAME> -Isrc -lncurses
```

Windows:
> [!NOTE]
> It is recommended to build and run using ***MSYS2*** or some other POSIX-like environment with ***MinGW-w64*** toolchain installed 
```
g++ <EXAMPLE.cpp> -o <OUT_NAME> -Isrc -lws2_32

// To build a TUI example that uses ncurses:
g++ <EXAMPLE.cpp> -o <OUT_NAME> -Isrc -lws2_32 -lncurses -DNCURSES_STATIC
```

