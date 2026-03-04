#!/bin/bash
set -e

REPO_URL="https://github.com/USER/vml.git"
PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="build"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[VML]${NC} $1"; }
warn()  { echo -e "${YELLOW}[VML]${NC} $1"; }
error() { echo -e "${RED}[VML]${NC} $1"; exit 1; }

install_deps() {
    info "Installing dependencies..."
    if command -v pacman &>/dev/null; then
        sudo pacman -S --needed --noconfirm qt6-base pipewire cmake gcc
    elif command -v apt &>/dev/null; then
        sudo apt update -qq
        sudo apt install -y qt6-base-dev libpipewire-0.3-dev libspa-0.2-dev cmake g++ pkg-config
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y qt6-qtbase-devel pipewire-devel cmake gcc-c++ pkg-config
    else
        warn "Unknown package manager. Install manually: Qt6, PipeWire dev headers, CMake, C++20 compiler"
    fi
}

find_source_dir() {
    # If run from inside the repo, use that
    if [[ -f "CMakeLists.txt" ]]; then
        SOURCE_DIR="$(pwd)"
    elif [[ -f "$(dirname "$0")/../CMakeLists.txt" ]]; then
        SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    else
        # Piped from curl — clone into temp dir
        info "Cloning VML..."
        SOURCE_DIR="$(mktemp -d)/vml"
        git clone --depth 1 "$REPO_URL" "$SOURCE_DIR"
    fi
}

do_build() {
    info "Building VML..."
    cd "$SOURCE_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
    make -j"$(nproc)"
    cd ..
}

do_install() {
    info "Installing VML to $PREFIX..."
    cd "$SOURCE_DIR/$BUILD_DIR"
    sudo make install
    info "Installed successfully!"
    info "Run 'vml' to start Voice Modulation for Linux."
}

do_uninstall() {
    info "Uninstalling VML..."
    sudo rm -f "$PREFIX/bin/vml"
    sudo rm -f /usr/share/applications/vml.desktop
    sudo rm -f /usr/share/icons/hicolor/scalable/apps/vml-icon.svg
    info "Uninstalled."
}

case "${1:-}" in
    --uninstall)
        do_uninstall
        ;;
    --deps-only)
        install_deps
        ;;
    --build-only)
        find_source_dir
        do_build
        ;;
    *)
        install_deps
        find_source_dir
        do_build
        do_install
        ;;
esac
