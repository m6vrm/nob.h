nob.h
=====

**No b**ullshit build system for C and C++.

Allows to write build system in language you already know without third-party tools like CMake, Make, shell.

Idea stolen from https://github.com/tsoding/nob.h.

Features
--------

*   Linux, macOS and Windows support
*   C and C++ support
*   Compilation database generation (`compile_commands.json`)
*   500 LOC

Usage
-----

    cd example
    cc -o nob nob.c
    ./nob
    ./build/example
