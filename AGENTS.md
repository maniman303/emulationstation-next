# Agent Instructions

## C++ conventions

- Do not add unnamed (anonymous) namespaces. Keep file-local declarations at
  file scope or use a named namespace when grouping is needed.

## RG DS Plus build and deployment

Use this repository for EmulationStation frontend work. Build and verify the
binary locally before connecting to the RG DS Plus; never build on the device.

### Build locally

- Build optimized `Release` binaries by default from now on. Do not use
  `Debug` or `RelWithDebInfo` unless explicitly requested. The RG DS Plus helper
  uses `-DCMAKE_BUILD_TYPE=Release` and strips unneeded symbols with
  `aarch64-linux-gnu-strip --strip-unneeded` to keep the deployed binary small.
- Run commands from the repository root. On the supported Ubuntu/Debian
  multiarch host, build the ROCKNIX AArch64 target with:

  ```sh
  scripts/build-rgdsplus.sh
  file emulationstation
  ```

- The helper configures `-DROCKNIX=ON`. This is required for the device's
  per-system settings store at
  `/storage/.config/system/configs/system.cfg`.
- CMake places the executable at the repository root as `emulationstation`.
  Deploy that executable, not `emulationstation.sh`, which is a restart wrapper.
- The helper uses the multiarch libraries and
  `CMake/toolchains/aarch64-multiarch-linux-gnu.cmake`. Keep these ARM64 paths;
  do not substitute host x86-64 libraries. Verify the artifact is AArch64
  before deployment.
- Resolve local build and configuration problems before device activity. If a
  dependency must be installed or a step requires `sudo`, stop and provide the
  user with exact commands, one per line. Do not invoke `sudo` or install
  packages yourself.

### Configure and deploy

The deployment target is
`/storage/.config/emulationstation/bin/emulationstation`. Configure the private,
git-ignored file `scripts/deploy-rgdsplus.conf` by copying
`scripts/deploy-rgdsplus.conf.example` and setting `DEVICE_IP` and
`DEVICE_PASSWORD`. The deploy script restricts the config file to mode `600`.
Never print or place the password in commands, logs, or tracked files.

From the repository root, deploy with:

```sh
scripts/deploy-rgdsplus.sh
```

This script deploys the root `emulationstation` artifact from the latest local
build. It checks the artifact architecture and calculates its SHA-256 at deploy
time; no expected hash belongs in the config. It uses the configured IP and
password for SSH/SCP, accepts a host key on first connection, and rejects a
changed key on later connections.

Before device activity, use read-only SSH inspection to confirm the current OS
and architecture, `essway.service` owner and state, frontend process and launch
command, `/usr/bin/start_es.sh`, built-in executable, Sway state, and mount state
for the deployment source and `/usr/bin/emulationstation`. Use network-enabled
execution from the first SSH/SCP attempt. Do not guess process names, service
commands, or paths. Never use `killall`, broad `pkill`, or reboot.

The deploy script checks that the deployment directory and mount helper exist
and that Sway is active. Copy the root build artifact directly to
`/storage/.config/emulationstation/bin/emulationstation`, replacing that file,
then compare its SHA-256 with the local build. Only after the hashes match, run
`/storage/.config/autostart/mount_emulationstation.sh`. Do not use `.new`, `.old`,
backup, or temporary upload filenames for the binary. Do not change the mount
helper to introduce staging or backup files.

The mount helper stops an active EmulationStation service, waits up to 10
seconds for its process to exit, clears stale bind mounts with normal `umount`,
copies the deployed binary to `/storage/mount/emulationstation/emulationstation`,
applies executable permission, and bind-mounts that runtime copy over
`/usr/bin/emulationstation`. It restarts a service that was active before the
helper ran. The deploy script starts `essway.service` if it was inactive, then
verifies the service, Sway, bind mount, deployed source hash, mounted target
hash, and running frontend process hash.

Keep the normal `/usr/bin/start_es.sh` launcher, service configuration, HOME,
arguments, and user EmulationStation config. Do not pass alternate config/home
paths or modify themes, gamelists, systems, settings, saves, or other user data.
Do not launch the deployed executable directly. Leave a verified deployment
mounted and running for the user's UI check; do not restore or unmount it unless
asked. Report build, transfer/hash, mount, and service/runtime results
separately. A successful build or transfer alone is not a device test.

### Observed RG DS Plus / ROCKNIX launcher

Verify current state each time because device updates can change it. Last
observed on 2026-10-21:

| Item | Observed value |
|---|---|
| Distribution | ROCKNIX `20261020` nightly |
| Kernel / architecture | Linux `7.2.11`, AArch64 |
| Hardware | RK3566 |
| Display stack | Sway / Wayland |
| EmulationStation unit | `essway.service` |
| Built-in executable | `/usr/bin/emulationstation` |
| Launcher | `/usr/bin/start_es.sh` |
| Frontend `PATH` | `/usr/bin:/usr/sbin` |

The observed `essway.service` runs `/usr/bin/start_es.sh` as root with
`WorkingDirectory=/storage`, `HOME=/storage`, and `/etc/profile`; it requires
`sway.service` and restarts after failure. The launcher sources
`/usr/bin/es_settings`, applies display setup, then executes EmulationStation
with `--log-path /var/log --no-splash`. The frontend used Wayland with
`DISPLAY=:0.0`, `WAYLAND_DISPLAY=wayland-1`,
`XDG_RUNTIME_DIR=/var/run/0-runtime-dir`, `SDL_VIDEODRIVER=wayland`, and
`WLR_CON=DSI-2`.

The installed mount helper is
`/storage/.config/autostart/mount_emulationstation.sh`; its tracked source is
`scripts/mount-emulationstation.sh`. It binds the deployed source over
`/usr/bin/emulationstation`. Do not change the service, launcher, PATH, symlinks,
or Sway configuration as part of deployment. The last successful deployment
was verified running on 2026-10-21; always verify the current process and mount
instead of relying on a remembered hash.

Keep hardware testing bounded and observable. Never silently change device
configuration to make a test pass. Record process and launcher details only
after confirming they are stable and contain no credentials or private network
details.
