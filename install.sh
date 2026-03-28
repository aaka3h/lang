#!/bin/bash
# Lang installer
# Usage: curl -fsSL https://raw.githubusercontent.com/aaka3h/lang/main/install.sh | bash

set -e

REPO="https://github.com/aaka3h/lang"
RAW="https://raw.githubusercontent.com/aaka3h/lang/main"
INSTALL_DIR="/usr/local/bin"
BUILD_DIR="/tmp/lang-install-$$"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}"
echo "  _                 "
echo " | |    __ _ _ __  __ _ "
echo " | |   / _\` | '_ \/ _\` |"
echo " | |__| (_| | | | | (_| |"
echo " |_____\__,_|_| |_|\__, |"
echo "                    |___/ "
echo -e "${NC}"
echo "Installing Lang programming language..."
echo ""

# Check dependencies
check_dep() {
    if ! command -v "$1" &> /dev/null; then
        echo -e "${RED}Error: $1 is required but not installed.${NC}"
        exit 1
    fi
}

check_dep gcc
check_dep git
check_dep make

# Clone repo
echo "Cloning repository..."
mkdir -p "$BUILD_DIR"
git clone --depth=1 "$REPO" "$BUILD_DIR/lang" 2>/dev/null
cd "$BUILD_DIR/lang"

# Build
echo "Building..."
gcc -Wall -std=c99 -O2 \
    lexer.c parser.c interp.c compiler.c vm_impl.c main.c \
    -o lang -lm

# Install
echo "Installing to $INSTALL_DIR..."
if [ -w "$INSTALL_DIR" ]; then
    cp lang "$INSTALL_DIR/lang"
else
    sudo cp lang "$INSTALL_DIR/lang"
fi

# Cleanup
cd /
rm -rf "$BUILD_DIR"

# Verify
if command -v lang &> /dev/null; then
    echo ""
    echo -e "${GREEN}Lang installed successfully!${NC}"
    echo ""
    echo "Try it:"
    echo '  echo '"'"'print "Hello, World!"'"'"' | lang'
    echo "  lang --help"
    echo ""
else
    echo -e "${RED}Installation failed.${NC}"
    exit 1
fi
