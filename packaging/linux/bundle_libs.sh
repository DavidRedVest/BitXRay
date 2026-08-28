#!/usr/bin/env bash
# Copies every non-core-system shared library the executable depends on
# (transitively, via ldd) into a lib/ directory next to it, so the resulting
# directory runs on a machine that doesn't have Qt/FFmpeg installed. Meant to
# be paired with a launcher script that points LD_LIBRARY_PATH at that lib/
# directory (see packaging/linux/bitxray_ui.sh).
#
# This is a portable-directory approach, not a real package (no AppImage/deb
# integration yet) — see the Linux note in README.md's Downloads section.
#
# Usage: bundle_libs.sh /path/to/bitxray_ui /path/to/output/lib
set -euo pipefail

BIN="$1"
LIBDIR="$2"
mkdir -p "$LIBDIR"

# Core glibc/loader pieces: version-sensitive and expected to already be
# present (and compatible) on any target machine's own system — bundling a
# mismatched copy of these causes much worse breakage than not bundling them.
EXCLUDE_REGEX='^(linux-vdso\.so|ld-linux|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|libresolv\.so)'

collect() {
    local target="$1" so base dest
    while IFS= read -r so; do
        [[ -z "$so" ]] && continue
        base=$(basename "$so")
        [[ "$base" =~ $EXCLUDE_REGEX ]] && continue
        dest="$LIBDIR/$base"
        if [[ ! -f "$dest" ]]; then
            cp -L "$so" "$dest"
            collect "$dest"
        fi
    done < <(ldd "$target" | awk '{print $3}' | grep -E '^/')
}

collect "$BIN"
echo "Bundled $(find "$LIBDIR" -type f | wc -l | tr -d ' ') shared libraries into $LIBDIR"
