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
Existing `.old` backups are moved aside with a unique `predeploy` suffix before
the mount helper rotates files.

Deploy the latest build:

```sh
scripts/deploy-rgdsplus.sh
```

The script stages the binary in the device's deployment directory, compares
SHA-256 before promotion, runs the installed mount helper, and verifies the
service, Sway, bind mount, and running frontend executable hash. It preserves
the normal launcher and device configuration.
