# Changelog

## v1.0.1

### Fixed
- **`SpawnDelay` is no longer doubled in single player.** The spawn coroutine
  waits twice per titan (a prep wait and a cooldown wait) and both were being set
  to the full `SpawnDelay`, so the real gap between spawns was ~2×. Each wait is
  now half the value, so the total matches what you configure.
  > If you tuned around the old behavior, halve your old `SpawnDelay` (or just
  > re-tune) — a value of 10 now means ~10s between spawns, not ~20s.

### Changed
- Multiplayer spawn delay is set once when the scene controller starts, instead
  of patching the spawn coroutine every frame.
- Config is cached; rebuilt on the shared `aoqcore` library.
- Requires the AoQ-Modloader **v1.1.0** patched APK.
- Push mods to `/sdcard/DCIM/AoQMods/mods/`.

### Config (unchanged keys)
- `MaxTitans` — max titans alive at once (single player and multiplayer).
- `SpawnDelay` — total seconds between spawns.
