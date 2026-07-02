#!/bin/bash
set -e

ROOT="$1"

mkdir -p "$ROOT/bin"

clang++ \
    -std=c++17 \
    -g \
    "$ROOT"/src/*.cpp \
    "$ROOT"/src/*.c \
    -I"$ROOT/include" \
    -I/opt/homebrew/include \
    $(pkg-config --cflags --libs glfw3) \
    -framework OpenGL \
    -o "$ROOT/bin/main"