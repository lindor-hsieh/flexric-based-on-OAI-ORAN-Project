/*
 * xapp_prb_alloc.c — Unified PRB Allocation xApp (No DRL)
 *
 * 論文: "Performance Evaluation of xApp-based PRB Allocation in an O-RAN IAB Testbed"
 *
 * 單一 binary，控制所有 IAB 節點的 PRB 分配。
 * 策略由環境變數 PRB_POLICY 決定，無需重新編譯：
 *
 *   PRB_POLICY=fixed50   BH=50%, AC=50%
 *   PRB_POLICY=fixed30   BH=30%, AC=70%
 *   PRB_POLICY=fixed10   BH=10%, AC=90%
 *   PRB_POLICY=adaptive  Algorithm 1 (BSR-based dynamic)
 *   (PF baseline: 不啟動此 xApp)
 *
 * 節點分類 (nb_id):
 *   3585, 3586 → relay  → apply BH_ratio
 *   3587, 3588, 3589 → access → apply AC_ratio
 *
 * 編譯 (在 flexric/build 目錄下):
 *   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
 *         -DKPM_VERSION=KPM_V3_00 -DE2AP_VERSION=E2AP_V2 ..
 *   ninja xapp_prb_alloc
 */

#define PLAIN

#include "xApp/e42_xapp_api.h"
#include "sm/mac_sm/ie/mac_data_ie.h"
#include "sm/mac_sm/mac_sm_id.h"
#include "lib/e2ap/v2_03/e2ap_types/common/e2ap_global_node_id.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

/* ─── System parameters ──────────────────────────────────────────────────── */
#define TOTAL_PRB_COUNT    106     /* 40 MHz @ 30 kHz SCS */
#define MAX_UE_COUNT       16
#define SLOT_MASK_FULL     0xFFFF
#define MAX_NODES          8

/* Adaptive policy (Algorithm 1) */
#define ADAPTIVE_TAU       0.10   /* threshold τ */
#define ADAPTIVE_EPSILON   1.0    /* ε: avoid div-by-zero */
#define ADAPTIVE_INTERVAL  10     /* update ratio every N callbacks */

/* Watchdog: exit if a subscribed node goes silent for this many seconds */
#define WATCHDOG_TIMEOUT_S 30

/* ─── Node role tables ───────────────────────────────────────────────────── */
#define DONOR_NB_ID    3584   /* Donor DU: skip, not an IAB relay */
static const uint32_t RELAY_NB_IDS[] = {3585, 3586};
#define NUM_RELAY  2

/* ─── Policy type ────────────────────────────────────────────────────────── */
typedef enum { POLICY_FIXED50, POLICY_FIXED30, POLICY_FIXED10, POLICY_ADAPTIVE } policy_t;
static const char *POLICY_NAMES[] = {"fixed50", "fixed30", "fixed10", "adaptive"};

/* ─── Per-node state ─────────────────────────────────────────────────────── */
typedef struct {
    global_e2_node_id_t id;
    bool                is_relay;
    sm_ans_xapp_t       sub;

    pthread_mutex_t     lock;
    uint16_t            rnti[MAX_UE_COUNT];
    int                 num_ues;

    /* BSR proxy: dl_aggr_tbs delta per UE */
    uint64_t            prev_tbs[MAX_UE_COUNT];
    double              bsr_sum;      /* aggregate BSR for adaptive policy */

    volatile time_t     last_ts;
} node_ctx_t;

static node_ctx_t g_ctx[MAX_NODES];
static int        g_nctx = 0;

/* ─── Global allocation ratios ───────────────────────────────────────────── */
static policy_t        g_policy;
static double          g_bh_ratio = 0.5;
static double          g_ac_ratio = 0.5;
static pthread_mutex_t g_ratio_lock = PTHREAD_MUTEX_INITIALIZER;

/* ─── Helpers ────────────────────────────────────────────────────────────── */
static bool is_relay_nb(uint32_t nb_id)
{
    for (int i = 0; i < NUM_RELAY; i++)
        if (RELAY_NB_IDS[i] == nb_id) return true;
    return false;
}

/* Compute dl_aggr_tbs delta for one UE within a node context (called under lock). */
static uint64_t delta_tbs(node_ctx_t *ctx, int ue_idx, uint64_t curr)
{
    uint64_t prev = ctx->prev_tbs[ue_idx];
    uint64_t d    = (curr >= prev) ? (curr - prev) : 0;
    ctx->prev_tbs[ue_idx] = curr;
    return d;
}

/* ─── Algorithm 1: Dynamic Traffic-Aware PRB Allocation ─────────────────── */
static void adaptive_update(void)
{
    double bsr_bh = 0.0, bsr_ac = 0.0;

    for (int i = 0; i < g_nctx; i++) {
        pthread_mutex_lock(&g_ctx[i].lock);
        double s = g_ctx[i].bsr_sum;
        pthread_mutex_unlock(&g_ctx[i].lock);

        if (g_ctx[i].is_relay) bsr_bh += s;
        else                    bsr_ac += s;
    }

    double rho = bsr_bh / (bsr_bh + bsr_ac + ADAPTIVE_EPSILON);

    if (fabs(rho - 0.5) > ADAPTIVE_TAU) {
        double new_bh = floor(rho * TOTAL_PRB_COUNT) / TOTAL_PRB_COUNT;
        double new_ac = 1.0 - new_bh;

        pthread_mutex_lock(&g_ratio_lock);
        g_bh_ratio = new_bh;
        g_ac_ratio = new_ac;
        pthread_mutex_unlock(&g_ratio_lock);

        printf("[PRB] Adaptive: BSR_BH=%.0f BSR_AC=%.0f rho=%.3f"
               " → BH=%.0f%% AC=%.0f%%\n",
               bsr_bh, bsr_ac, rho, new_bh * 100, new_ac * 100);
    }
}

/* ─── Send E2 control message to one node ───────────────────────────────── */
static void send_ctrl(node_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    int n = ctx->num_ues;
    uint16_t rnti[MAX_UE_COUNT];
    memcpy(rnti, ctx->rnti, n * sizeof(uint16_t));
    pthread_mutex_unlock(&ctx->lock);

    if (n == 0) return;

    pthread_mutex_lock(&g_ratio_lock);
    double ratio = ctx->is_relay ? g_bh_ratio : g_ac_ratio;
    pthread_mutex_unlock(&g_ratio_lock);

    /* Distribute total ratio evenly across UEs at this node.
     * Per-UE quota = (ratio × PRB_total) / n  normalized back to [0,1]. */
    float per_ue = (float)(ratio / n);
    if (per_ue < 0.01f) per_ue = 0.01f;
    if (per_ue > 1.00f) per_ue = 1.00f;

    mac_slice_params_t slices[MAX_UE_COUNT];
    for (int i = 0; i < n; i++) {
        slices[i].id        = rnti[i];
        slices[i].prb_quota = per_ue;
        slices[i].slot_mask = SLOT_MASK_FULL;
        slices[i].priority  = 0;
    }

    mac_ctrl_req_data_t req = {0};
    req.msg.type       = 0;
    req.msg.len_slices = (uint32_t)n;
    req.msg.slices     = slices;
    control_sm_xapp_api(&ctx->id, SM_MAC_ID, &req);
    printf("[PRB] ctrl → nb_id=%u  role=%s  n_ues=%d  per_ue_quota=%.3f\n",
           ctx->id.nb_id.nb_id, ctx->is_relay ? "relay" : "access", n, per_ue);
    fflush(stdout);
}

/* ─── Common MAC indication handler ─────────────────────────────────────── */
static void sm_cb_common(sm_ag_if_rd_t const *rd, int idx)
{
    if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0) return;
    if (rd->ind.type != MAC_STATS_V0) return;

    node_ctx_t *ctx = &g_ctx[idx];
    ctx->last_ts = time(NULL);

    mac_ind_data_t const     *ind   = &rd->ind.mac;
    uint32_t                  nue   = ind->msg.len_ue_stats;
    mac_ue_stats_impl_t const *stats = ind->msg.ue_stats;

    if (nue == 0 || stats == NULL) return;

    int n = (int)((nue < MAX_UE_COUNT) ? nue : MAX_UE_COUNT);

    pthread_mutex_lock(&ctx->lock);
    ctx->num_ues  = n;
    ctx->bsr_sum  = 0.0;
    for (int i = 0; i < n; i++) {
        ctx->rnti[i]   = stats[i].rnti;
        uint64_t d     = delta_tbs(ctx, i, stats[i].dl_aggr_tbs);
        ctx->bsr_sum  += (double)d;
    }
    pthread_mutex_unlock(&ctx->lock);

    /* Adaptive: recompute ratios every ADAPTIVE_INTERVAL callbacks (global) */
    if (g_policy == POLICY_ADAPTIVE) {
        static uint32_t s_tick = 0;
        if (++s_tick % ADAPTIVE_INTERVAL == 0)
            adaptive_update();
    }

    send_ctrl(ctx);
}

/* Five static callback wrappers — one per node slot (C has no closures). */
static void sm_cb_0(sm_ag_if_rd_t const *rd) { sm_cb_common(rd, 0); }
static void sm_cb_1(sm_ag_if_rd_t const *rd) { sm_cb_common(rd, 1); }
static void sm_cb_2(sm_ag_if_rd_t const *rd) { sm_cb_common(rd, 2); }
static void sm_cb_3(sm_ag_if_rd_t const *rd) { sm_cb_common(rd, 3); }
static void sm_cb_4(sm_ag_if_rd_t const *rd) { sm_cb_common(rd, 4); }

typedef void (*mac_cb_t)(sm_ag_if_rd_t const *);
static mac_cb_t const CB_TABLE[MAX_NODES] = {
    sm_cb_0, sm_cb_1, sm_cb_2, sm_cb_3, sm_cb_4,
    sm_cb_0, sm_cb_0, sm_cb_0   /* unused padding */
};

/* ─── Watchdog thread ────────────────────────────────────────────────────── */
static void *watchdog_thread(void *arg)
{
    (void)arg;
    sleep(60); /* initial grace: allow subscriptions to warm up */
    while (1) {
        sleep(10);
        time_t now = time(NULL);
        for (int i = 0; i < g_nctx; i++) {
            time_t last = g_ctx[i].last_ts;
            if (last > 0 && (now - last) > WATCHDOG_TIMEOUT_S) {
                fprintf(stderr, "[PRB] Watchdog: node nb_id=%u silent %lds, exit\n",
                        g_ctx[i].id.nb_id.nb_id, (long)(now - last));
                exit(1);
            }
        }
    }
    return NULL;
}

/* ─── Policy parsing ─────────────────────────────────────────────────────── */
static policy_t parse_policy(void)
{
    const char *p = getenv("PRB_POLICY");
    if (!p) p = "fixed50";
    if (strcmp(p, "fixed30")  == 0) return POLICY_FIXED30;
    if (strcmp(p, "fixed10")  == 0) return POLICY_FIXED10;
    if (strcmp(p, "adaptive") == 0) return POLICY_ADAPTIVE;
    return POLICY_FIXED50;
}

static void init_ratios(policy_t pol)
{
    switch (pol) {
        case POLICY_FIXED30:  g_bh_ratio = 0.30; g_ac_ratio = 0.70; break;
        case POLICY_FIXED10:  g_bh_ratio = 0.10; g_ac_ratio = 0.90; break;
        case POLICY_ADAPTIVE: g_bh_ratio = 0.50; g_ac_ratio = 0.50; break;
        default:              g_bh_ratio = 0.50; g_ac_ratio = 0.50; break;
    }
}

/* ─── main ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    /* Make stdout unbuffered so log is written even if killed with -9 */
    setvbuf(stdout, NULL, _IONBF, 0);

    g_policy = parse_policy();
    init_ratios(g_policy);

    printf("====================================================\n"
           "  PRB Allocation xApp — %s\n"
           "  BH=%.0f%%  AC=%.0f%%  PRB_total=%d\n"
           "====================================================\n",
           POLICY_NAMES[g_policy], g_bh_ratio * 100, g_ac_ratio * 100,
           TOTAL_PRB_COUNT);

    /* Init FlexRIC xApp API */
    fr_args_t args = init_fr_args(argc, argv);
    init_xapp_api(&args);
    sleep(1);

    /* Wait for E2 nodes to connect (up to 120 s) */
    printf("[PRB] Waiting for E2 nodes...\n");
    e2_node_arr_xapp_t nodes = {0};
    for (int waited = 0; waited < 120 && nodes.len == 0; waited += 2) {
        if (waited > 0) free_e2_node_arr_xapp(&nodes);
        nodes = e2_nodes_xapp_api();
        if (nodes.len == 0) sleep(2);
    }
    if (nodes.len == 0) {
        fprintf(stderr, "[PRB] No E2 nodes found after 120 s, exit\n");
        return EXIT_FAILURE;
    }
    printf("[PRB] %d E2 node(s) connected\n", nodes.len);

    /* Build node context list (skip Donor DU 3584 — not an IAB relay) */
    g_nctx = 0;
    for (int i = 0; i < nodes.len && g_nctx < MAX_NODES - 3; i++) {
        uint32_t nb = nodes.n[i].id.nb_id.nb_id;
        if (nb == DONOR_NB_ID) {
            printf("[PRB] Skipping Donor DU nb_id=%u\n", nb);
            continue;
        }
        node_ctx_t *c = &g_ctx[g_nctx];
        /* Deep copy — global_e2_node_id_t contains uint64_t* cu_du_id pointer;
         * shallow assignment then free_e2_node_arr_xapp() leaves dangling ptr. */
        c->id       = cp_global_e2_node_id(&nodes.n[i].id);
        c->is_relay = is_relay_nb(nb);
        c->num_ues  = 0;
        c->bsr_sum  = 0.0;
        c->last_ts  = time(NULL);
        pthread_mutex_init(&c->lock, NULL);
        printf("[PRB] ctx[%d] nb_id=%u  role=%s\n",
               g_nctx, nb, c->is_relay ? "relay" : "access");
        g_nctx++;
    }
    free_e2_node_arr_xapp(&nodes);

    /* Subscribe to MAC SM on each node */
    for (int i = 0; i < g_nctx; i++) {
        g_ctx[i].sub = report_sm_xapp_api(
            &g_ctx[i].id, SM_MAC_ID, "100_ms", CB_TABLE[i]);
        if (!g_ctx[i].sub.success)
            fprintf(stderr, "[PRB] MAC SM sub failed for ctx[%d]\n", i);
        else
            printf("[PRB] Subscribed ctx[%d] nb_id=%u (handle=%d)\n",
                   i, g_ctx[i].id.nb_id.nb_id, g_ctx[i].sub.u.handle);
    }

    /* Start watchdog */
    pthread_t wd;
    if (pthread_create(&wd, NULL, watchdog_thread, NULL) == 0)
        pthread_detach(wd);

    printf("[PRB] Control loop active — policy=%s\n", POLICY_NAMES[g_policy]);
    xapp_wait_end_api();

    /* Cleanup */
    for (int i = 0; i < g_nctx; i++) {
        if (g_ctx[i].sub.success)
            rm_report_sm_xapp_api(g_ctx[i].sub.u.handle);
    }
    printf("[PRB] Exiting cleanly.\n");
    return EXIT_SUCCESS;
}
