#!/bin/sh
# Launcher for the portable Linux distribution: points the dynamic linker at
# the bundled lib/ directory, and Qt at the bundled xcb platform plugin,
# before exec'ing the real binary.
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
exec "$DIR/bin/bitxray_ui" "$@"
