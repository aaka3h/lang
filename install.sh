#!/bin/bash
set -e
REPO="https://github.com/aaka3h/lang"
BUILD_DIR="/tmp/lang-install-$$"
echo "Installing Lang..."
git clone --depth=1 "$REPO" "$BUILD_DIR" 2>/dev/null
cd "$BUILD_DIR"
gcc -Wall -std=c99 -O2 lexer.c parser.c interp.c compiler.c vm_impl.c main.c -o lang -lm
sudo cp lang /usr/local/bin/lang
cd / && rm -rf "$BUILD_DIR"
echo "Lang installed! Run: lang"
