/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 * contact@openairinterface.org
 */

#ifndef MAC_DATA_INFORMATION_ELEMENTS_H
#define MAC_DATA_INFORMATION_ELEMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//////////////////////////////////////
// 1. RIC Event Trigger Definition
/////////////////////////////////////

typedef struct {
  uint32_t ms; 
} mac_event_trigger_t;

void free_mac_event_trigger(mac_event_trigger_t* src); 
mac_event_trigger_t cp_mac_event_trigger(mac_event_trigger_t const* src);
bool eq_mac_event_trigger(mac_event_trigger_t const* m0, mac_event_trigger_t const* m1);

//////////////////////////////////////
// 2. RIC Action Definition 
/////////////////////////////////////

typedef struct {
  uint32_t dummy;  
} mac_action_def_t;

void free_mac_action_def(mac_action_def_t* src); 
mac_action_def_t cp_mac_action_def(mac_action_def_t const* src);
bool eq_mac_action_def(mac_action_def_t const* m0, mac_action_def_t const* m1);

//////////////////////////////////////
// 3. RIC Indication Header 
/////////////////////////////////////

typedef struct {
  uint32_t dummy;  
} mac_ind_hdr_t;

void free_mac_ind_hdr(mac_ind_hdr_t* src); 
mac_ind_hdr_t cp_mac_ind_hdr(mac_ind_hdr_t const* src);
bool eq_mac_ind_hdr(mac_ind_hdr_t const* m0, mac_ind_hdr_t const* m1);

//////////////////////////////////////
// 4. RIC Indication Message (包含感測數據擴展)
/////////////////////////////////////

// [論文關鍵擴充]：即時 Performance Feedback 數據
typedef struct {
  // --- 論文需要的感知數據 ---
  // 這些欄位讓 Local xApp 能感知 Buffer 堆積與延遲，進行預測決策
  uint32_t dl_buffer_info;   // 即時下行緩衝區狀態 (bytes) - 判斷擁塞的核心指標
  uint32_t ul_buffer_info;   // 即時上行緩衝區狀態 (bytes)
  float    rlc_delay_ms;     // RLC 層觀測到的即時延遲 (ms)
  
  // --- 標準 OAI 數據欄位 ---
  uint64_t dl_aggr_tbs;
  uint64_t ul_aggr_tbs;
  uint64_t dl_aggr_bytes_sdus;
  uint64_t ul_aggr_bytes_sdus;
  uint64_t dl_curr_tbs;
  uint64_t ul_curr_tbs;
  uint64_t dl_sched_rb;
  uint64_t ul_sched_rb;
  float pusch_snr; 
  float pucch_snr; 
  float dl_bler;
  float ul_bler;
  uint32_t dl_harq[5];
  uint32_t ul_harq[5];
  uint32_t dl_num_harq;
  uint32_t ul_num_harq;
  uint32_t rnti;            // 用戶識別碼
  uint32_t dl_aggr_prb; 
  uint32_t ul_aggr_prb;
  uint32_t dl_aggr_sdus;
  uint32_t ul_aggr_sdus;
  uint32_t dl_aggr_retx_prb;
  uint32_t ul_aggr_retx_prb;
  uint32_t bsr;
  uint16_t frame;
  uint16_t slot;
  uint8_t wb_cqi; 
  uint8_t dl_mcs1;
  uint8_t ul_mcs1;
  uint8_t dl_mcs2; 
  uint8_t ul_mcs2;
  int8_t phr;
  
} mac_ue_stats_impl_t;

typedef struct {
  uint32_t len_ue_stats;
  mac_ue_stats_impl_t* ue_stats;
  int64_t tstamp;
} mac_ind_msg_t;

void free_mac_ind_msg(mac_ind_msg_t* src); 
mac_ind_msg_t cp_mac_ind_msg(mac_ind_msg_t const* src);
bool eq_mac_ind_msg(mac_ind_msg_t const* m0, mac_ind_msg_t const* m1);

//////////////////////////////////////
// 5. RIC Control Message (包含 2D 資源控制擴展)
/////////////////////////////////////

// [論文關鍵擴充]：定義切片/UE 的控制參數
// 這是 xApp 下達決策的載體，支援頻域與時域控制
typedef struct {
  uint32_t id;             // Slice ID 或 UE RNTI (例如 0x3076)
  float    prb_quota;      // [頻域] AI 決策的 PRB 分配配額 (0.0 ~ 1.0)
  uint16_t slot_mask;      // [時域] Slot 分配遮罩 (0xFFFF=全開, 0xAAAA=偶數...)
  uint8_t  priority;       // [優先級] 可用於搶佔式調度
} mac_slice_params_t;

typedef struct {
  uint8_t type;            // Control Type (0: Resource Allocation, 1: Other...)
  uint32_t len_slices;     // 控制的對象數量
  mac_slice_params_t* slices; // 動態陣列，存放具體參數
} mac_ctrl_msg_t;

void free_mac_ctrl_msg(mac_ctrl_msg_t* src); 
mac_ctrl_msg_t cp_mac_ctrl_msg(mac_ctrl_msg_t const* src);
bool eq_mac_ctrl_msg(mac_ctrl_msg_t const* m0, mac_ctrl_msg_t const* m1);

//////////////////////////////////////
// 6. RAN Function Definition 
/////////////////////////////////////

typedef struct {
  size_t len;
  uint8_t* buf;
} mac_func_def_t;

void free_mac_func_def(mac_func_def_t* src); 
mac_func_def_t cp_mac_func_def(mac_func_def_t const* src);
bool eq_mac_func_def(mac_func_def_t const* m0, mac_func_def_t const* m1);

//////////////////////////////////////
// 7. 其他 E2 過程相關 (Boilerplate)
/////////////////////////////////////

typedef struct { uint32_t dummy; } mac_call_proc_id_t;
void free_mac_call_proc_id(mac_call_proc_id_t* src);
mac_call_proc_id_t cp_mac_call_proc_id(mac_call_proc_id_t const* src);
bool eq_mac_call_proc_id(mac_call_proc_id_t const* m0, mac_call_proc_id_t const* m1);

typedef struct { uint32_t dummy; } mac_ctrl_hdr_t;
void free_mac_ctrl_hdr(mac_ctrl_hdr_t* src);
mac_ctrl_hdr_t cp_mac_ctrl_hdr(mac_ctrl_hdr_t const* src);
bool eq_mac_ctrl_hdr(mac_ctrl_hdr_t const* m0, mac_ctrl_hdr_t const* m1);

typedef enum { MAC_CTRL_OUT_OK, MAC_CTRL_OUT_FAIL } mac_ctrl_out_e;
typedef struct { mac_ctrl_out_e ans; } mac_ctrl_out_t;
void free_mac_ctrl_out(mac_ctrl_out_t* src);
mac_ctrl_out_t cp_mac_ctrl_out(mac_ctrl_out_t const* src);
bool eq_mac_ctrl_out(mac_ctrl_out_t const* m0, mac_ctrl_out_t const* m1);

// RIC Service Model Wrappers
typedef struct {
  mac_event_trigger_t et; 
  mac_action_def_t ad;
} mac_sub_data_t;

typedef struct {
  mac_ind_hdr_t hdr;
  mac_ind_msg_t msg;
  mac_call_proc_id_t* proc_id;
} mac_ind_data_t;

typedef struct {
  mac_ctrl_hdr_t hdr;
  mac_ctrl_msg_t msg;
} mac_ctrl_req_data_t;

typedef struct { 
    mac_ctrl_hdr_t hdr;
    mac_ctrl_out_t out; 
} mac_ctrl_out_data_t;

typedef struct { mac_func_def_t func_def; } mac_e2_setup_data_t;
typedef struct { mac_func_def_t func_def; } mac_ric_service_update_t;

// 必要的 Data Wrapper 函式宣告
void free_mac_ind_data(mac_ind_data_t* ind);
mac_ind_data_t cp_mac_ind_data(mac_ind_data_t const* src);

void free_mac_ctrl_req_data(mac_ctrl_req_data_t* src);
mac_ctrl_req_data_t cp_mac_ctrl_req_data(mac_ctrl_req_data_t const* src);
bool eq_mac_ctrl_req_data(mac_ctrl_req_data_t const* m0, mac_ctrl_req_data_t const* m1);

void free_mac_ctrl_out_data(mac_ctrl_out_data_t* src);
mac_ctrl_out_data_t cp_mac_ctrl_out_data(mac_ctrl_out_data_t const* src);
bool eq_mac_ctrl_out_data(mac_ctrl_out_data_t const* m0, mac_ctrl_out_data_t const* m1);

#ifdef __cplusplus
}
#endif

#endif