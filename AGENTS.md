# Agent Instructions

## Build and RG DS Plus device test workflow

Use this project for EmulationStation frontend work. The sibling `../SeedlessDS-Linux`
and `../DSperate` repositories are references for ARM64 cross builds and carefully staged
SSH device validation; their application-specific scripts and binaries do not apply here.

### Build locally first

- Run commands from this repository root. On the supported Ubuntu/Debian
  multiarch host, build for the RG DS Plus with:

  ```sh
  scripts/build-rgdsplus.sh
  file emulationstation
  ```

- This target is a ROCKNIX device build. `scripts/build-rgdsplus.sh` must set
  `-DROCKNIX=ON`; otherwise per-system emulator/core settings use the generic
  EmulationStation settings store instead of `/storage/.config/system/configs/system.cfg`.

- This project's CMake rules place the executable at the repository root as
  `emulationstation`, even when the build tree is under `build/`. Check the
  actual generated path if CMake changes its layout. The project's
  executable target is `emulationstation`; `emulationstation.sh` is a restart
  wrapper, not the binary to deploy.
- The build helper pins CMake's dependency paths to installed ARM64
  multiarch libraries and uses `cmake/toolchains/aarch64-multiarch-linux-gnu.cmake`,
  based on the sibling DSperate cross-build setup. Verify the artifact's ELF
  architecture and target ABI before connecting to hardware. Do not replace
  these paths with host x86-64 libraries.
- Build and resolve local dependency/configuration issues before any device
  activity. Do not build on the handheld.
- If a dependency must be installed or any step requires `sudo`, stop before
  running that command. Provide the exact commands for the user to run, one
  command per line (no multiline script blocks), and wait for the user's
  confirmation that they ran them before continuing. Do not invoke `sudo`
  yourself or install packages/dependencies on the user's behalf.
- Any SSH, SCP, or other command that needs network access must be launched with
  network-enabled execution from its first attempt. Never use a restricted
  attempt first. Do not include passwords in commands, logs, or tracked files;
  use the owner's existing SSH key/configuration and host-key verification.

### Device deployment and test

Target: RG DS Plus, destination binary
`/storage/.config/emulationstation/bin/emulationstation`. The frontend may be
supervised and user configuration already exists on device.

1. Build locally first. Confirm the local binary exists and is AArch64 before
   connecting. Connect using the user's existing SSH configuration and strict
   host-key verification. If using the owner-only
   `../SeedlessDS-Linux/.device.local` reference settings, read them
   programmatically and never print the password. Inspect the device OS and
   architecture, current frontend process and launch command, built-in
   executable/wrapper, service owner, and mount state for the deployed source and
   `/usr/bin/emulationstation`. Never guess process names, paths, or supervisor
   commands. Never use `killall`, broad `pkill`, or reboot.
2. Preserve the existing EmulationStation configuration. Do not pass alternate
   config/home paths, create or replace config files, clear data, or alter themes,
   gamelists, systems, settings, saves, or other user data. Do not launch the
   deployed executable directly; use the normal device launcher and service so it
   receives the normal environment, arguments, and configuration.
3. Stage every deployment as
   `/storage/.config/emulationstation/bin/emulationstation.new`. Compare its
   SHA-256 with the local artifact before running the mount helper. If it does
   not match, remove only that incomplete staged file and stop. If the current
   `emulationstation` is an unrelated or unverified user binary, preserve it and
   stop rather than replace it. The helper promotes a verified `.new` file after
   stopping EmulationStation and unmounting stale bind mounts: it removes the
   previous `.old`, renames the current binary to `.old` when present, renames
   `.new` to `emulationstation`, and applies executable permission. This keeps
   the immediately previous binary as a rollback copy. Do not remove the newly
   promoted binary after a successful deployment.
4. Run `/storage/.config/autostart/mount_emulationstation.sh`. Its tracked source
   is `scripts/mount-emulationstation.sh`. It stops the frontend, clears stale
   bind mounts with normal `umount`, promotes `emulationstation.new` when present,
   then bind-mounts the deployed binary over `/usr/bin/emulationstation`. Every
   invocation with a deployed or staged binary checks whether
   `essway.service` is active; if so, it stops the service and starts it again
   from an exit trap, including when unmounting or mounting fails. Do not use
   lazy/forced unmount. Before unmounting, it checks every 0.5 seconds for up to
   10 seconds until the EmulationStation executable process has exited. If the
   service was inactive before the helper ran, start it after a successful mount
   when a UI test is requested.
5. The service runs `/usr/bin/start_es.sh` with the device's normal environment
   and arguments, preserving its normal config selection. Verify the service and
   child process are running, then compare the child's executable SHA-256 with the
   deployed artifact to confirm the bind mount is active. Check that Sway remains
   available; do not stop Sway or kill unrelated processes.
6. Leave the verified deployment mounted and running for the user's UI check. Do
   not restore the built-in binary, unmount the deployed executable, or remove it
   unless the user asks. Keep existing EmulationStation config and data intact.
7. Report build, transfer/hash, mount, service launch/runtime, and config-preserving
   launch as separate outcomes. A successful build or transfer alone is not a
   successful device test. If the launcher/service cannot be identified or
   restarted safely, stop and report that clearly rather than guessing.

### Observed RG DS Plus / ROCKNIX launcher

These details were observed on the user's RG DS Plus during the 2026-10-20 test;
verify current unit/process state each time because device updates can change them.

| Item | Observed value |
|---|---|
| Distribution | ROCKNIX `20261020` nightly |
| Kernel / architecture | Linux `7.2.11`, AArch64 |
| Hardware | RK3566 |
| Display stack | Sway / Wayland; `sway.service` was active |
| EmulationStation unit | `essway.service` |
| Built-in executable | `/usr/bin/emulationstation` |
| Launcher | `/usr/bin/start_es.sh` |
| Frontend `PATH` | `/usr/bin:/usr/sbin` |

The `essway.service` unit is `Type=simple`, `ExecStart=/usr/bin/start_es.sh`,
`WorkingDirectory=/storage`, `HOME=/storage`, and loads `/etc/profile`. It has
`Requires=sway.service`, `Restart=always`, and a 2-second restart delay. The
launcher sources `/usr/bin/es_settings`, performs display rotation and stale
scanout setup, then executes EmulationStation with `--log-path /var/log
--no-splash`. The frontend environment included `HOME=/storage`, `DISPLAY=:0.0`,
`WAYLAND_DISPLAY=wayland-1`, `XDG_RUNTIME_DIR=/var/run/0-runtime-dir`,
`SDL_VIDEODRIVER=wayland`, and `WLR_CON=DSI-2`.

The device has a DSperate bind-mount script in
`/storage/.config/autostart/mount_dsperate.sh`. The equivalent EmulationStation
script is installed at `/storage/.config/autostart/mount_emulationstation.sh`
and tracked in this repository as `scripts/mount-emulationstation.sh`. It binds
`/storage/.config/emulationstation/bin/emulationstation` over
`/usr/bin/emulationstation`; run it after deploying and before starting
`essway.service`. This selects the deployed binary while keeping the normal
launcher, environment, working directory, arguments, and config path. Do not
change the service, PATH, symlinks, or Sway configuration as part of deployment.
`systemctl stop essway.service` stopped the frontend without stopping Sway on the
observed device; do not stop or restart Sway as part of this workflow.

On 2026-10-20 the deployed executable at
`/storage/.config/emulationstation/bin/emulationstation` was verified as AArch64
with SHA-256 `c982ca4fcda82a04c6cf0ccea6294d7980773f6c27d4e2b7204b0ddd89368d9c`.
The current device deployment is built with `ROCKNIX=ON`; its running frontend
process was verified against this hash after starting `essway.service`.
The bind-mount helper follows the existing DSperate helper's idempotent pattern,
with `/usr/bin/emulationstation` as its target.

Keep hardware testing bounded and observable. Never silently change device
configuration to make a test pass. Record process/launcher details only after
confirming they are stable and contain no credentials or private network details.
