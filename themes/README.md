# Art Book DS

`art-book-ds` is a copy of the RG DS Plus system theme, obtained on
2026-10-21 from `/storage/.config/emulationstation/themes/system-theme`
(which resolved to `/usr/share/themes/es-theme-art-book-next`).
The theme folder and theme name are renamed to Art Book DS. Original author
and license notices, assets, and the upstream README are preserved.

Deploy from the repository root:

```sh
scripts/deploy-theme-rgdsplus.sh
```

The script uses `DEVICE_IP` and `DEVICE_PASSWORD` from the private
`scripts/deploy-rgdsplus.conf`, just like the binary deployment script.
If needed, copy `scripts/deploy-rgdsplus.conf.example` to that path and fill
in the values. It requires `sshpass`, `ssh`, and `scp` on the local host.
Set `DEPLOY_CONFIG` to use another connection config file.

Files are uploaded to
`/storage/.config/emulationstation/themes/art-book-ds`. The existing
`art-book-ds` directory is removed first, then the local theme is uploaded in
full, including hidden files. A symlink destination is refused. The script
does not select the theme or restart the frontend. Select `art-book-ds` in
EmulationStation's theme settings after deployment.

See [the upstream README](art-book-ds/README.md) for theme options,
attribution, and licensing.
