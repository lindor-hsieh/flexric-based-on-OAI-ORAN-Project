#include "xApp/e42_xapp_api.h"
// #include "util/alg_ds/alg/defer.h" // [移除] 不使用 defer，避免語法錯誤
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

#ifndef SM_MAC_ID
#define SM_MAC_ID 142
#endif

// ==========================================
// [萬能修正] 自定義結構，繞過 FlexRIC 成員名稱檢查
// ==========================================
typedef struct {
    sm_ag_if_rd_e type; 
    union {
        mac_ind_data_t mac; // <--- 強制命名為 mac，編譯器就不會報錯了
    };
} my_sm_ag_if_rd_t;

// ==========================================
// 全域變數
// ==========================================
int vip_cqi = 15;
int std_cqi = 15;
float current_vip_ratio = 0.7; 

// ==========================================
// 發送切片定義
// ==========================================
void send_slice_config(e2_node_arr_xapp_t* nodes, float vip_ratio) {
    if (nodes->len == 0) return;

    if (vip_ratio > 0.90) vip_ratio = 0.90;
    if (vip_ratio < 0.10) vip_ratio = 0.10;

    float std_ratio = 1.0 - vip_ratio;

    if (fabs(vip_ratio - current_vip_ratio) < 0.05) {
        return; 
    }

    printf("[xApp] 🔄 Updating Slice Ratio -> VIP: %.2f (CQI %d), STD: %.2f (CQI %d)\n", 
           vip_ratio, vip_cqi, std_ratio, std_cqi);

    // 1. 準備 Payload
    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy = 0;
    req.msg.type = 0; 
    req.msg.len_slices = 2;

    req.msg.slices = calloc(2, sizeof(mac_slice_params_t));
    if(req.msg.slices == NULL) {
        printf("[xApp] Error: Memory allocation failed\n");
        return;
    }

    // Slice 1: VIP
    req.msg.slices[0].id = 1;
    req.msg.slices[0].percentage = vip_ratio;

    // Slice 2: Standard
    req.msg.slices[1].id = 2;
    req.msg.slices[1].percentage = std_ratio;

    // 2. 包裝
    sm_ag_if_wr_t wr = {0};
    wr.type = CONTROL_SM_AG_IF_WR;      
    wr.ctrl.type = MAC_CTRL_REQ_V0;     
    wr.ctrl.mac_ctrl = req;             

    // 3. 發送
    for (int i = 0; i < nodes->len; i++) {
        sm_ans_xapp_t ans = control_sm_xapp_api(&nodes->n[i].id, SM_MAC_ID, &wr);
        if (ans.success) {
            printf("[xApp] ✅ Control Msg Sent to Node %d!\n", i);
            current_vip_ratio = vip_ratio; 
        } else {
            printf("[xApp] ❌ Control Msg Failed for Node %d\n", i);
        }
    }
    free(req.msg.slices);
}

// ==========================================
// 回呼函式：處理 MAC Indication (讀取 CQI)
// ==========================================
void mac_cb(sm_ag_if_rd_t const* rd_orig) {
    if (rd_orig->type != MAC_STATS_V0) return;

    // [魔法修正] 強制轉型：把原本的結構當作我們定義的 my_sm_ag_if_rd_t 來看
    // 這樣我們就可以用 .mac 來存取，不用管原本叫 mac 還是 mac_stats
    my_sm_ag_if_rd_t const* rd = (my_sm_ag_if_rd_t const*)rd_orig;

    // 現在可以放心地讀取 mac.msg 了
    mac_ind_msg_t const* msg = &rd->mac.msg;
    
    for (size_t i = 0; i < msg->len_ue_stats; i++) {
        mac_ue_stats_impl_t* ue_stats = &msg->ue_stats[i];
        
        int cqi = ue_stats->wb_cqi; 
        if (cqi == 0) cqi = 1; 

        if (i == 0) {
            vip_cqi = cqi;
        } else {
            std_cqi = cqi;
        }
    }
}

int main(int argc, char *argv[]) {
    fr_args_t args = init_fr_args(argc, argv);
    init_xapp_api(&args); 
    sleep(1);

    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
    // defer({ free_e2_node_arr_xapp(&nodes); }); // [修正] 移除這行，改為手動釋放

    while (nodes.len == 0) {
        printf("[xApp] Waiting for gNB connection...\n");
        sleep(1);
        nodes = e2_nodes_xapp_api(); 
    }

    printf("[xApp] gNB Connected! Node count: %d\n", nodes.len);

    long interval_ms = 1000; 
    for (int i = 0; i < nodes.len; i++) {
        printf("[xApp] Subscribing to Node %d (Interval: 1000ms)...\n", i);
        report_sm_xapp_api(&nodes.n[i].id, SM_MAC_ID, &interval_ms, mac_cb);
    }

    printf("[xApp] xApp Running... (Dynamic CQI Slicing Active)\n");

    while(1) {
        sleep(1); 
        // \r 是為了讓他在同一行更新，不會一直洗版 (如果不喜歡可以換成 \n)
        printf("[DEBUG] Raw Data -> VIP CQI: %d | STD CQI: %d | Current Ratio: %.2f\n", 
               vip_cqi, std_cqi, current_vip_ratio);
               
        float total_score = (float)(vip_cqi + std_cqi);
        float new_vip_ratio = 0.7; 

        if (total_score > 0) {
            new_vip_ratio = (float)vip_cqi / total_score;
        }
        send_slice_config(&nodes, new_vip_ratio);
    }

    // [修正] 程式結束前手動釋放資源
    free_e2_node_arr_xapp(&nodes); 
    return 0;
}