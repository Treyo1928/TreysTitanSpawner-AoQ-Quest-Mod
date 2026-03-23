# Titan Spawn Control — AoQ Quest Mod

A Quest C mod for **Attack on Quest (AoQ) 0.5.0** that lets you control the maximum number of titans alive at once and the time between spawns. Works in both single player and multiplayer (host only).

Built on the [AoQ-ModLoader-For-Quest](https://github.com/Treyo1928/AoQ-Modloader) framework.

---

## Config

All values are editable in-game via **Mods → Configure Mods → Titan Spawn Control**.

| Key | Type | Default | Description |
|---|---|---|---|
| `MaxTitans` | int | `5` | Maximum number of titans alive at once |
| `SpawnDelay` | float | `5.0` | Seconds between titan spawns |

Both settings apply to single player and multiplayer. In multiplayer you must be the host.

Config is stored at:
```
/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs/titanspawn.json
```

---

## How It Works

**Max titans** — hooks `Scoring.Start` (single player) and `NetworkSceneController.Start` (multiplayer) to overwrite the `maxTitans` field after initialisation.

**Spawn delay** — hooks the `MoveNext()` method on both spawn coroutine state machines. Every time a coroutine yields a `WaitForSeconds` object, the mod overwrites its `m_Seconds` field with the configured delay. This replaces the timing at the coroutine level without touching the actual spawn logic.

---

## Install

**Prerequisites:** The game must be running the patched APK from [AoQ-ModLoader-For-Quest](https://github.com/Treyo1928/AoQ-Modloader).

1. Download `libtitanspawn.so` from the [Releases](../../releases) page.
2. Push it to your headset:
```bash
adb push libtitanspawn.so /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/
```
3. Restart the game. The mod will appear in the **Mods** panel in the main menu.

---

## Build from Source

**Prerequisites:** Android NDK r26d

```bash
git clone <this-repo>
cd libtitanspawn-quest
bash build.sh
```

To build and push to a connected headset in one step:
```bash
bash build.sh --push
```

To also open a filtered logcat session after pushing:
```bash
bash build.sh --push --logs
```

The output binary is at `libs/armeabi-v7a/libtitanspawn.so`.

---

## Verify It's Working

```bash
adb logcat -s TitanSpawn,AoQModManager,QuestHook
```

You should see on level load:
```
TitanSpawn: SP maxTitans: 3 -> 5
```
or in multiplayer:
```
TitanSpawn: MP maxTitans: 3 -> 5
```
