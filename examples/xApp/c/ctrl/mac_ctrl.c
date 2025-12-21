#include "xApp/e42_xapp_api.h"
#include "util/alg_ds/alg/defer.h"
#include "sm/mac_sm/ie/mac_data_ie.h"
#include "sm/mac_sm/mac_sm_id.h" 

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef SM_MAC_ID
#define SM_MAC_ID 142
#endif

// ==========================================
// 發送切片定義
// ==========================================
void send_slice_config(e2_node_arr_xapp_t* nodes) {
    if (nodes->len == 0) {
        printf("[xApp] No E2 nodes connected yet...\n");
        return;
    }

    // 1. 準備 MAC Payload
    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy = 0;
    req.msg.type = 0; // Type 0: Slice Config
    req.msg.len_slices = 2;

    req.msg.slices = calloc(2, sizeof(mac_slice_params_t));
    if(req.msg.slices == NULL) {
        printf("[xApp] Memory allocation failed\n");
        return;
    }

    // --- Slice 1: VIP (70%) ---
    req.msg.slices[0].id = 1;
    req.msg.slices[0].percentage = 0.7;

    // --- Slice 2: Standard (30%) ---
    req.msg.slices[1].id = 2;
    req.msg.slices[1].percentage = 0.3;

    // 2. 包裝成通用寫入介面
    sm_ag_if_wr_t wr = {0};
    wr.type = CONTROL_SM_AG_IF_WR;      
    wr.ctrl.type = MAC_CTRL_REQ_V0;     
    wr.ctrl.mac_ctrl = req;             

    // 3. 發送給所有連線的 E2 Node
    for (int i = 0; i < nodes->len; i++) {
        printf("[xApp] Sending Slice Config to Node ID: Global ID\n");
        
        // 直接發送控制指令，不需事先訂閱
        sm_ans_xapp_t ans = control_sm_xapp_api(&nodes->n[i].id, SM_MAC_ID, &wr);
        
        if (ans.success) {
            printf("[xApp] -> Slice Config Sent Successfully: VIP(70%%), STD(30%%)\n");
        } else {
            printf("[xApp] -> Control Message Failed! (Check gNB logs)\n");
        }
    }

    free(req.msg.slices);
}

int main(int argc, char *argv[]) {
    // 1. 初始化
    fr_args_t args = init_fr_args(argc, argv);
    init_xapp_api(&args); 

    printf("[xApp] Starting Dynamic Slicing xApp (Control Only)...\n");

    // 2. 等待並獲取連線的 E2 Nodes
    e2_node_arr_xapp_t nodes = {0};
    while (nodes.len == 0) {
        printf("[xApp] Waiting for gNB connection...\n");
        sleep(1);
        nodes = e2_nodes_xapp_api(); 
    }

    printf("[xApp] gNB Connected! Node count: %d\n", nodes.len);

    // 3. [修正] 跳過訂閱步驟，直接發送控制
    // 這樣可以避開 report_sm_xapp_api 因參數 NULL 而導致的 Segfault
    
    printf("[xApp] Sending control message immediately...\n");
    sleep(1); // 稍微等待連線穩定
    send_slice_config(&nodes);

    // 4. 任務完成，保持運作或退出
    printf("[xApp] Experiment Active. Press Ctrl+C to stop.\n");
    while (1) {
        sleep(10); 
    }

    return 0;
}