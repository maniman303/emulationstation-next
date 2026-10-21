# RG DS Plus build and deployment

Build the ROCKNIX AArch64 executable locally:

```sh
scripts/build-rgdsplus.sh
```

Copy `scripts/deploy-rgdsplus.conf.example` to
`scripts/deploy-rgdsplus.conf`, then set `DEVICE_IP` and `DEVICE_PASSWORD` in
the copied file. Keep that file private; Git ignores it and the deploy script
sets its permissions to `600`. The template is safe to commit and contains no
device credentials.

The deploy script uses `sshpass`, SSH, SCP, and
`aarch64-linux-gnu-readelf`. It deploys the root `emulationstation` artifact
from the most recent local build. SSH automatically accepts the first host key
for the configured IP, then checks that key on later connections. A changed
host key stops deployment so the device identity can be checked.

The script calculates SHA-256 from the local build and checks it against the
uploaded file, the mounted executable, and the running frontend process.
The verified upload atomically replaces the storage binary; no `.new` or `.old`
binaries are needed.

Deploy the latest build:

```sh
scripts/deploy-rgdsplus.sh
```

The script stages the binary in the device's deployment directory, compares
SHA-256 before promotion, runs the installed mount helper, and verifies the
service, Sway, bind mount, and running frontend executable hash. It preserves
the normal launcher and device configuration.

On each run, the mount helper stops the active frontend, clears the previous
bind mount, and removes `/storage/mount/emulationstation`. It creates that
directory if needed, copies only
`/storage/.config/emulationstation/bin/emulationstation` to
`/storage/mount/emulationstation/emulationstation`, and bind-mounts the copy over
`/usr/bin/emulationstation`. If the storage directory or its `emulationstation` binary is missing, it leaves
the system executable without an override. It restarts the frontend if it was active.

To recover from a faulty override, remove
`/storage/.config/emulationstation/bin/emulationstation` (or the entire `bin`
directory) and reboot the handheld. The runtime copy is cleared on the next
helper run, and the original system EmulationStation is used.
