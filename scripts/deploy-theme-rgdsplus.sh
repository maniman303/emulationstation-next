#!/bin/sh
set -eu
umask 077

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
CONFIG=${DEPLOY_CONFIG:-"$ROOT/scripts/deploy-rgdsplus.conf"}
THEME="$ROOT/themes/art-book-ds"
REMOTE_THEMES="/storage/.config/emulationstation/themes"

fail()
{
	printf 'Error: %s\n' "$1" >&2
	exit 1
}

[ -f "$CONFIG" ] || fail "Missing config: $CONFIG (copy scripts/deploy-rgdsplus.conf.example first)"
chmod 600 "$CONFIG"
# Reuse the private, git-ignored connection settings from binary deployment.
# shellcheck source=/dev/null
. "$CONFIG"
: "${DEVICE_IP:?Set DEVICE_IP in $CONFIG}"
: "${DEVICE_PASSWORD:?Set DEVICE_PASSWORD in $CONFIG}"
case "$DEVICE_IP" in
	''|*[!0-9.]*|.*|*.) fail "DEVICE_IP must be an IPv4 address" ;;
esac

for tool in ssh sshpass scp; do
	command -v "$tool" >/dev/null 2>&1 || fail "Missing required tool: $tool"
done
[ -f "$THEME/theme.xml" ] || fail "Theme not found: $THEME/theme.xml"

SSH_OPTIONS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -o PreferredAuthentications=password,keyboard-interactive -o PubkeyAuthentication=no"

# Refuse a symlink destination so an upload cannot overwrite the system theme.
# shellcheck disable=SC2086
SSHPASS=$DEVICE_PASSWORD sshpass -e ssh $SSH_OPTIONS "root@$DEVICE_IP" sh -s <<'REMOTE'
set -eu
themes=/storage/.config/emulationstation/themes
target="$themes/art-book-ds"
[ -d "$themes" ] || { echo 'EmulationStation themes folder is missing' >&2; exit 1; }
[ ! -L "$target" ] || { echo 'Theme destination is a symlink; refusing upload' >&2; exit 1; }
if [ -e "$target" ]; then
	[ -d "$target" ] || { echo 'Theme destination is not a directory' >&2; exit 1; }
fi
mkdir -p "$target"
REMOTE

# Copy the contents into the named theme directory, including hidden files.
# shellcheck disable=SC2086
SSHPASS=$DEVICE_PASSWORD sshpass -e scp -O -r -C $SSH_OPTIONS "$THEME/." "root@$DEVICE_IP:$REMOTE_THEMES/art-book-ds/"
printf 'Uploaded art-book-ds to %s/art-book-ds\n' "$REMOTE_THEMES"
printf 'Select art-book-ds in EmulationStation theme settings (restart EmulationStation if needed).\n'
