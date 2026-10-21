#!/bin/sh
set -eu
umask 077

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CONFIG=${DEPLOY_CONFIG:-"$ROOT/scripts/deploy-rgdsplus.conf"}
BINARY="$ROOT/emulationstation"
REMOTE_DIR="/storage/.config/emulationstation/bin"
REMOTE_SOURCE="$REMOTE_DIR/emulationstation"
REMOTE_HELPER="/storage/.config/autostart/mount_emulationstation.sh"

fail()
{
	printf 'Error: %s\n' "$1" >&2
	exit 1
}

[ -f "$CONFIG" ] || fail "Missing config: $CONFIG (copy deploy-rgdsplus.conf.example to deploy-rgdsplus.conf)"
chmod 600 "$CONFIG"
# This is a user-owned, git-ignored shell config. Keep it mode 600.
. "$CONFIG"

: "${DEVICE_IP:?Set DEVICE_IP in $CONFIG}"
: "${DEVICE_PASSWORD:?Set DEVICE_PASSWORD in $CONFIG}"

case "$DEVICE_IP" in
	''|*[!0-9.]*|.*|*.) fail "DEVICE_IP must be an IPv4 address" ;;
esac

for tool in ssh sshpass scp sha256sum awk date aarch64-linux-gnu-readelf; do
	command -v "$tool" >/dev/null 2>&1 || fail "Missing required tool: $tool"
done

[ -f "$BINARY" ] || fail "Build artifact not found: $BINARY (run scripts/build-rgdsplus.sh first)"
machine=$(aarch64-linux-gnu-readelf -h "$BINARY" | awk -F: '/Machine:/ { gsub(/^[[:space:]]+/, "", $2); print $2 }')
case "$machine" in
	*AArch64*) ;;
	*) fail "Build artifact is not AArch64: ${machine:-unknown architecture}" ;;
esac

BUILD_SHA256=$(sha256sum "$BINARY" | awk '{print $1}')
case "$BUILD_SHA256" in
	*[!0-9a-f]*|'') fail "Could not calculate build artifact SHA-256" ;;
esac

SSH_OPTIONS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -o PreferredAuthentications=password,keyboard-interactive -o PubkeyAuthentication=no"
ssh_device()
{
	SSHPASS=$DEVICE_PASSWORD sshpass -e ssh $SSH_OPTIONS "root@$DEVICE_IP" "$@"
}
scp_device()
{
	SSHPASS=$DEVICE_PASSWORD sshpass -e scp $SSH_OPTIONS "$@"
}

UPLOAD_ID="$(date +%Y%m%d-%H%M%S)-$$"
REMOTE_UPLOAD="$REMOTE_DIR/emulationstation.upload.$UPLOAD_ID"
UPLOAD_PENDING=false

cleanup()
{
	if [ "$UPLOAD_PENDING" = true ]; then
		ssh_device sh -s -- "$REMOTE_UPLOAD" <<'REMOTE_CLEANUP' >/dev/null 2>&1 || true
set -eu
rm -f -- "$1"
REMOTE_CLEANUP
	fi
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

printf 'Build artifact: %s\nSHA-256: %s\n' "$BINARY" "$BUILD_SHA256"

# Check the deployment identity and device environment before transferring.
ssh_device sh -s -- "$REMOTE_SOURCE" "$REMOTE_UPLOAD" "$REMOTE_HELPER" <<'REMOTE_PREFLIGHT'
set -eu
source=$1
upload=$2
helper=$3
[ -d "$(dirname "$source")" ] || { echo 'Deployment directory does not exist' >&2; exit 1; }
[ -f /usr/bin/emulationstation ] || { echo 'System EmulationStation executable is missing' >&2; exit 1; }
[ -x "$helper" ] || { echo 'Installed mount helper is missing or not executable' >&2; exit 1; }
[ ! -e "$upload" ] && [ ! -L "$upload" ] || { echo 'Upload path already exists; stopping' >&2; exit 1; }
if [ -e "$source" ] || [ -L "$source" ]; then
	[ -f "$source" ] || { echo 'Existing deployment is not a regular file; preserving it and stopping' >&2; exit 1; }
fi
systemctl is-active --quiet sway.service || { echo 'sway.service is not active; stopping before deployment' >&2; exit 1; }
REMOTE_PREFLIGHT

UPLOAD_PENDING=true
scp_device "$BINARY" "root@$DEVICE_IP:$REMOTE_UPLOAD"

# Verify the upload, replace the storage binary atomically, and refresh the
# runtime copy through the normal mount helper.
ssh_device sh -s -- "$BUILD_SHA256" "$REMOTE_UPLOAD" "$REMOTE_SOURCE" <<'REMOTE_DEPLOY'
set -eu
expected_build=$1
upload=$2
source=$3

actual_upload=$(sha256sum "$upload" | awk '{print $1}')
[ "$actual_upload" = "$expected_build" ] || { echo 'Uploaded binary SHA-256 mismatch; refusing replacement' >&2; exit 1; }
chmod +x "$upload"
if [ -e "$source" ] || [ -L "$source" ]; then
    [ -f "$source" ] || { echo 'Existing deployment is not a regular file; stopping' >&2; exit 1; }
fi
mv -f "$upload" "$source"

service_was_active=false
if systemctl is-active --quiet essway.service; then
	service_was_active=true
fi
"/storage/.config/autostart/mount_emulationstation.sh"
if [ "$service_was_active" = false ]; then
	systemctl start essway.service
fi

systemctl is-active --quiet essway.service || { echo 'essway.service is not active after mount' >&2; exit 1; }
systemctl is-active --quiet sway.service || { echo 'sway.service is not active after mount' >&2; exit 1; }
awk '$5 == "/usr/bin/emulationstation" { found = 1 } END { exit !found }' /proc/self/mountinfo || { echo 'EmulationStation bind mount is missing' >&2; exit 1; }

source_hash=$(sha256sum "$source" | awk '{print $1}')
target_hash=$(sha256sum /usr/bin/emulationstation | awk '{print $1}')
[ "$source_hash" = "$expected_build" ] && [ "$target_hash" = "$expected_build" ] || { echo 'Mounted executable SHA-256 mismatch' >&2; exit 1; }

running=0
for exe in /proc/[0-9]*/exe; do
	[ -e "$exe" ] || continue
	target=$(readlink "$exe" 2>/dev/null || true)
	case "$target" in
		/usr/bin/emulationstation*|/storage/.config/emulationstation/bin/emulationstation*|/storage/mount/emulationstation/emulationstation*)
			pid=${exe#/proc/}; pid=${pid%/exe}
			running_hash=$(sha256sum "/proc/$pid/exe" | awk '{print $1}')
			[ "$running_hash" = "$expected_build" ] || continue
			printf 'Running frontend PID %s SHA-256: %s\n' "$pid" "$running_hash"
			running=$((running + 1))
			;;
	esac
done
[ "$running" -gt 0 ] || { echo 'No running frontend uses the deployed executable' >&2; exit 1; }
printf 'Verified essway.service, sway.service, bind mount, and %s running frontend process(es)\n' "$running"
REMOTE_DEPLOY
UPLOAD_PENDING=false

printf 'Deployment complete. Build SHA-256 verified at the upload, mount, and running process.\n'
