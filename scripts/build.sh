#!/bin/bash

###############################################################################
# Build Script untuk mod_live_call
# 
# Usage:
#   ./scripts/build.sh [clean|build|install|uninstall|rebuild]
###############################################################################

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default paths
FS_INCLUDES=${FS_INCLUDES:-/usr/local/freeswitch/include}
FS_MODULES=${FS_MODULES:-/usr/local/freeswitch/mod}
FS_CONF=${FS_CONF:-/usr/local/freeswitch/conf}

# Functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check for compiler
    if ! command -v gcc &> /dev/null; then
        log_error "GCC not found. Please install build-essential"
        exit 1
    fi
    
    # Check for FreeSWITCH headers
    if [ ! -d "$FS_INCLUDES" ]; then
        log_error "FreeSWITCH headers not found at $FS_INCLUDES"
        log_error "Please install FreeSWITCH development files or set FS_INCLUDES"
        exit 1
    fi
    
    # Check for libwebsockets
    if ! pkg-config --exists libwebsockets; then
        log_warn "libwebsockets not found via pkg-config"
        log_warn "Attempting to compile anyway..."
    fi
    
    log_info "Dependencies OK"
}

clean() {
    log_info "Cleaning build artifacts..."
    make clean
    log_info "Clean complete"
}

build() {
    log_info "Building mod_live_call..."
    check_dependencies
    
    # Export paths for Makefile
    export FS_INCLUDES
    
    make
    log_info "Build complete"
}

install() {
    log_info "Installing mod_live_call..."
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        log_warn "Installation typically requires root privileges"
        log_warn "You may need to run: sudo ./scripts/build.sh install"
    fi
    
    # Install module
    export FS_MODULES
    export FS_CONF
    make install
    
    log_info "Module installed to $FS_MODULES"
    log_info "Config installed to $FS_CONF/autoload_configs"
    
    # Check if module is in modules.conf.xml
    if [ -f "$FS_CONF/autoload_configs/modules.conf.xml" ]; then
        if ! grep -q "mod_live_call" "$FS_CONF/autoload_configs/modules.conf.xml"; then
            log_warn "mod_live_call not found in modules.conf.xml"
            log_info "Add this line to $FS_CONF/autoload_configs/modules.conf.xml:"
            echo '    <load module="mod_live_call"/>'
        fi
    fi
    
    log_info "Installation complete"
    log_info ""
    log_info "Next steps:"
    log_info "  1. Add <load module=\"mod_live_call\"/> to modules.conf.xml"
    log_info "  2. Configure $FS_CONF/autoload_configs/live_call.conf.xml"
    log_info "  3. Reload FreeSWITCH or run: fs_cli -x 'reload mod_live_call'"
}

uninstall() {
    log_info "Uninstalling mod_live_call..."
    
    if [ "$EUID" -ne 0 ]; then
        log_warn "Uninstallation typically requires root privileges"
        log_warn "You may need to run: sudo ./scripts/build.sh uninstall"
    fi
    
    export FS_MODULES
    export FS_CONF
    make uninstall
    
    log_info "Uninstallation complete"
}

rebuild() {
    clean
    build
}

show_help() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  clean      - Remove build artifacts"
    echo "  build      - Compile the module (default)"
    echo "  install    - Install module to FreeSWITCH"
    echo "  uninstall  - Remove module from FreeSWITCH"
    echo "  rebuild    - Clean and build"
    echo "  help       - Show this help"
    echo ""
    echo "Environment variables:"
    echo "  FS_INCLUDES - Path to FreeSWITCH headers (default: /usr/local/freeswitch/include)"
    echo "  FS_MODULES  - Path to FreeSWITCH modules (default: /usr/local/freeswitch/mod)"
    echo "  FS_CONF     - Path to FreeSWITCH config (default: /usr/local/freeswitch/conf)"
    echo ""
    echo "Examples:"
    echo "  $0 build"
    echo "  sudo $0 install"
    echo "  FS_INCLUDES=/opt/freeswitch/include $0 build"
}

# Main
case "${1:-build}" in
    clean)
        clean
        ;;
    build)
        build
        ;;
    install)
        build
        install
        ;;
    uninstall)
        uninstall
        ;;
    rebuild)
        rebuild
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        log_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
