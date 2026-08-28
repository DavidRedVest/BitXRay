#!/usr/bin/env bash
# Embeds every non-system dylib the app's main executable depends on
# (transitively) into <App>.app/Contents/Frameworks, rewriting install names
# to @rpath and adding an @executable_path/../Frameworks rpath so the bundle
# runs standalone without the FFmpeg dev tree (or Homebrew prefix) it was
# built against being present on the target machine.
#
# Run *after* macdeployqt (which handles the Qt frameworks/plugins) and
# *before* codesign (this rewrites install names, which invalidates any
# existing signature).
#
# Usage: bundle_ffmpeg.sh /path/to/BitXRay.app
set -euo pipefail

APP="$1"
FRAMEWORKS="$APP/Contents/Frameworks"
EXE=$(find "$APP/Contents/MacOS" -maxdepth 1 -type f | head -1)
mkdir -p "$FRAMEWORKS"

# LC_RPATH entries of a Mach-O file, one per line.
rpaths_of() {
    otool -l "$1" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" { want = 1; next }
        want && $1 == "path" { print $2; want = 0 }
    '
}

# Resolves an "@rpath/libfoo.dylib" reference seen in $1's load commands to
# an absolute path on disk, by trying $1's own LC_RPATH entries.
resolve_rpath_dep() {
    local referencing="$1" dep="$2" name dir
    name="${dep#@rpath/}"
    while IFS= read -r dir; do
        [[ -z "$dir" ]] && continue
        if [[ -f "$dir/$name" ]]; then
            echo "$dir/$name"
            return 0
        fi
    done < <(rpaths_of "$referencing")
    return 1
}

# Bash 3.2 (macOS's /bin/bash, and what CI runners default to) has no
# associative arrays — track "already embedded" via the destination file's
# existence instead.
queue=("$EXE")
embedded_count=0

while [[ ${#queue[@]} -gt 0 ]]; do
    cur="${queue[0]}"
    queue=("${queue[@]:1}")

    while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        case "$dep" in
            /usr/lib/*|/System/*) continue ;;
            @rpath/Qt*) continue ;; # already handled by macdeployqt
        esac

        local_path=""
        case "$dep" in
            @rpath/*)
                local_path=$(resolve_rpath_dep "$cur" "$dep") || {
                    echo "warning: could not resolve $dep referenced by $cur" >&2
                    continue
                }
                ;;
            /*)
                local_path="$dep"
                ;;
            *)
                continue ;;
        esac

        base=$(basename "$local_path")
        dest="$FRAMEWORKS/$base"
        if [[ ! -f "$dest" ]]; then
            cp -L "$local_path" "$dest"
            chmod u+w "$dest"
            install_name_tool -id "@rpath/$base" "$dest"
            embedded_count=$((embedded_count + 1))
            queue+=("$dest")
        fi
        install_name_tool -change "$dep" "@rpath/$base" "$cur"
    done < <(otool -L "$cur" | tail -n +2 | awk '{print $1}')
done

# Belt-and-suspenders: the main executable already has this rpath (added by
# macdeployqt), which is enough for dyld to resolve @rpath deps anywhere in
# the load chain — but add it to each embedded lib too in case one is ever
# reached by a path that doesn't go through the main executable's rpath set.
for f in "$FRAMEWORKS"/*; do
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$f" 2>/dev/null || true
done

# Strip any leftover absolute-path LC_RPATH entries (e.g. from the build
# machine's FFmpeg dev tree via CMake's BUILD_RPATH) from the executable and
# every embedded lib — they're meaningless on another machine, and on this
# one they'd let dyld silently resolve @rpath deps from the build tree
# instead of the copies just embedded, defeating the point of embedding them.
for f in "$EXE" "$FRAMEWORKS"/*; do
    [[ -f "$f" ]] || continue
    while IFS= read -r rp; do
        [[ "$rp" == /* ]] && install_name_tool -delete_rpath "$rp" "$f" 2>/dev/null || true
    done < <(rpaths_of "$f")
done

echo "Embedded $embedded_count non-system dylib(s) into $FRAMEWORKS"
