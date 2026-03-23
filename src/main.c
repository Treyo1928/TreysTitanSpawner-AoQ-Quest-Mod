/* libtitanspawn-quest/src/main.c
 * Titan spawn control mod for AttackOnQuest 0.5.0
 *
 * Config:
 *   MaxTitans  — max titans alive simultaneously (both modes)
 *   SpawnDelay — seconds between spawns (both modes)
 *
 * Max titans:
 *   Single player : Scoring.Start postfix → maxTitans (0x40)
 *   Multiplayer   : NetworkSceneController.Start postfix → maxTitans (0x18)
 *
 * Spawn delay:
 *   Both modes: hook the coroutine state machine MoveNext() postfix.
 *   After MoveNext yields, <>2__current (0xC) holds the WaitForSeconds
 *   object. We overwrite its m_Seconds (0x8) with cfg_spawn_delay.
 *   This replaces the timing on every yield, whether the coroutine uses
 *   a stored WaitForSeconds or creates a new one each iteration.
 *
 *   SP state machine: Scoring.<SpawnNewTitan_CR>d__37  MoveNext 0x4A88DC
 *   MP state machine: NetworkSceneController.<TitanSpawnCR>d__11  MoveNext 0x633910
 */

#include <android/log.h>
#include "../../AoQ-ModLoader-For-Quest/shared/inline-hook/inlineHook.h"
#include "../../AoQ-ModLoader-For-Quest/shared/utils/utils.h"
#include "../../AoQ-ModLoader-For-Quest/shared/modapi/modapi.h"
#include "../../AoQ-ModLoader-For-Quest/modmanager/modconfig.h"

#define TAG "TitanSpawn"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

/* ── Config ─────────────────────────────────────────────────────────── */
static int   cfg_max_titans  = 5;
static float cfg_spawn_delay = 5.0f;

static void reload_config(void)
{
    ModConfig cfg;
    if (load_config("libtitanspawn.so", &cfg) != 0) return;
    ModCfgEntry *e;
    if ((e = get_entry(&cfg, "MaxTitans")))  cfg_max_titans  = (int)e->value_num;
    if ((e = get_entry(&cfg, "SpawnDelay"))) cfg_spawn_delay = (float)e->value_num;
    if (cfg_max_titans < 1)    cfg_max_titans  = 1;
    if (cfg_spawn_delay < 0.0f) cfg_spawn_delay = 0.0f;
}

/* ── Scoring.Start (single player, postfix) ─────────────────────────── */
/*   Fields: currentTitans 0x3C | maxTitans 0x40                        */

MAKE_HOOK(Scoring_Start, 0x4A7664, void, void *self)
{
    Scoring_Start(self);
    reload_config();
    int prev = *(int *)((char *)self + 0x40);
    *(int *)((char *)self + 0x40) = cfg_max_titans;
    LOGI("SP maxTitans: %d -> %d", prev, cfg_max_titans);
}

/* ── NetworkSceneController.Start (multiplayer, postfix) ────────────── */
/*   Fields: maxTitans 0x18                                             */

MAKE_HOOK(NetworkSceneController_Start, 0x6332E0, void, void *self)
{
    NetworkSceneController_Start(self);
    reload_config();
    int prev = *(int *)((char *)self + 0x18);
    *(int *)((char *)self + 0x18) = cfg_max_titans;
    LOGI("MP maxTitans: %d -> %d", prev, cfg_max_titans);
}

/* ── Coroutine MoveNext helper ──────────────────────────────────────── */
/*   Called after the original MoveNext runs. If the coroutine is still
 *   running (result == 1) and yielded a non-null object, treat it as a
 *   WaitForSeconds and overwrite m_Seconds (0x8) with cfg_spawn_delay. */

static void patch_yield(int result, void *state_machine)
{
    if (!result) return;   /* coroutine finished — nothing yielded */
    void *current = *(void **)((char *)state_machine + 0x0C);
    if (!current) return;  /* yield return null (frame wait) — skip */
    *(float *)((char *)current + 0x8) = cfg_spawn_delay;
}

/* ── Scoring.<SpawnNewTitan_CR>d__37.MoveNext (single player) ──────── */

MAKE_HOOK(SpawnNewTitan_CR_MoveNext, 0x4A88DC, int, void *self)
{
    int result = SpawnNewTitan_CR_MoveNext(self);
    patch_yield(result, self);
    return result;
}

/* ── NetworkSceneController.<TitanSpawnCR>d__11.MoveNext (multi) ────── */

MAKE_HOOK(TitanSpawnCR_MoveNext, 0x633910, int, void *self)
{
    int result = TitanSpawnCR_MoveNext(self);
    patch_yield(result, self);
    return result;
}

/* ── Entry point ─────────────────────────────────────────────────────── */
__attribute__((constructor)) void lib_main(void)
{
    LOGI("loading...");

    aoqmm_register("libtitanspawn.so", "Titan Spawn Control", "1.0.0", "Treyo1928",
                   "Controls max simultaneous titans and time between spawns.");

    aoqmm_ensure_config("libtitanspawn.so",
        "{\n"
        "  \"entries\": [\n"
        "    {\"key\":\"MaxTitans\",\"type\":\"int\",\"value\":5,"
            "\"description\":\"Maximum number of titans alive at once. "
            "Applies to both single player and multiplayer.\"},\n"
        "    {\"key\":\"SpawnDelay\",\"type\":\"float\",\"value\":5.0,"
            "\"description\":\"Seconds between titan spawns. "
            "Applies to both single player and multiplayer.\"}\n"
        "  ]\n"
        "}\n"
    );

    reload_config();

    INSTALL_HOOK(Scoring_Start);
    INSTALL_HOOK(NetworkSceneController_Start);
    INSTALL_HOOK(SpawnNewTitan_CR_MoveNext);
    INSTALL_HOOK(TitanSpawnCR_MoveNext);

    LOGI("hooks installed!");
}
