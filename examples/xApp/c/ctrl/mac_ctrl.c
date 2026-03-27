#define PLAIN 

#include "xApp/e42_xapp_api.h"
#include "sm/mac_sm/ie/mac_data_ie.h"
#include "sm/mac_sm/mac_sm_id.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

// =================================================================================
// [Node 3 論文場景參數 - 2D 資源精準控制]
// =================================================================================
#define TARGET_NODE_3_ID 3587           // 鎖定 Node 3 的實體 ID

#define THRESHOLD_EMBB_CONGEST 8000000  // 8 MB (觸發 eMBB 限速門檻)
#define THRESHOLD_EMBB_IDENTIFY 1000000 // 1 MB (用來區分 eMBB 與 mMTC)

#define POLICY_RATIO_EMBB_LIMIT 0.50f   // eMBB 擁塞時限縮至 50%
#define POLICY_RATIO_MMTC_GUARD 0.10f   // mMTC 保障 10%
#define POLICY_RATIO_FULL       1.00f   // 正常狀態 100%

#define MASK_EMBB   0xAAAA   // 時域交錯 (10101010)
#define MASK_NORMAL 0xFFFF   // 時域全開 (11111111)

#define MAX_UE_COUNT 16

// [視覺化顏色控制]
#define CLR_RED    "\x1b[31m"
#define CLR_GREEN  "\x1b[32m"
#define CLR_YEL    "\x1b[33m"
#define CLR_CYAN   "\x1b[36m"
#define CLR_RESET  "\x1b[0m"

// =================================================================================
// [全域數據庫] 用於動態追蹤 RNTI 狀態
// =================================================================================
typedef struct {
    uint16_t rnti;
    uint32_t buffer;
    bool is_active;
} ue_live_data_t;

ue_live_data_t g_ue_db[MAX_UE_COUNT];
pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;
int g_active_ue_num = 0;

// =================================================================================
// [感測回呼函式] 接收來自 gNB 的最新數據
// =================================================================================
static void sm_cb_mac(sm_ag_if_rd_t const* rd)
{
    char* base = (char*)rd;
    uint32_t num_ues = *(uint32_t*)(base + 24);
    mac_ue_stats_impl_t* stats = *(mac_ue_stats_impl_t**)(base + 32);

    pthread_mutex_lock(&g_db_lock);
    memset(g_ue_db, 0, sizeof(g_ue_db));
    g_active_ue_num = (num_ues > MAX_UE_COUNT) ? MAX_UE_COUNT : (int)num_ues;

    if (num_ues > 0 && stats != NULL) {
        for (int i = 0; i < g_active_ue_num; i++) {
            g_ue_db[i].rnti   = stats[i].rnti;
            g_ue_db[i].buffer = stats[i].dl_buffer_info;
            g_ue_db[i].is_active = true;
        }
    }
    pthread_mutex_unlock(&g_db_lock);
}

void format_buf(uint32_t b) {
    if (b > 1048576) printf("%.2f MB", b/1048576.0);
    else if (b > 1024) printf("%.2f KB", b/1024.0);
    else printf("%u B", b);
}

// =================================================================================
// [主程式控制迴圈]
// =================================================================================
int main(int argc, char *argv[])
{
  // 1. 初始化 xApp API
  fr_args_t args = init_fr_args(argc, argv);
  init_xapp_api(&args);
  sleep(1);

  // 2. 穩健獲取目標 Node 3 (直到連線上為止)
  e2_node_arr_xapp_t nodes;
  int target_idx = -1;

  while (target_idx == -1) {
      nodes = e2_nodes_xapp_api();
      
      if (nodes.len == 0) {
          printf(CLR_YEL "[Wait] RIC has no registered nodes. Retrying in 2s...\n" CLR_RESET);
      } else {
          for (int i = 0; i < nodes.len; i++) {
              // 修正點：必須連續呼叫兩次 nb_id 才能取得整數數值
              if (nodes.n[i].id.nb_id.nb_id == TARGET_NODE_3_ID) {
                  target_idx = i;
                  break;
              }
          }
          if (target_idx == -1) {
              printf(CLR_YEL "[Wait] Found %d nodes, but Node 3 (%d) is not connected yet. Retrying...\n" CLR_RESET, nodes.len, TARGET_NODE_3_ID);
              free_e2_node_arr_xapp(&nodes); 
          }
      }
      if (target_idx == -1) sleep(2);
  }

  // 鎖定 Node 3 實體地址
  global_e2_node_id_t *target_node_id = &nodes.n[target_idx].id;
  // 修正點：列印時也必須加上第二層的 .nb_id
  printf(CLR_GREEN "[xApp] Successfully locked Node 3 (nb_id: %d)\n" CLR_RESET, target_node_id->nb_id.nb_id);

  // 3. 訂閱 MAC SM 數據 (僅針對 Node 3)
  report_sm_xapp_api(target_node_id, SM_MAC_ID, "1_ms", sm_cb_mac);

  printf(CLR_CYAN "====================================================\n");
  printf("  Node 3 Local xApp: Precision Resource Controller\n");
  printf("  Target Node ID: %d\n", TARGET_NODE_3_ID);
  printf("====================================================\n" CLR_RESET);

  // 4. 控制決策無限迴圈
  while(1) {
      sleep(1); 

      pthread_mutex_lock(&g_db_lock);
      if (g_active_ue_num == 0) {
          // 修正點：列印時也必須加上第二層的 .nb_id
          printf("\r" CLR_YEL "[Node 3] Scanning for active UEs on Node %d...          " CLR_RESET, target_node_id->nb_id.nb_id);
          fflush(stdout);
          pthread_mutex_unlock(&g_db_lock);
          continue;
      }

      printf("\n" CLR_CYAN "[Node 3 Status Report]" CLR_RESET "\n");

      mac_ctrl_req_data_t req = {0};
      req.msg.len_slices = g_active_ue_num;
      req.msg.slices = calloc(g_active_ue_num, sizeof(mac_slice_params_t));
      assert(req.msg.slices != NULL);

      for (int i = 0; i < g_active_ue_num; i++) {
          uint16_t rnti = g_ue_db[i].rnti;
          uint32_t buf  = g_ue_db[i].buffer;

          printf("  - UE[%04x]: Buffer ", rnti);
          format_buf(buf);

          if (buf > THRESHOLD_EMBB_CONGEST) {
              printf(CLR_RED " -> [eMBB] Congested: Limit to 50%% PRB\n" CLR_RESET);
              req.msg.slices[i].id = rnti;
              req.msg.slices[i].prb_quota = POLICY_RATIO_EMBB_LIMIT;
              req.msg.slices[i].slot_mask = MASK_EMBB;
          } 
          else if (buf > 0 && buf < THRESHOLD_EMBB_IDENTIFY) {
              printf(CLR_GREEN " -> [mMTC] Priority: Guard 10%% PRB\n" CLR_RESET);
              req.msg.slices[i].id = rnti;
              req.msg.slices[i].prb_quota = POLICY_RATIO_MMTC_GUARD;
              req.msg.slices[i].slot_mask = MASK_NORMAL; 
          }
          else {
              printf(" -> [Normal] Full Resource Access\n");
              req.msg.slices[i].id = rnti;
              req.msg.slices[i].prb_quota = POLICY_RATIO_FULL;
              req.msg.slices[i].slot_mask = MASK_NORMAL;
          }
      }

      // 5. 下發控制訊息 (加入跨機通訊延遲緩衝)
      req.hdr.dummy = 0;
      req.msg.type = 0; 
      
      usleep(50000); // 50ms 延遲，防止 SCTP 瞬間過載
      control_sm_xapp_api(target_node_id, SM_MAC_ID, &req);

      free(req.msg.slices);
      pthread_mutex_unlock(&g_db_lock);
      printf(CLR_CYAN "----------------------------------------------------\n" CLR_RESET);
  }

  free_e2_node_arr_xapp(&nodes);
  return 0;
}