/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "../../../../src/xApp/e42_xapp_api.h"
#include "../../../../src/util/alg_ds/alg/defer.h"
#include "../../../../src/sm/mac_sm/mac_sm_id.h" // 必須引入 ID 定義

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

// 
#define PRB_LOW  5     // 低頻寬模式
#define PRB_HIGH 48    // 高頻寬模式
#define INTERVAL 10     // 每 10 秒切換一次

// 封裝發送函式
void send_prb_limit(e2_node_arr_xapp_t* nodes, uint32_t limit) {
    if (nodes->len == 0) return;

    // 1. 準備 Control Request 資料
    mac_ctrl_req_data_t req = {0};
    req.hdr.dummy = 0;
    
    // 設定我們定義好的參數
    // 我們把 Limit 左移 16 位，然後跟 Action 1 組合起來
    // 例如 Limit=5 -> 0x00050001 (十進位 327681)
    req.msg.action = (limit << 16) | 1;  
    req.msg.rnti = 0;
    // req.msg.prb_limit = limit; // ★ 填入限制值

    // 2. 傳送給每一個連線的 gNB
    for (int i = 0; i < nodes->len; i++) {
        e2_node_connected_xapp_t* n = &nodes->n[i];
        
        // 只傳送給 gNB 或 gNB-DU
        if(n->id.type == ngran_gNB || n->id.type == ngran_gNB_DU) {
            
            // SM_MAC_ID 是 142，定義在 mac_sm_id.h
            sm_ans_xapp_t const ans = control_sm_xapp_api(&n->id, SM_MAC_ID, &req);
            
            if(ans.success) {
                printf("[xApp] Node %d -> Set Limit: %d PRBs (Success)\n", i, limit);
            } else {
                printf("[xApp] Node %d -> Failed to send limit!\n", i);
            }
        }
    }
}

int main(int argc, char *argv[])
{
  fr_args_t args = init_fr_args(argc, argv);

  // 1. 初始化 xApp
  init_xapp_api(&args);
  
  printf("[xApp] Waiting for E2 connection...\n");
  sleep(1); // 等待連線建立

  // 2. 取得連線節點
  e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
  defer({ free_e2_node_arr_xapp(&nodes); });

  if (nodes.len > 0) {
      printf("[xApp] Connected to %d E2 node(s). Starting Heartbeat Test...\n", nodes.len);
      printf("==============================================================\n");

      // 3. 進入無限迴圈 (Heartbeat Loop)
      while(1) {
          // --- Phase A: Low Bandwidth ---
          printf("\n>>> [State] LOW BANDWIDTH (%d PRB) <<<\n", PRB_LOW);
          send_prb_limit(&nodes, PRB_LOW);
          sleep(INTERVAL);

          // --- Phase B: High Bandwidth ---
          printf("\n>>> [State] HIGH BANDWIDTH (%d PRB) <<<\n", PRB_HIGH);
          send_prb_limit(&nodes, PRB_HIGH);
          sleep(INTERVAL);
      }

  } else {
      printf("[Error] No E2 Node connected. Please check OAI gNB status.\n");
  }

  while(try_stop_xapp_api() == false)
    usleep(1000);

  return 0;
}