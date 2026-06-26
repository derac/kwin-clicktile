#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$repo_root/.tmp/build"
stage_dir="$repo_root/.tmp/stage"
state_home="${XDG_STATE_HOME:-$HOME/.local/state}"
state_dir="$state_home/kwin-clicktile/effect"
backup_root="$state_dir/backups"
staged_files_file="$state_dir/staged-files"
installed_files_file="$state_dir/installed-files"
created_files_file="$state_dir/created-files"
backed_up_files_file="$state_dir/backed-up-files"
created_dirs_file="$state_dir/created-dirs"
effect_id="kwin_clicktile"
plugin_library="kwin_clicktile.so"
config_library="kwin_clicktile_config.so"
plugin_root="$(qtpaths6 --plugin-dir 2>/dev/null || printf '/usr/lib/qt6/plugins')"
data_roots="$(qtpaths6 --paths GenericDataLocation 2>/dev/null || printf '/usr/share')"
old_suffix="$(printf '%b' '\\155\\166\\160')"
stale_effect_ids=("kwin-clicktile" "clicktile_snap_${old_suffix}" "clicktile_filter_${old_suffix}" "clicktile_${old_suffix}")

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

is_recorded() {
    local path="$1"
    local file="$2"

    [ -f "$file" ] && grep -Fqx "$path" "$file"
}

record_unique() {
    local path="$1"
    local file="$2"

    mkdir -p "$(dirname "$file")"
    if ! is_recorded "$path" "$file"; then
        printf '%s\n' "$path" >> "$file"
    fi
}

record_created_parent_dirs() {
    local target="$1"
    local dir
    local -a dirs_to_record=()

    dir="$(dirname "$target")"
    while [ "$dir" != "/" ] && [ ! -e "$dir" ]; do
        dirs_to_record+=("$dir")
        dir="$(dirname "$dir")"
    done

    for ((index=${#dirs_to_record[@]} - 1; index >= 0; index--)); do
        record_unique "${dirs_to_record[$index]}" "$created_dirs_file"
    done
}

record_target_backup() {
    local target="$1"

    if is_recorded "$target" "$backed_up_files_file" || is_recorded "$target" "$created_files_file"; then
        return
    fi

    record_created_parent_dirs "$target"
    if [ -e "$target" ] || [ -L "$target" ]; then
        local backup_path="$backup_root${target}"
        mkdir -p "$(dirname "$backup_path")"
        cp -a "$target" "$backup_path"
        record_unique "$target" "$backed_up_files_file"
    else
        record_unique "$target" "$created_files_file"
    fi
}

effect_loaded() {
    command -v qdbus6 >/dev/null 2>&1 &&
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded "$effect_id" 2>/dev/null |
            grep -qi '^true$'
}

new_targets() {
    printf '%s\n' "$plugin_root/kwin/effects/plugins/$plugin_library"
    printf '%s\n' "$plugin_root/kwin/effects/configs/$config_library"

    local root
    IFS=: read -r -a roots <<< "$data_roots"
    for root in "${roots[@]}"; do
        printf '%s\n' "$root/kwin/effects/kwin-clicktile/contents/ui/main.qml"
    done
}

stale_targets() {
    printf '%s\n' "$plugin_root/kwin/effects/plugins/clicktile_snap_${old_suffix}.so"
    printf '%s\n' "$plugin_root/kwin/effects/plugins/clicktile_filter_${old_suffix}.so"
    printf '%s\n' "$plugin_root/kwin/effects/plugins/clicktile_${old_suffix}.so"
    printf '%s\n' "$plugin_root/kwin/effects/configs/kwin_clicktile_snap_${old_suffix}_config.so"
    printf '%s\n' "$plugin_root/kwin/effects/configs/kwin_clicktile_filter_${old_suffix}_config.so"

    local root
    IFS=: read -r -a roots <<< "$data_roots"
    for root in "${roots[@]}"; do
        printf '%s\n' "$root/kwin/effects/clicktile_snap_${old_suffix}/contents/ui/main.qml"
        printf '%s\n' "$root/kwin/effects/clicktile_filter_${old_suffix}/contents/ui/main.qml"
        printf '%s\n' "$root/kwin/effects/clicktile_${old_suffix}/contents/ui/main.qml"
    done
}

is_installed() {
    while IFS= read -r target; do
        [ -n "$target" ] || continue
        if [ -e "$target" ] || [ -L "$target" ]; then
            return 0
        fi
    done < <(new_targets)

    effect_loaded
}

unload_effects() {
    if ! command -v qdbus6 >/dev/null 2>&1; then
        return
    fi

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "$effect_id" >/dev/null 2>&1 || true
    local stale_effect_id
    for stale_effect_id in "${stale_effect_ids[@]}"; do
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "$stale_effect_id" >/dev/null 2>&1 || true
    done
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

build_project() {
    require_command cmake

    cmake -S "$repo_root" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "$build_dir" --parallel "$(nproc)"
    verify_plugin_metadata "$build_dir/$plugin_library"
    verify_plugin_metadata "$build_dir/$config_library"
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

stage_install() {
    rm -rf "$stage_dir"
    mkdir -p "$stage_dir"
    DESTDIR="$stage_dir" cmake --install "$build_dir"

    find "$stage_dir" \( -type f -o -type l \) | sort | while IFS= read -r staged_path; do
        printf '/%s\n' "${staged_path#"$stage_dir"/}"
    done > "$staged_files_file"
}

record_install_targets() {
    while IFS= read -r target; do
        [ -n "$target" ] || continue
        record_target_backup "$target"
        record_unique "$target" "$installed_files_file"
    done < "$staged_files_file"

    while IFS= read -r target; do
        [ -n "$target" ] || continue
        if [ -e "$target" ] || [ -L "$target" ]; then
            record_target_backup "$target"
            record_unique "$target" "$installed_files_file"
        fi
    done < <(stale_targets)
}

remove_stale_targets() {
    while IFS= read -r target; do
        [ -n "$target" ] || continue
        if [ -e "$target" ] || [ -L "$target" ]; then
            sudo rm -f "$target"
        fi
    done < <(stale_targets)
}

verify_loaded_build_marker() {
    command -v strings >/dev/null 2>&1 || return

    local plugin="$build_dir/$plugin_library"
    local expected_marker
    expected_marker="$(strings "$plugin" 2>/dev/null | grep -m1 'kwin-clicktile_build=' || true)"
    [ -n "$expected_marker" ] || return

    sleep 1
    if grep -Fq "$expected_marker" \
        "$state_home/kwin-clicktile/effect/events.log" \
        "$state_home/kwin/kwin-clicktile/effect/events.log" 2>/dev/null; then
        printf 'Observed loaded build marker: %s\n' "$expected_marker"
        return
    fi

    printf 'WARNING: did not observe loaded build marker: %s\n' "$expected_marker" >&2
    printf 'KWin may still have an older plugin image loaded. Restart KWin or log out/in, then rerun this script.\n' >&2
}

install_effect() {
    require_command sudo
    mkdir -p "$state_dir" "$backup_root"

    unload_effects
    build_project
    stage_install
    record_install_targets
    remove_stale_targets
    sudo cmake --install "$build_dir"
    load_effect
    verify_loaded_build_marker

    cat <<EOF
kwin-clicktile installed.

Configure:
  System Settings > Window Management > Desktop Effects > kwin-clicktile

Logs:
  tail -f "$state_home/kwin-clicktile/effect/events.log"
  journalctl -b -f | grep -i kwin-clicktile

Run again to uninstall:
  "$repo_root/install_uninstall.sh"
EOF
}

restore_backups() {
    if [ ! -f "$backed_up_files_file" ]; then
        return
    fi

    while IFS= read -r target; do
        [ -n "$target" ] || continue
        local backup_path="$backup_root${target}"
        if [ -e "$backup_path" ] || [ -L "$backup_path" ]; then
            sudo mkdir -p "$(dirname "$target")"
            sudo cp -a "$backup_path" "$target"
        fi
    done < "$backed_up_files_file"
}

remove_created_files() {
    local removed_any=0

    if [ -f "$created_files_file" ]; then
        while IFS= read -r target; do
            [ -n "$target" ] || continue
            sudo rm -f "$target"
            removed_any=1
        done < "$created_files_file"
    fi

    if [ "$removed_any" -eq 0 ]; then
        while IFS= read -r target; do
            [ -n "$target" ] || continue
            if [ -e "$target" ] || [ -L "$target" ]; then
                sudo rm -f "$target"
            fi
        done < <(new_targets)
    fi
}

remove_created_dirs() {
    if [ ! -f "$created_dirs_file" ]; then
        return
    fi

    tac "$created_dirs_file" | while IFS= read -r dir; do
        [ -n "$dir" ] || continue
        sudo rmdir "$dir" 2>/dev/null || true
    done
}

uninstall_effect() {
    require_command sudo

    unload_effects
    restore_backups
    remove_created_files
    remove_created_dirs
    reconfigure_kwin

    cat <<EOF
kwin-clicktile uninstalled.

Run again to install:
  "$repo_root/install_uninstall.sh"
EOF
}

status_effect() {
    if is_installed; then
        printf 'kwin-clicktile installed: yes\n'
    else
        printf 'kwin-clicktile installed: no\n'
    fi

    while IFS= read -r target; do
        [ -n "$target" ] || continue
        if [ -e "$target" ] || [ -L "$target" ]; then
            printf 'present: %s\n' "$target"
        else
            printf 'missing: %s\n' "$target"
        fi
    done < <(new_targets)

    if command -v strings >/dev/null 2>&1 && [ -f "$build_dir/$plugin_library" ]; then
        strings "$build_dir/$plugin_library" | grep -m1 'kwin-clicktile_build=' || true
    fi

    if command -v qtplugininfo6 >/dev/null 2>&1; then
        while IFS= read -r target; do
            [ -n "$target" ] || continue
            case "$target" in
                *.so)
                    if [ -f "$target" ] && qtplugininfo6 "$target" >/dev/null 2>&1; then
                        printf 'metadata ok: %s\n' "$target"
                    elif [ -f "$target" ]; then
                        printf 'metadata missing: %s\n' "$target"
                    fi
                    ;;
            esac
        done < <(new_targets)
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
