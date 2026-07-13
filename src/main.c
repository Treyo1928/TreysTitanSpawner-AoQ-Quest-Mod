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
 *   Multiplayer : NetworkSceneController creates ONE WaitForSeconds in
 *                 Start ("spawnDelay", field 0x1C) and yields it forever —
 *                 we overwrite its m_Seconds once in the Start postfix.
 *                 (v1 hooked the coroutine MoveNext every tick for this.)
 *   Single player: Scoring.<SpawnNewTitan_CR>d__37 yields TWO fresh
 *                 WaitForSeconds per spawn (1s prep + 2-4s cooldown), so we
 *                 patch each yielded wait to SpawnDelay/2 in a MoveNext
 *                 postfix. v1 patched both to the FULL value, which silently
 *                 doubled the configured delay.
 */

#include <android/log.h>
#include "../../AoQ-ModLoader-For-Quest/shared/inline-hook/inlineHook.h"
#include "../../AoQ-ModLoader-For-Quest/shared/utils/utils.h"
#include "../../AoQ-ModLoader-For-Quest/shared/aoqcore/aoq.h"
#include "../../AoQ-ModLoader-For-Quest/shared/modapi/modapi.h"

#define TAG "TitanSpawn"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define MOD_SO_NAME "libtitanspawn.so"

/* ── Hook targets (AoQ 0.5.0, verified with Il2CppDumper) ───────────── */
#define ADDR_Scoring_Start        0x4A7664
#define ADDR_NSC_Start            0x6332E0
#define ADDR_SpawnCR_MoveNext     0x4A88DC   /* Scoring.<SpawnNewTitan_CR>d__37 */

/* ── Field offsets (from dump.cs) ───────────────────────────────────── */
#define SCORING_MAX_TITANS_OFF    0x40
#define NSC_MAX_TITANS_OFF        0x18
#define NSC_SPAWN_DELAY_OFF       0x1C   /* WaitForSeconds instance */
#define WFS_SECONDS_OFF           0x08   /* WaitForSeconds.m_Seconds */
#define CR_CURRENT_OFF            0x0C   /* state machine <>2__current */

/* ── Config (cached) ────────────────────────────────────────────────── */
static AoqCfgCache g_cfg = {0};

static int   cfg_max_titans  = 5;
static float cfg_spawn_delay = 5.0f;

static void refresh_config(void)
{
    if (aoq_cfg_refresh(&g_cfg, MOD_SO_NAME) != 0) return;
    cfg_max_titans  = aoq_cfg_int(&g_cfg, "MaxTitans",  5);
    cfg_spawn_delay = aoq_cfg_flt(&g_cfg, "SpawnDelay", 5.0f);
    if (cfg_max_titans < 1)     cfg_max_titans  = 1;
    if (cfg_spawn_delay < 0.0f) cfg_spawn_delay = 0.0f;
}

/* ── Scoring.Start (single player, postfix) ─────────────────────────── */
MAKE_HOOK(Scoring_Start, ADDR_Scoring_Start, void, void *self)
{
    Scoring_Start(self);
    refresh_config();
    int prev = AOQ_FIELD(self, SCORING_MAX_TITANS_OFF, int);
    AOQ_FIELD(self, SCORING_MAX_TITANS_OFF, int) = cfg_max_titans;
    LOGI("SP maxTitans: %d -> %d", prev, cfg_max_titans);
}

/* ── NetworkSceneController.Start (multiplayer, postfix) ────────────── */
MAKE_HOOK(NetworkSceneController_Start, ADDR_NSC_Start, void, void *self)
{
    NetworkSceneController_Start(self);
    refresh_config();

    int prev = AOQ_FIELD(self, NSC_MAX_TITANS_OFF, int);
    AOQ_FIELD(self, NSC_MAX_TITANS_OFF, int) = cfg_max_titans;

    /* Original Start just created spawnDelay = new WaitForSeconds(5f) and
     * yields that same object forever — patch its seconds once. */
    void *wfs = AOQ_FIELD(self, NSC_SPAWN_DELAY_OFF, void *);
    if (wfs) AOQ_FIELD(wfs, WFS_SECONDS_OFF, float) = cfg_spawn_delay;

    LOGI("MP maxTitans: %d -> %d, spawnDelay -> %.2fs", prev, cfg_max_titans,
         cfg_spawn_delay);
}

/* ── Scoring.<SpawnNewTitan_CR>d__37.MoveNext (single player) ──────── */
/*   The coroutine waits twice per spawned titan; each yielded object is
 *   always a WaitForSeconds (verified in the decompiled game code), so
 *   patching m_Seconds on <>2__current is type-safe here.               */
MAKE_HOOK(SpawnNewTitan_CR_MoveNext, ADDR_SpawnCR_MoveNext, int, void *self)
{
    int result = SpawnNewTitan_CR_MoveNext(self);
    if (result) {                       /* still running — something was yielded */
        void *current = AOQ_FIELD(self, CR_CURRENT_OFF, void *);
        if (current)                    /* NULL = yield return null (frame wait) */
            AOQ_FIELD(current, WFS_SECONDS_OFF, float) = cfg_spawn_delay * 0.5f;
    }
    return result;
}

/* ── Entry point ─────────────────────────────────────────────────────── */
__attribute__((constructor)) void lib_main(void)
{
    LOGI("loading...");

    aoqmm_register(MOD_SO_NAME, "Titan Spawn Control", "1.1.0", "Treyo1928",
                   "Controls max simultaneous titans and time between spawns.");

    aoqmm_ensure_config(MOD_SO_NAME,
        "{\n"
        "  \"entries\": [\n"
        "    {\"key\":\"MaxTitans\",\"type\":\"int\",\"value\":5,"
            "\"description\":\"Maximum number of titans alive at once. "
            "Applies to both single player and multiplayer.\"},\n"
        "    {\"key\":\"SpawnDelay\",\"type\":\"float\",\"value\":5.0,"
            "\"description\":\"Total seconds between titan spawns. "
            "Applies to both single player and multiplayer.\"}\n"
        "  ]\n"
        "}\n"
    );

    aoq_init();
    refresh_config();

    INSTALL_HOOK(Scoring_Start);
    INSTALL_HOOK(NetworkSceneController_Start);
    INSTALL_HOOK(SpawnNewTitan_CR_MoveNext);

    LOGI("hooks installed!");
}
