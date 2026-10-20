#!/bin/sh
set -eu

source_bin="/storage/.config/emulationstation/bin/emulationstation"
target_bin="/usr/bin/emulationstation"
frontend_service="essway.service"

frontend_pids()
{
    for exe in /proc/[0-9]*/exe; do
        [ -e "$exe" ] || continue

        process_exe=$(readlink "$exe" 2>/dev/null || true)
        case "$process_exe" in
            "$target_bin"|"$target_bin (deleted)"|"$source_bin"|"$source_bin (deleted)")
                pid=${exe#/proc/}
                printf '%s ' "${pid%/exe}"
                ;;
        esac
    done
}

[ -f "$source_bin" ] || exit 0
chmod +x "$source_bin"

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

if [ ! -f "$target_bin" ]; then
    printf 'System executable missing: %s\n' "$target_bin" >&2
    exit 1
fi

mount --bind "$source_bin" "$target_bin"
printf 'Mounted %s at %s\n' "$source_bin" "$target_bin"
