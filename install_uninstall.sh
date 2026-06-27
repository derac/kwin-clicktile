#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$repo_root/.tmp/build"
effect_id="kwin_clicktile"
plugin_library="kwin_clicktile.so"
config_library="kwin_clicktile_config.so"
plugin_root="$(qtpaths6 --plugin-dir 2>/dev/null || printf '/usr/lib/qt6/plugins')"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

ensure_sudo() {
    require_command sudo
    if ! sudo -v; then
        printf 'Could not obtain sudo credentials; leaving KWin and installed files unchanged.\n' >&2
        exit 1
    fi
}

effect_loaded() {
    command -v qdbus6 >/dev/null 2>&1 &&
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded "$effect_id" 2>/dev/null |
            grep -qi '^true$'
}

path_present() {
    [ -e "$1" ] || [ -L "$1" ]
}

build_marker_for() {
    local path="$1"
    local marker

    if ! command -v strings >/dev/null 2>&1; then
        printf '<strings unavailable>'
        return
    fi

    if [ ! -f "$path" ]; then
        printf '<missing>'
        return
    fi

    marker="$(strings "$path" 2>/dev/null | grep -m1 'kwin-clicktile_build=' || true)"
    printf '%s' "${marker:-<missing>}"
}

metadata_status_for() {
    local path="$1"
    local output

    if ! command -v qtplugininfo6 >/dev/null 2>&1; then
        printf 'unchecked (qtplugininfo6 missing)'
        return
    fi

    if [ ! -f "$path" ]; then
        printf 'missing'
        return
    fi

    if output="$(qtplugininfo6 "$path" 2>&1)"; then
        printf 'ok'
    else
        printf 'invalid (%s)' "${output%%$'\n'*}"
    fi
}

kwin_loaded_status() {
    local output

    if ! command -v qdbus6 >/dev/null 2>&1; then
        printf 'unknown (qdbus6 missing)'
        return
    fi

    if output="$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded "$effect_id" 2>&1)"; then
        if printf '%s\n' "$output" | grep -qi '^true$'; then
            printf 'yes'
        elif printf '%s\n' "$output" | grep -qi '^false$'; then
            printf 'no'
        else
            printf 'unknown (%s)' "${output%%$'\n'*}"
        fi
    else
        printf 'unknown (%s)' "${output%%$'\n'*}"
    fi
}

installed_plugin_path() {
    printf '%s\n' "$plugin_root/kwin/effects/plugins/$plugin_library"
}

installed_config_path() {
    printf '%s\n' "$plugin_root/kwin/effects/configs/$config_library"
}

is_installed() {
    path_present "$(installed_plugin_path)" ||
        path_present "$(installed_config_path)" ||
        effect_loaded
}

unload_effect() {
    if ! command -v qdbus6 >/dev/null 2>&1; then
        return
    fi

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "$effect_id" >/dev/null 2>&1 || true
}

reconfigure_kwin() {
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
    fi

    if command -v qdbus6 >/dev/null 2>&1; then
        qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
    fi
}

load_effect() {
    reconfigure_kwin
    if ! command -v qdbus6 >/dev/null 2>&1; then
        printf 'qdbus6 not found; installed files but could not request KWin load.\n' >&2
        return
    fi

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect "$effect_id" >/dev/null 2>&1 || true
    if effect_loaded; then
        printf 'KWin reports %s loaded: true\n' "$effect_id"
    else
        printf 'KWin did not report %s loaded. Check KWin logs if System Settings also cannot enable it.\n' "$effect_id" >&2
    fi
}

verify_plugin_metadata() {
    local plugin="$1"

    require_command qtplugininfo6
    if ! qtplugininfo6 "$plugin" >/dev/null; then
        printf 'Built plugin has no readable Qt/KPlugin metadata: %s\n' "$plugin" >&2
        printf 'Refusing to install; System Settings would not be able to discover it.\n' >&2
        exit 1
    fi
}

build_project() {
    require_command cmake

    cmake -S "$repo_root" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "$build_dir" --parallel "$(nproc)"
    verify_plugin_metadata "$build_dir/$plugin_library"
    verify_plugin_metadata "$build_dir/$config_library"
}

verify_installed_build_marker() {
    local built_marker
    local installed_marker

    built_marker="$(build_marker_for "$build_dir/$plugin_library")"
    installed_marker="$(build_marker_for "$(installed_plugin_path)")"

    if [ "$built_marker" = '<missing>' ] || [ "$installed_marker" = '<missing>' ]; then
        return
    fi

    if [ "$built_marker" = "$installed_marker" ]; then
        printf 'Installed build marker: %s\n' "$installed_marker"
    else
        printf 'WARNING: installed build marker differs from built marker.\n' >&2
        printf '  built:     %s\n' "$built_marker" >&2
        printf '  installed: %s\n' "$installed_marker" >&2
    fi
}

install_effect() {
    ensure_sudo

    unload_effect
    build_project
    sudo cmake --install "$build_dir"
    load_effect
    verify_installed_build_marker

    cat <<EOF
kwin-clicktile installed.

Configure:
  System Settings > Window Management > Desktop Effects > kwin-clicktile

Logs:
  journalctl -b -f | grep -i kwin-clicktile

Run again to uninstall:
  "$repo_root/install_uninstall.sh"
EOF
}

remove_installed_files() {
    sudo rm -f -- "$(installed_plugin_path)" "$(installed_config_path)"
}

uninstall_effect() {
    ensure_sudo

    unload_effect
    remove_installed_files
    reconfigure_kwin

    cat <<EOF
kwin-clicktile uninstalled.

Run again to install:
  "$repo_root/install_uninstall.sh"
EOF
}

status_effect() {
    local built_plugin="$build_dir/$plugin_library"
    local built_config="$build_dir/$config_library"
    local installed_plugin
    local installed_config
    local built_marker
    local installed_marker

    installed_plugin="$(installed_plugin_path)"
    installed_config="$(installed_config_path)"
    built_marker="$(build_marker_for "$built_plugin")"
    installed_marker="$(build_marker_for "$installed_plugin")"

    printf 'kwin-clicktile status\n'
    printf 'repo: %s\n' "$repo_root"
    printf 'effect id: %s\n\n' "$effect_id"

    printf 'Build artifacts\n'
    if path_present "$built_plugin"; then
        printf '  effect plugin: present\n'
    else
        printf '  effect plugin: MISSING\n'
    fi
    printf '    path: %s\n' "$built_plugin"
    printf '    marker: %s\n' "$built_marker"
    printf '    metadata: %s\n' "$(metadata_status_for "$built_plugin")"

    if path_present "$built_config"; then
        printf '  config module: present\n'
    else
        printf '  config module: MISSING\n'
    fi
    printf '    path: %s\n' "$built_config"
    printf '    metadata: %s\n\n' "$(metadata_status_for "$built_config")"

    printf 'Installed artifacts\n'
    if path_present "$installed_plugin"; then
        printf '  effect plugin: present\n'
    else
        printf '  effect plugin: MISSING\n'
    fi
    printf '    path: %s\n' "$installed_plugin"
    printf '    marker: %s\n' "$installed_marker"
    printf '    metadata: %s\n' "$(metadata_status_for "$installed_plugin")"

    if path_present "$installed_config"; then
        printf '  config module: present\n'
    else
        printf '  config module: MISSING\n'
    fi
    printf '    path: %s\n' "$installed_config"
    printf '    metadata: %s\n\n' "$(metadata_status_for "$installed_config")"

    printf 'Runtime\n'
    printf '  KWin loaded: %s\n\n' "$(kwin_loaded_status)"

    printf 'Summary\n'
    if path_present "$installed_plugin" && path_present "$installed_config"; then
        printf '  installed files: complete\n'
    elif path_present "$installed_plugin" || path_present "$installed_config"; then
        printf '  installed files: incomplete\n'
    else
        printf '  installed files: absent\n'
    fi

    if [ "$built_marker" != '<missing>' ] && [ "$installed_marker" != '<missing>' ] && [ "$built_marker" != "$installed_marker" ]; then
        printf '  version mismatch: built %s, installed %s\n' "$built_marker" "$installed_marker"
        printf '  suggested action: %s --install\n' "$0"
    else
        printf '  version mismatch: none\n'
    fi
}

usage() {
    cat <<EOF
Usage: $0 [--install|--uninstall|--status]

Without an argument, this toggles based on the current machine state:
  installed   -> uninstall
  not present -> build, install, and load
EOF
}

case "${1:-}" in
    --install)
        install_effect
        ;;
    --uninstall)
        uninstall_effect
        ;;
    --status)
        status_effect
        ;;
    -h|--help)
        usage
        ;;
    "")
        if is_installed; then
            uninstall_effect
        else
            install_effect
        fi
        ;;
    *)
        printf 'Unknown argument: %s\n' "$1" >&2
        usage >&2
        exit 1
        ;;
esac
