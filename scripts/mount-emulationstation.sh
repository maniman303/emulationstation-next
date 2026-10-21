#!/bin/sh
set -eu

source_dir="/storage/.config/emulationstation/bin"
source_bin="$source_dir/emulationstation"
runtime_dir="/storage/mount/emulationstation"
runtime_bin="$runtime_dir/emulationstation"
target_bin="/usr/bin/emulationstation"
frontend_service="essway.service"

frontend_pids()
{
    for exe in /proc/[0-9]*/exe; do
        [ -e "$exe" ] || continue

        process_exe=$(readlink "$exe" 2>/dev/null || true)
        case "$process_exe" in
            "$target_bin"|"$target_bin (deleted)"|"$source_bin"|"$source_bin (deleted)"|"$runtime_bin"|"$runtime_bin (deleted)")
                pid=${exe#/proc/}
                printf '%s ' "${pid%/exe}"
                ;;
        esac
    done
}

service_was_running=false
if systemctl is-active --quiet "$frontend_service"; then
    service_was_running=true
fi

restart_frontend()
{
    status=$?
    trap - EXIT

    if [ "$service_was_running" = true ] && ! systemctl start "$frontend_service"; then
        printf 'Failed to restart %s after updating the EmulationStation mount\n' "$frontend_service" >&2
        [ "$status" -ne 0 ] || status=1
    fi

    exit "$status"
}

trap restart_frontend EXIT

if [ "$service_was_running" = true ]; then
    systemctl stop "$frontend_service"
fi

wait_count=0
while :; do
    running_pids=$(frontend_pids)
    service_state=$(systemctl is-active "$frontend_service" 2>/dev/null || true)

    if [ -z "$running_pids" ] && [ "$service_state" != active ]; then
        break
    fi

    printf 'Waiting for EmulationStation to stop (PIDs:%s service:%s)\n' \
        "${running_pids:-none}" "${service_state:-unknown}"

    if [ "$wait_count" -ge 20 ]; then
        printf 'Timed out waiting for EmulationStation to stop (PIDs:%s service:%s)\n' \
            "$running_pids" "$service_state" >&2
        exit 1
    fi

    wait_count=$((wait_count + 1))
    sleep 0.5
done

printf 'EmulationStation stopped; updating executable mount\n'

# Clear any previous bind mount, including one pointing at a replaced executable.
while awk -v target="$target_bin" '
    $5 == target { found = 1 }
    END { exit !found }
' /proc/self/mountinfo; do
    umount "$target_bin"
done

rm -rf -- "$runtime_dir"

if [ ! -d "$source_dir" ]; then
    printf 'Deployment directory missing: %s; using the system executable\n' "$source_dir"
    exit 0
fi

if [ ! -f "$target_bin" ]; then
    printf 'System executable missing: %s\n' "$target_bin" >&2
    exit 1
fi

if [ ! -f "$source_bin" ]; then
    printf 'Deployed executable missing: %s; using the system executable\n' "$source_bin"
    exit 0
fi

mkdir -p "$runtime_dir"
if ! cp -p "$source_bin" "$runtime_bin"; then
    rm -rf -- "$runtime_dir"
    printf 'Failed to copy deployed executable to %s\n' "$runtime_bin" >&2
    exit 1
fi
chmod +x "$runtime_bin"
mount --bind "$runtime_bin" "$target_bin"
printf 'Mounted %s at %s\n' "$runtime_bin" "$target_bin"
