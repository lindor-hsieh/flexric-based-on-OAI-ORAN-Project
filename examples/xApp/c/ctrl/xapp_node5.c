/*
 * =============================================================================
 *  xapp_node5.c — Node 5 Local xApp (Near-RT 閉環控制)
 *
 *  架構角色：
 *    - 部署於 PC 2 的 Docker 容器，專職控制 IAB Node 5
 *    - 透過 E2SM-MAC 每 10ms 收集 UE 狀態
 *    - 透過 ZeroMQ REQ/REP 呼叫 Python AI 推論伺服器取得 PRB 分配決策
 *    - 將決策透過 control_sm_xapp_api() 寫回 OAI MAC 層
 *
 *  ZMQ 協定：
 *    發送 (C → Python):
 *      {"node_id": 3589, "ues": [{"rnti": X, "bsr": Y, "wb_cqi": Z}, ...]}
 *    接收 (Python → C):
 *      {"allocations": [{"rnti": X, "prb_abs": N}, ...]}
 *      prb_abs 為絕對 PRB 數量，C 端將除以 TOTAL_PRB_COUNT 轉換為比例
 *
 *  Fallback 機制：
 *    當 ZMQ 逾時 (5ms) 或 JSON 解析失敗時，自動回退至等比例 PRB 分配，
 *    確保 OAI MAC 層不因推論伺服器無回應而當機。
 *
 *  編譯指令 (在 flexric/build 目錄下)：
 *    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
 *          -DKPM_VERSION=KPM_V3_00 -DE2AP_VERSION=E2AP_V2 ..
 *    ninja
 *  連結時需加入: -lzmq -lcjson
 * =============================================================================
 */

/* 強制使用明文 (Plain) 編碼，不使用 ASN.1 或 Flatbuffers */
#define PLAIN

/* ── FlexRIC 公開 API ───────────────────────────────────────────────────── */
#include "xApp/e42_xapp_api.h"
#include "sm/mac_sm/ie/mac_data_ie.h"
#include "sm/mac_sm/mac_sm_id.h"

/* ── 跨語言 IPC ─────────────────────────────────────────────────────────── */
#include <zmq.h>
#include <cjson/cJSON.h>

/* ── 標準函式庫 ─────────────────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

/* =============================================================================
 * 全域常數
 * =========================================================================== */

/* 鎖定目標節點 ID (Node 5 的 nb_id = 0xe05 = 3589) */
const uint32_t TARGET_NODE_ID = 3589;

/* ZMQ IPC 路徑：Python 推論伺服器監聽此位址 */
#define ZMQ_ENDPOINT      "ipc:///tmp/zmq_node5_inference.ipc"

/* ZMQ 收發逾時 (毫秒)，防止 Python 端卡死拖垮 OAI MAC 迴圈 */
#define ZMQ_TIMEOUT_MS    5

/* 每個節點最多同時服務的 UE 數量 */
#define MAX_UE_COUNT      16

/* 系統頻寬 PRB 總數 (依實際部署調整：106=100MHz, 52=20MHz, 25=10MHz) */
#define TOTAL_PRB_COUNT   106

/* Slot 遮罩常數 */
#define SLOT_MASK_FULL    0xFFFF   /* 時域全開，不限制 slot */

/* 終端機顏色控制碼，方便 Debug 輸出辨識 */
#define CLR_RED    "\x1b[31m"
#define CLR_GREEN  "\x1b[32m"
#define CLR_YEL    "\x1b[33m"
#define CLR_CYAN   "\x1b[36m"
#define CLR_RESET  "\x1b[0m"

/* =============================================================================
 * 全域狀態變數 (main 初始化後為唯讀，callback 只讀取)
 * =========================================================================== */

/*
 * 目標節點的 E2 Node ID 指標。
 * 指向 main() 中 nodes 陣列的元素，nodes 陣列在程式結束前不會被 free。
 * callback 用它來呼叫 control_sm_xapp_api()。
 */
static global_e2_node_id_t *g_target_node_id = NULL;

/* ZeroMQ context (整個程式共用一個) */
static void *g_zmq_ctx  = NULL;

/* ZeroMQ REQ socket (僅在 FlexRIC callback 執行緒中使用，無需加鎖) */
static void *g_zmq_sock = NULL;

/* =============================================================================
 * zmq_socket_init()
 *   建立或重建 ZMQ REQ socket 並連線至 Python 推論伺服器。
 *   當 recv() 逾時導致 REQ 狀態機卡住時，也呼叫此函式重置。
 *
 * 回傳: true 表示成功，false 表示失敗
 * =========================================================================== */
static bool zmq_socket_init(void)
{
    /* 若舊 socket 存在，先關閉以釋放資源 */
    if (g_zmq_sock != NULL) {
        zmq_close(g_zmq_sock);
        g_zmq_sock = NULL;
    }

    /* 建立新的 REQ socket */
    g_zmq_sock = zmq_socket(g_zmq_ctx, ZMQ_REQ);
    if (g_zmq_sock == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] zmq_socket() 失敗: %s\n" CLR_RESET,
                zmq_strerror(zmq_errno()));
        return false;
    }

    /* 設定發送逾時 (毫秒)：防止 Python 端斷線後 send() 永久阻塞 */
    int timeout_ms = ZMQ_TIMEOUT_MS;
    zmq_setsockopt(g_zmq_sock, ZMQ_SNDTIMEO, &timeout_ms, sizeof(int));

    /* 設定接收逾時 (毫秒)：防止 Python 端無回應時 recv() 永久阻塞 */
    zmq_setsockopt(g_zmq_sock, ZMQ_RCVTIMEO, &timeout_ms, sizeof(int));

    /*
     * ZMQ_REQ_RELAXED = 1：
     *   允許 REQ socket 在上一個 reply 尚未收到時就發送新的 request。
     *   這是 recv() 逾時後繼續工作的關鍵選項 (libzmq >= 4.1 支援)。
     */
    int relaxed = 1;
    zmq_setsockopt(g_zmq_sock, ZMQ_REQ_RELAXED, &relaxed, sizeof(int));

    /*
     * ZMQ_LINGER = 0：
     *   關閉 socket 時立即丟棄所有待送訊息，避免程式結束時卡住。
     */
    int linger = 0;
    zmq_setsockopt(g_zmq_sock, ZMQ_LINGER, &linger, sizeof(int));

    /* 連線至 Python 推論伺服器 (IPC，不需要對方先啟動) */
    if (zmq_connect(g_zmq_sock, ZMQ_ENDPOINT) != 0) {
        fprintf(stderr, CLR_RED "[Node5 xApp] zmq_connect(%s) 失敗: %s\n" CLR_RESET,
                ZMQ_ENDPOINT, zmq_strerror(zmq_errno()));
        zmq_close(g_zmq_sock);
        g_zmq_sock = NULL;
        return false;
    }

    return true;
}

/* =============================================================================
 * apply_fallback()
 *   Fallback 排程策略：對所有活躍 UE 進行等比例 PRB 分配。
 *   當 ZMQ 通訊失敗時呼叫，確保 OAI MAC 層能繼續正常運作。
 *
 * 參數:
 *   num_ues  - 活躍 UE 數量
 *   stats    - 指向 MAC 狀態陣列 (用來取得各 UE 的 RNTI)
 * =========================================================================== */
static void apply_fallback(uint32_t num_ues, mac_ue_stats_impl_t const *stats)
{
    /* 限制最大 UE 數，避免 calloc 過大 */
    uint32_t n = (num_ues > MAX_UE_COUNT) ? (uint32_t)MAX_UE_COUNT : num_ues;

    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy      = 0;
    req.msg.type       = 0;   /* 0 = Resource Allocation */
    req.msg.len_slices = n;
    req.msg.slices     = calloc(n, sizeof(mac_slice_params_t));
    if (req.msg.slices == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] Fallback: calloc 失敗，跳過本輪控制\n" CLR_RESET);
        return;
    }

    /* 等比例分配：每個 UE 得到 1/n 的 PRB 配額 */
    float equal_ratio = 1.0f / (float)n;
    for (uint32_t i = 0; i < n; i++) {
        req.msg.slices[i].id        = stats[i].rnti;
        req.msg.slices[i].prb_quota = equal_ratio;
        req.msg.slices[i].slot_mask = SLOT_MASK_FULL;
        req.msg.slices[i].priority  = 0;
    }

    /* 下發控制訊息 */
    control_sm_xapp_api(g_target_node_id, SM_MAC_ID, &req);

    /* 釋放動態分配的切片陣列，防止 Memory Leak */
    free(req.msg.slices);
}

/* =============================================================================
 * sm_cb_mac()
 *   MAC Indication Callback — 每 10ms 由 FlexRIC 框架觸發一次。
 *   執行完整的感測 → 推論 → 控制閉環。
 *
 * 參數:
 *   rd - FlexRIC 傳入的感測資料指標 (sm_ag_if_rd_t)
 * =========================================================================== */
static void sm_cb_mac(sm_ag_if_rd_t const *rd)
{
    assert(rd != NULL);

    /* ── [Guard 1] 確認訊息是 Indication，不處理其他類型 ────────────────── */
    if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0) return;

    /* ── [Guard 2] 確認是 MAC SM 的 Indication ───────────────────────────── */
    if (rd->ind.type != MAC_STATS_V0) return;

    /* ── [Step 0] 擷取 MAC 狀態資料 ─────────────────────────────────────── */
    mac_ind_data_t const     *mac_ind  = &rd->ind.mac;
    uint32_t                  num_ues  = mac_ind->msg.len_ue_stats;
    mac_ue_stats_impl_t const *stats   = mac_ind->msg.ue_stats;

    /* 若無活躍 UE 或 ZMQ socket 尚未就緒，靜默跳過 */
    if (num_ues == 0 || stats == NULL || g_zmq_sock == NULL) return;

    /* ── [Step 1] 序列化 UE 狀態為 JSON ──────────────────────────────────
     *   格式範例:
     *   {
     *     "node_id": 3589,
     *     "ues": [
     *       {"rnti": 12345, "bsr": 1024, "wb_cqi": 12},
     *       ...
     *     ]
     *   }
     * ─────────────────────────────────────────────────────────────────── */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] cJSON_CreateObject 失敗，Fallback\n" CLR_RESET);
        apply_fallback(num_ues, stats);
        return;
    }

    /* 寫入節點識別 ID */
    cJSON_AddNumberToObject(root, "node_id", (double)TARGET_NODE_ID);

    /* 建立 UE 陣列並填入各 UE 的關鍵狀態 */
    cJSON *ue_array = cJSON_AddArrayToObject(root, "ues");
    if (ue_array == NULL) {
        cJSON_Delete(root);
        apply_fallback(num_ues, stats);
        return;
    }

    uint32_t n = (num_ues > MAX_UE_COUNT) ? (uint32_t)MAX_UE_COUNT : num_ues;
    for (uint32_t i = 0; i < n; i++) {
        cJSON *ue_obj = cJSON_CreateObject();
        if (ue_obj == NULL) continue;

        /* rnti：UE 識別碼 */
        cJSON_AddNumberToObject(ue_obj, "rnti",   (double)stats[i].rnti);
        /* bsr：Buffer Status Report，反映下行待送資料量 */
        cJSON_AddNumberToObject(ue_obj, "bsr",    (double)stats[i].bsr);
        /* wb_cqi：Wideband Channel Quality Indicator，反映通道品質 */
        cJSON_AddNumberToObject(ue_obj, "wb_cqi", (double)stats[i].wb_cqi);

        cJSON_AddItemToArray(ue_array, ue_obj);
    }

    /* 序列化為 JSON 字串 (不含縮排，降低傳輸大小) */
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);   /* root 已序列化完畢，立即釋放避免 Memory Leak */

    if (payload == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] cJSON 序列化失敗，Fallback\n" CLR_RESET);
        apply_fallback(num_ues, stats);
        return;
    }

    /* ── [Step 2] 透過 ZMQ 發送 JSON 至 Python 推論伺服器 ───────────────
     *   ZMQ_SNDTIMEO = 5ms，若超時回傳 EAGAIN
     * ─────────────────────────────────────────────────────────────────── */
    int send_rc = zmq_send(g_zmq_sock, payload, strlen(payload), 0);
    free(payload);   /* cJSON_PrintUnformatted 回傳的字串需手動 free */

    if (send_rc < 0) {
        fprintf(stderr, CLR_YEL "[Node5 xApp] ZMQ send 失敗 (%s)，Fallback\n" CLR_RESET,
                zmq_strerror(zmq_errno()));
        apply_fallback(num_ues, stats);
        return;
    }

    /* ── [Step 3] 接收 Python 推論結果 ──────────────────────────────────
     *   ZMQ_RCVTIMEO = 5ms，若超時回傳 EAGAIN
     *   超時後需重建 socket，否則 REQ 狀態機卡住導致下一輪 send 也失敗
     * ─────────────────────────────────────────────────────────────────── */
    char recv_buf[8192];
    memset(recv_buf, 0, sizeof(recv_buf));
    int recv_rc = zmq_recv(g_zmq_sock, recv_buf, sizeof(recv_buf) - 1, 0);

    if (recv_rc < 0) {
        fprintf(stderr, CLR_YEL "[Node5 xApp] ZMQ recv 逾時 (%s)，重建 socket 並 Fallback\n" CLR_RESET,
                zmq_strerror(zmq_errno()));
        zmq_socket_init();
        apply_fallback(num_ues, stats);
        return;
    }
    recv_buf[recv_rc] = '\0';   /* 確保字串以 null 結尾 */

    /* ── [Step 4] 解析 Python 回傳的 PRB 分配 JSON ───────────────────────
     *   期望格式:
     *   {
     *     "allocations": [
     *       {"rnti": 12345, "prb_abs": 30},
     *       {"rnti": 23456, "prb_abs": 50},
     *       ...
     *     ]
     *   }
     *   prb_abs 為 AI 決策分配給該 UE 的絕對 PRB 數量。
     *   C 端將除以 TOTAL_PRB_COUNT 轉換為 0.0~1.0 的比例。
     * ─────────────────────────────────────────────────────────────────── */
    cJSON *resp = cJSON_Parse(recv_buf);
    if (resp == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] 回傳 JSON 解析失敗，Fallback\n" CLR_RESET);
        apply_fallback(num_ues, stats);
        return;
    }

    cJSON *allocs_arr = cJSON_GetObjectItemCaseSensitive(resp, "allocations");
    if (!cJSON_IsArray(allocs_arr) || cJSON_GetArraySize(allocs_arr) == 0) {
        fprintf(stderr, CLR_RED "[Node5 xApp] 回傳格式無效 (缺少 'allocations' 陣列)，Fallback\n" CLR_RESET);
        cJSON_Delete(resp);
        apply_fallback(num_ues, stats);
        return;
    }

    int alloc_n = cJSON_GetArraySize(allocs_arr);

    /* ── [Step 5] 組裝 mac_ctrl_req_data_t 並下發至 OAI MAC 層 ──────────
     *
     *   mac_slice_params_t 欄位說明：
     *     .id        = UE 的 RNTI，MAC 層以此識別目標 UE
     *     .prb_quota = PRB 分配比例 (0.0 ~ 1.0)，由絕對 PRB 數轉換而來
     *     .slot_mask = 時域 Slot 遮罩，0xFFFF 表示允許使用所有 Slot
     *     .priority  = 搶佔優先級，0 為預設
     * ─────────────────────────────────────────────────────────────────── */
    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy      = 0;
    req.msg.type       = 0;   /* 0 = Resource Allocation Control */
    req.msg.len_slices = (uint32_t)alloc_n;
    req.msg.slices     = calloc(alloc_n, sizeof(mac_slice_params_t));

    if (req.msg.slices == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] calloc 失敗，Fallback\n" CLR_RESET);
        cJSON_Delete(resp);
        apply_fallback(num_ues, stats);
        return;
    }

    for (int i = 0; i < alloc_n; i++) {
        cJSON *item    = cJSON_GetArrayItem(allocs_arr, i);
        cJSON *j_rnti  = cJSON_GetObjectItemCaseSensitive(item, "rnti");
        cJSON *j_prb   = cJSON_GetObjectItemCaseSensitive(item, "prb_abs");

        /* 若 JSON 欄位格式不符，退回等比例分配作為保底 */
        if (!cJSON_IsNumber(j_rnti) || !cJSON_IsNumber(j_prb)) {
            req.msg.slices[i].id        = (num_ues > 0) ? stats[i % num_ues].rnti : 0;
            req.msg.slices[i].prb_quota = 1.0f / (float)alloc_n;
            req.msg.slices[i].slot_mask = SLOT_MASK_FULL;
            req.msg.slices[i].priority  = 0;
            continue;
        }

        /* 絕對 PRB 數 → 比例，並夾緊到合法範圍 [0.0, 1.0] */
        double prb_abs   = j_prb->valuedouble;
        double prb_ratio = prb_abs / (double)TOTAL_PRB_COUNT;
        if (prb_ratio < 0.0) prb_ratio = 0.0;
        if (prb_ratio > 1.0) prb_ratio = 1.0;

        req.msg.slices[i].id        = (uint32_t)j_rnti->valueint;
        req.msg.slices[i].prb_quota = (float)prb_ratio;
        req.msg.slices[i].slot_mask = SLOT_MASK_FULL;
        req.msg.slices[i].priority  = 0;

        printf("[Node5 xApp] AI 決策 UE[0x%04x]: prb_abs=%.0f → ratio=%.3f\n",
               req.msg.slices[i].id, prb_abs, prb_ratio);
    }

    /* 回傳 JSON 已解析完畢，立即釋放 */
    cJSON_Delete(resp);

    /* 下發 PRB 控制訊息至 OAI MAC 層 */
    control_sm_xapp_api(g_target_node_id, SM_MAC_ID, &req);

    /* 釋放動態分配的切片陣列，防止每 10ms 觸發一次的 Memory Leak */
    free(req.msg.slices);
}

/* =============================================================================
 * main()
 * =========================================================================== */
int main(int argc, char *argv[])
{
    printf(CLR_CYAN
           "====================================================\n"
           "  Node 5 Local xApp — Near-RT 閉環 PRB 控制器\n"
           "  目標節點 ID : %u\n"
           "  ZMQ 端點    : %s\n"
           "  回報週期    : 10 ms\n"
           "====================================================\n"
           CLR_RESET,
           TARGET_NODE_ID, ZMQ_ENDPOINT);

    /* ─────────────────────────────────────────────────────────────────────
     * [1] 初始化 FlexRIC xApp API
     * ─────────────────────────────────────────────────────────────────── */
    fr_args_t args = init_fr_args(argc, argv);
    init_xapp_api(&args);
    sleep(1);   /* 等待連線穩定 */

    /* ─────────────────────────────────────────────────────────────────────
     * [2] 初始化 ZeroMQ
     * ─────────────────────────────────────────────────────────────────── */
    g_zmq_ctx = zmq_ctx_new();
    if (g_zmq_ctx == NULL) {
        fprintf(stderr, CLR_RED "[Node5 xApp] zmq_ctx_new() 失敗，程式退出\n" CLR_RESET);
        return EXIT_FAILURE;
    }

    if (!zmq_socket_init()) {
        fprintf(stderr, CLR_RED "[Node5 xApp] ZMQ socket 初始化失敗，程式退出\n" CLR_RESET);
        zmq_ctx_destroy(g_zmq_ctx);
        return EXIT_FAILURE;
    }
    printf(CLR_GREEN "[Node5 xApp] ZMQ REQ socket 已連線至 %s\n" CLR_RESET, ZMQ_ENDPOINT);

    /* ─────────────────────────────────────────────────────────────────────
     * [3] 尋找目標節點 (阻塞等待，直到 Node 5 連線上 FlexRIC Server)
     * ─────────────────────────────────────────────────────────────────── */
    e2_node_arr_xapp_t nodes;
    int target_idx = -1;

    while (target_idx == -1) {
        nodes = e2_nodes_xapp_api();

        for (int i = 0; i < nodes.len; i++) {
            if (nodes.n[i].id.nb_id.nb_id == TARGET_NODE_ID) {
                target_idx = i;
                break;
            }
        }

        if (target_idx == -1) {
            printf(CLR_YEL "[Node5 xApp] 等待 Node 5 (ID=%u) 連線... "
                   "目前已連接節點數: %d，2 秒後重試\n" CLR_RESET,
                   TARGET_NODE_ID, nodes.len);
            free_e2_node_arr_xapp(&nodes);
            sleep(2);
        }
    }

    /* 鎖定目標節點 ID，callback 將使用此指標下發控制訊息 */
    g_target_node_id = &nodes.n[target_idx].id;
    printf(CLR_GREEN "[Node5 xApp] 成功鎖定 Node 5 (nb_id=%u)\n" CLR_RESET,
           g_target_node_id->nb_id.nb_id);

    /* ─────────────────────────────────────────────────────────────────────
     * [4] 訂閱 MAC SM Indication，回報週期 10ms
     * ─────────────────────────────────────────────────────────────────── */
    sm_ans_xapp_t sub_ans = report_sm_xapp_api(
        g_target_node_id,
        SM_MAC_ID,
        "10_ms",
        sm_cb_mac
    );

    if (!sub_ans.success) {
        fprintf(stderr, CLR_RED "[Node5 xApp] 訂閱 MAC SM 失敗，程式退出\n" CLR_RESET);
        free_e2_node_arr_xapp(&nodes);
        if (g_zmq_sock != NULL) zmq_close(g_zmq_sock);
        zmq_ctx_destroy(g_zmq_ctx);
        return EXIT_FAILURE;
    }
    printf(CLR_GREEN "[Node5 xApp] MAC SM 訂閱成功 (handle=%d)，"
           "閉環控制迴圈啟動中...\n" CLR_RESET, sub_ans.u.handle);

    /* ─────────────────────────────────────────────────────────────────────
     * [5] 主迴圈：阻塞等待直到收到 SIGINT/SIGTERM
     * ─────────────────────────────────────────────────────────────────── */
    xapp_wait_end_api();

    /* ─────────────────────────────────────────────────────────────────────
     * [6] 優雅退出：依序取消訂閱、關閉 ZMQ、釋放節點陣列
     * ─────────────────────────────────────────────────────────────────── */
    printf("[Node5 xApp] 收到停止訊號，開始清理資源...\n");

    rm_report_sm_xapp_api(sub_ans.u.handle);

    if (g_zmq_sock != NULL) {
        zmq_close(g_zmq_sock);
        g_zmq_sock = NULL;
    }
    if (g_zmq_ctx != NULL) {
        zmq_ctx_destroy(g_zmq_ctx);
        g_zmq_ctx = NULL;
    }

    free_e2_node_arr_xapp(&nodes);

    printf(CLR_GREEN "[Node5 xApp] 程式正常退出\n" CLR_RESET);
    return EXIT_SUCCESS;
}
