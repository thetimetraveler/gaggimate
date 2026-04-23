# Fork build pipeline

This fork (`thetimetraveler/gaggimate`) maintains a long-lived integration branch, `fork/main`, that combines `upstream/master` with in-flight feature branches and publishes a rolling `fork-nightly` GitHub release with firmware artifacts.

## Layout

- `upstream/master` — source of truth for upstream
- `feature/*` and `fix/*` branches on `origin` — one per in-flight PR, each rebased clean on `upstream/master`
- `fork/main` — regenerated from `upstream/master + $(cat scripts/fork-features.txt)` by `scripts/sync-fork.sh`, force-pushed. Do not commit directly
- `fork-nightly` GitHub release — built by `.github/workflows/build-fork.yml` on every push to `fork/main`. Contains `display-*.bin`, `board-*.bin`, `version.txt`, and filesystem images

## Routine operations

### Pull latest from upstream

```
./scripts/sync-fork.sh --push
```

What the script does:
1. Requires a clean working tree.
2. Fetches `upstream/master` and `origin`.
3. Resets `fork/main` to `upstream/master`.
4. Merges each feature branch from `scripts/fork-features.txt` in order.
5. Skips branches already in `HEAD` (e.g., if a PR landed upstream between runs).
6. Bails on merge conflicts.

Force-push triggers GitHub Actions → new `fork-nightly` release in ~10 minutes.

### Add a new feature

```
git checkout -b feature/foo upstream/master
# ...work, commit, push to origin...
# edit scripts/fork-features.txt to add "feature/foo", commit, push feature/fork-infra
./scripts/sync-fork.sh --push
```

### A feature branch conflicts on merge

1. Rebase the feature branch locally to absorb the conflict (preferred — keeps the branch PR-ready).
2. Push the feature branch, rerun `sync-fork.sh --push`.

Resolving on `fork/main` directly won't survive the next regen.

### An upstream PR lands

Nothing required. On the next sync run the script auto-skips the feature branch because its commits are ancestors of `upstream/master`. Tidy up by deleting the branch and its line in `scripts/fork-features.txt` when convenient.

## Flashing

1. **First time (USB-C):** download the three partitions from `fork-nightly` and flash via ESP Web Tool (https://espressif.github.io/esptool-js/) or `esptool.py`. Offsets:
   - `0x0` — `display-bootloader.bin` (or `board-bootloader.bin` for the controller board)
   - `0x8000` — `display-partitions.bin`
   - `0x10000` — `display-firmware.bin`
2. **Subsequent updates (wireless):** in the display's web UI, set **OTA channel → Custom** (default URL points at this fork's `fork-nightly` release) and hit Update.
