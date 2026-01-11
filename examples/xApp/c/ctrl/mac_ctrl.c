#include "xApp/e42_xapp_api.h"
#include "sm/mac_sm/ie/mac_data_ie.h"
#include "sm/mac_sm/mac_sm_id.h" 

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h> 
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#ifndef SM_MAC_ID
#define SM_MAC_ID 142
#endif

// ==========================================
// 全域變數 & 訊號處理
// ==========================================
int vip_mcs = 0; // 改為追蹤最高 MCS
float current_vip_ratio = 0.5; 
volatile sig_atomic_t keep_running = 1;

void sig_handler(int signo) {
    if (signo == SIGINT) {
        printf("\n\n[xApp] >>> 收到停止訊號 (Ctrl+C)，準備優雅退場... <<<\n");
        keep_running = 0;
    }
}

// ==========================================
// 發送切片定義 (Control)
// ==========================================
void send_slice_config(e2_node_arr_xapp_t* nodes, float vip_ratio) {
    if (nodes->len == 0) return;

    if (vip_ratio > 0.90) vip_ratio = 0.90;
    if (vip_ratio < 0.10) vip_ratio = 0.10;

    float std_ratio = 1.0 - vip_ratio;

    if (fabs(vip_ratio - current_vip_ratio) < 0.01) {
        return; 
    }

    printf("[xApp] 🔄 Updating Slice Ratio -> VIP (High Eff): %.2f, STD: %.2f\n", vip_ratio, std_ratio);

    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy = 0;
    req.msg.type = 0; 
    req.msg.len_slices = 2;
    req.msg.slices = calloc(2, sizeof(mac_slice_params_t));
    if(req.msg.slices == NULL) return;

    req.msg.slices[0].id = 1; // VIP 切片 (高效率優先)
    req.msg.slices[0].percentage = vip_ratio;
    req.msg.slices[1].id = 2; // 普通切片
    req.msg.slices[1].percentage = std_ratio;

    sm_ag_if_wr_t wr = {0};
    wr.type = CONTROL_SM_AG_IF_WR;      
    wr.ctrl.type = MAC_CTRL_REQ_V0;     
    wr.ctrl.mac_ctrl = req;             

    for (int i = 0; i < nodes->len; i++) {
        sm_ans_xapp_t ans = control_sm_xapp_api(&nodes->n[i].id, SM_MAC_ID, &wr);
        if (ans.success) {
            printf("[xApp] ✅ Control Success Node %d!\n", i);
            current_vip_ratio = vip_ratio; 
        }
    }
    free(req.msg.slices);
}

// ==========================================
// 回呼函式：處理 MAC Indication (尋找最高 MCS)
// ==========================================
void mac_cb(sm_ag_if_rd_t const* rd) {
    if (rd->type != INDICATION_MSG_AGENT_IF_ANS_V0) return;
    if (rd->ind.type != MAC_STATS_V0) return;

    mac_ind_msg_t const* msg = &rd->ind.mac.msg;
    uint32_t num_ue = msg->len_ue_stats;
    
    printf("\n[DEBUG] --- MAC Report (UE Count: %u) ---\n", num_ue);

    if (num_ue == 0) {
        vip_mcs = 0; 
        return;
    }

    int highest_mcs = 0; // 從 0 開始找最高者
    for (size_t i = 0; i < num_ue; i++) {
        uint16_t rnti = msg->ue_stats[i].rnti;
        int mcs = (int)msg->ue_stats[i].dl_mcs1; 
        
        printf("[DEBUG] UE [%zu] RNTI: %04x | MCS: %d\n", i, rnti, mcs);

        // 策略：尋找目前頻道品質最好的 UE
        if (mcs > highest_mcs) {
            highest_mcs = mcs;
        }
    }

    // 更新全域變數，代表當前系統能達到的最高傳輸效率
    vip_mcs = highest_mcs; 
}

// ==========================================
// Main
// ==========================================
int main(int argc, char *argv[]) {
    signal(SIGINT, sig_handler);

    fr_args_t args = init_fr_args(argc, argv);
    init_xapp_api(&args); 
    sleep(1);

    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();

    while (nodes.len == 0 && keep_running) {
        printf("[xApp] 等待 gNB 連線中...\n");
        sleep(1);
        nodes = e2_nodes_xapp_api(); 
    }
    
    if (!keep_running) goto cleanup;

    printf("[xApp] 偵測到 %d 個 gNB！\n", nodes.len);

    long interval_ms = 1000; 
    for (int i = 0; i < nodes.len; i++) {
        report_sm_xapp_api(&nodes.n[i].id, SM_MAC_ID, &interval_ms, mac_cb);
    }

    printf("[xApp] 演算法啟動：基於吞吐量最大化 (Throughput Maximization) 的切片優化...\n");

    while(keep_running) {
        sleep(1); 
        
        // 顯示目前最高效能的 MCS 狀態
        printf("[DEBUG] Max MCS detected: %d | Current Ratio: %.2f\n", vip_mcs, current_vip_ratio);
        
        float new_vip_ratio = 0.50;

        /* 
         * 核心策略：效能激勵 (Performance Incentive)
         * 訊號越好的人，代表每個 PRB 能換取的 Bit 越多，
         * 我們給予更多資源以衝高基地台的總產出 (Total Throughput)。
         */
        if (vip_mcs >= 25) {
            new_vip_ratio = 0.85; // 訊號極佳，分配 85% 資源全力衝刺
            printf(">>> 🚀 訊號優秀 (%d)，資源傾斜至高效率用戶！\n", vip_mcs);
        } 
        else if (vip_mcs >= 15) {
            new_vip_ratio = 0.65; // 訊號良好，給予較多資源
            printf(">>> 📈 訊號良好 (%d)，增加資源分配。\n", vip_mcs);
        }
        else if (vip_mcs > 0 && vip_mcs < 10) {
            new_vip_ratio = 0.20; // 訊號太差，減少資源浪費，保留給高效率者
            printf(">>> 📉 訊號不佳 (%d)，降低資源分配以維持系統總效能。\n", vip_mcs);
        }
        else {
            new_vip_ratio = 0.50; // 中庸狀態，公平分配
        }

        send_slice_config(&nodes, new_vip_ratio);
    }

cleanup:
    printf("\n[xApp] 正在清理資源並結束...\n");
    free_e2_node_arr_xapp(&nodes); 
    printf("[xApp] Bye Bye!\n");
    
    return 0;
}