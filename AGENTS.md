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

1. Confirm the local binary exists and is AArch64 before connecting. Connect
   using the user's existing SSH configuration and strict host-key verification.
   If using the owner-only `../SeedlessDS-Linux/.device.local` reference settings,
   read them programmatically and never print the password. Use the requested
   destination explicitly; do not take an install path from another project.
   Inspect the device OS/architecture, current frontend process and launch command,
   built-in executable/wrapper, service owner, and whether the destination is
   mounted. Do not guess process names, paths, or supervisor commands. Never use
   `killall`, broad `pkill`, or reboot.
2. Preserve and use the device's existing EmulationStation configuration. Do not
   pass alternate config/home paths, create or replace config files, clear data,
   or alter themes, gamelists, systems, settings, saves, or other user data.
   Launch the test executable with no extra arguments unless the user specifically
   requested a test argument; this lets the application use its normal device
   configuration.
3. Before testing, attempt a normal unmount of the deployed bin path if it is
   mounted (or make a best-effort `umount /storage/.config/emulationstation/bin`
   after checking mount state). Do not use lazy/forced unmount. If unmount fails,
   inspect the reason; do not proceed if the target would be ambiguous or unsafe.
4. Stop only the identified running EmulationStation instance gracefully using
   its confirmed service or exact PID. Verify it exited before testing and check
   that the graphical display service remains available. Do not kill unrelated
   processes.
5. Always deploy to the same predictable final name:
   `/storage/.config/emulationstation/bin/emulationstation`. Never use a
   session-specific name as the final deployed filename. Transfer to a temporary
   file in that directory, compare SHA-256 with the local artifact, set executable
   permission, and atomically rename it to the predictable final path. Replace a
   prior workflow deployment only after confirming it is not running. If that
   final path contains an unrelated or unverified user binary, preserve it and
   stop rather than overwrite it. This user-config override path is distinct from
   the built-in `/usr/bin/emulationstation`.
   A successfully transferred and verified binary is a useful device artifact:
   never remove it during cleanup. Remove only incomplete temporary transfer files
   created by the current session.
6. Run the deployed binary directly from its predictable path, without `sudo`, in
   the device's existing
   graphical/session environment and with its normal working directory if known.
   Capture output/status for diagnosis without exposing private data. Observe
   startup and normal UI operation; stop the test gracefully by its exact PID
   when done. Leave the verified deployed binary at the predictable path for
   subsequent testing.
7. Restart EmulationStation using its verified launcher/supervisor and normal
   environment. The current launcher does not automatically select the deployed
   binary. Do not change the service, PATH, symlinks, or boot behavior to activate
   it; the user plans to add a mount script on boot later. After testing, restart
   the built-in frontend and verify its actual executable; a service reporting
   active can precede its child starting. Keep the verified deployed binary and
   existing EmulationStation configs/files intact.
8. Report build, transfer/hash, test launch/runtime, cleanup, and built-in restart
   as separate outcomes. A successful build or transfer alone is not a successful
   device test. If the built-in launcher cannot be identified or restarted safely,
   stop and report that clearly rather than guessing.

### Observed RG DS Plus / ROCKNIX launcher

These details were observed on the user's RG DS Plus during the 2026-10-20 test;
they are a useful starting point, but verify current unit/process state each time
because device updates can change them.

| Item | Observed value |
|---|---|
| Distribution | ROCKNIX `20261020` nightly |
| Kernel / architecture | Linux `7.2.11`, AArch64 |
| Hardware | RK3566 |
| Display stack | Sway / Wayland; `sway.service` was active |
| EmulationStation unit | `essway.service`, active |
| Built-in executable | `/usr/bin/emulationstation` |
| Launcher | `/usr/bin/start_es.sh` |
| Frontend `PATH` | `/usr/bin:/usr/sbin` |

The `essway.service` unit is `Type=simple`, `ExecStart=/usr/bin/start_es.sh`,
`WorkingDirectory=/storage`, `HOME=/storage`, and loads `/etc/profile`. It has
`Requires=sway.service`, `Restart=always`, and a 2-second restart delay. The
launcher sources `/usr/bin/es_settings`, performs display rotation and stale
scanout setup, then executes the built-in frontend with
`--log-path /var/log --no-splash`. Its process tree was:

```text
systemd (PID 1)
└─ essway.service: /bin/bash /usr/bin/start_es.sh
   └─ /usr/bin/emulationstation --log-path /var/log --no-splash
```

The deployed binary directory is not automatically mounted or selected by the
current startup scripts. The active frontend environment included
`HOME=/storage`, `DISPLAY=:0.0`,
`WAYLAND_DISPLAY=wayland-1`, `XDG_RUNTIME_DIR=/var/run/0-runtime-dir`,
`SDL_VIDEODRIVER=wayland`, and `WLR_CON=DSI-2`. Its `PATH` was only
`/usr/bin:/usr/sbin`, so the configured binary directory was not searched and
deployment alone did not change the daily frontend. The user plans to create a
boot-time mount script in the future. Until then, keep the deployed binary at the
fixed path for that future workflow and continue launching the built-in frontend
after tests. Do not alter service configuration, PATH, symlinks, or boot behavior.
For direct testing, capture the current frontend's environment from
`/proc/<frontend-pid>/environ` before
stopping the service. `systemctl stop essway.service` stopped the frontend
without stopping `sway.service`; Sway remained available for the test UI. Do not
stop or restart Sway as part of this workflow.

During the 2026-10-20 test, `/storage/.config/emulationstation/bin` did not exist
and was not mounted. A normal `umount` attempt was made before testing. The test
binary ran for 35 seconds with the current frontend environment; its only
filtered log error was an unsuccessful HTTP request. `essway.service` was
restarted and its built-in child was verified running before identifying that its
PATH did not select the deployed binary. Keep successful test binaries on the
device after testing; do not remove the fixed-name deployed executable as part
of cleanup.
The final build is deployed as
`/storage/.config/emulationstation/bin/emulationstation` with matching local and
device SHA-256 `9ffd44f3504d486be6d2e1d2872dcc5d234904e8f0b64bca6652a953e0f1955d`.
The built-in frontend executable was not replaced.

Keep hardware testing bounded and observable. Never silently change device
configuration to make the test pass. The process/launcher details discovered on a
particular device may be recorded in project documentation only after confirming
they are stable and do not contain credentials or private network details.
