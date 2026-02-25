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

#include "mac_dec_plain.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ==========================================
// 1. RIC Event Trigger Definition
// ==========================================

mac_event_trigger_t mac_dec_event_trigger_plain(size_t len, uint8_t const* ev_tr)
{
  assert(ev_tr != NULL);
  assert(len == sizeof(uint32_t)); // ms

  mac_event_trigger_t ev = {0};
  memcpy(&ev.ms, ev_tr, sizeof(ev.ms));
  return ev;
}

// ==========================================
// 2. RIC Action Definition 
// ==========================================

mac_action_def_t mac_dec_action_def_plain(size_t len, uint8_t const* action_def)
{
  assert(action_def != NULL);
  // assert(len == sizeof(uint32_t)); // dummy check removed for flexibility

  mac_action_def_t act_def = {0};
  memcpy(&act_def.dummy, action_def, sizeof(act_def.dummy));
  return act_def;
}

// ==========================================
// 3. RIC Indication Header 
// ==========================================

mac_ind_hdr_t mac_dec_ind_hdr_plain(size_t len, uint8_t const* ind_hdr)
{
  assert(ind_hdr != NULL);
  
  mac_ind_hdr_t ret = {0};
  memcpy(&ret, ind_hdr, sizeof(mac_ind_hdr_t)); // use sizeof type for safety
  return ret;
}

// ==========================================
// 4. RIC Indication Message (感知數據解碼)
// ==========================================

mac_ind_msg_t mac_dec_ind_msg_plain(size_t len, uint8_t const* ind_msg)
{
  assert(ind_msg != NULL);
  mac_ind_msg_t ret = {0};

  uint8_t const* ptr = ind_msg;
  size_t remaining_len = len;

  // 1. 讀取時間戳記 (tstamp) - 與 enc 順序一致
  assert(remaining_len >= sizeof(ret.tstamp));
  memcpy(&ret.tstamp, ptr, sizeof(ret.tstamp));
  ptr += sizeof(ret.tstamp);
  remaining_len -= sizeof(ret.tstamp);

  // 2. 讀取 UE 數量 (len_ue_stats)
  assert(remaining_len >= sizeof(ret.len_ue_stats));
  memcpy(&ret.len_ue_stats, ptr, sizeof(ret.len_ue_stats));
  ptr += sizeof(ret.len_ue_stats);
  remaining_len -= sizeof(ret.len_ue_stats);

  // 3. 讀取 UE 統計資料陣列 (Payload)
  if(ret.len_ue_stats > 0){
    size_t stats_size = sizeof(mac_ue_stats_impl_t) * ret.len_ue_stats;
    assert(remaining_len >= stats_size && "Buffer underflow in mac_dec_ind_msg_plain");

    ret.ue_stats = calloc(ret.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(ret.ue_stats != NULL && "Memory exhausted!");

    // 直接複製陣列區塊 (Flat Copy)
    // 這裡會自動包含你在 .h 中新增的 dl_buffer_info 等欄位
    memcpy(ret.ue_stats, ptr, stats_size);
    ptr += stats_size;
    remaining_len -= stats_size;
  }
  
  assert(remaining_len == 0 && "Data layout mismatch in mac_dec_ind_msg_plain");

  return ret;
}

// ==========================================
// 5. RIC Call Process ID 
// ==========================================

mac_call_proc_id_t mac_dec_call_proc_id_plain(size_t len, uint8_t const* call_proc_id)
{
  assert(call_proc_id != NULL);
  
  mac_call_proc_id_t ret = {0};
  memcpy(&ret, call_proc_id, sizeof(mac_call_proc_id_t));
  return ret;
}

// ==========================================
// 6. RIC Control Header 
// ==========================================

mac_ctrl_hdr_t mac_dec_ctrl_hdr_plain(size_t len, uint8_t const* ctrl_hdr)
{
  assert(ctrl_hdr != NULL);
  
  mac_ctrl_hdr_t ret = {0};
  memcpy(&ret, ctrl_hdr, sizeof(mac_ctrl_hdr_t));
  return ret;
}

// ==========================================
// 7. RIC Control Message (核心解碼：支援 2D 資源控制)
// ==========================================

mac_ctrl_msg_t mac_dec_ctrl_msg_plain(size_t len, uint8_t const* ctrl_msg)
{
  assert(ctrl_msg != NULL);
  mac_ctrl_msg_t ret = {0};
  
  uint8_t const* ptr = ctrl_msg;
  size_t remaining_len = len;

  // 1. 讀取 Control Type
  assert(remaining_len >= sizeof(ret.type));
  memcpy(&ret.type, ptr, sizeof(ret.type));
  ptr += sizeof(ret.type);
  remaining_len -= sizeof(ret.type);

  // 2. 讀取 Slice 數量 (len_slices)
  assert(remaining_len >= sizeof(ret.len_slices));
  memcpy(&ret.len_slices, ptr, sizeof(ret.len_slices));
  ptr += sizeof(ret.len_slices);
  remaining_len -= sizeof(ret.len_slices);

  // 3. 讀取 Slices 參數陣列 (包含 prb_quota, slot_mask)
  if (ret.len_slices > 0) {
      size_t slices_size = ret.len_slices * sizeof(mac_slice_params_t);
      
      // [安全性檢查] 確保剩餘緩衝區大小足夠，防止 Buffer Overflow
      assert(remaining_len >= slices_size && "Buffer underflow in mac_dec_ctrl_msg_plain");

      // 分配記憶體
      ret.slices = calloc(ret.len_slices, sizeof(mac_slice_params_t));
      assert(ret.slices != NULL && "Memory exhausted in mac_dec_ctrl_msg_plain");
      
      // 複製內容
      memcpy(ret.slices, ptr, slices_size);
      ptr += slices_size;
      remaining_len -= slices_size;
  }

  assert(remaining_len == 0 && "Data layout mismatch in mac_dec_ctrl_msg_plain");

  return ret;
}

// ==========================================
// 8. RIC Control Outcome 
// ==========================================

mac_ctrl_out_t mac_dec_ctrl_out_plain(size_t len, uint8_t const* ctrl_out) 
{
  assert(ctrl_out != NULL);

  mac_ctrl_out_t ret = {0};
  memcpy(&ret, ctrl_out, sizeof(mac_ctrl_out_t));
  return ret;
}

// ==========================================
// 9. RAN Function Definition 
// ==========================================

mac_func_def_t mac_dec_func_def_plain(size_t len, uint8_t const* func_def)
{
  assert(func_def != NULL);
  mac_func_def_t ret = {0};

  uint8_t const* ptr = func_def;
  size_t remaining_len = len;
  
  // 讀取長度
  assert(remaining_len >= sizeof(ret.len));
  memcpy(&ret.len, ptr, sizeof(ret.len));
  ptr += sizeof(ret.len);
  remaining_len -= sizeof(ret.len);

  // 讀取內容
  if (ret.len > 0) {
      assert(remaining_len >= ret.len);
      ret.buf = calloc(ret.len, sizeof(uint8_t));
      assert(ret.buf != NULL);
      memcpy(ret.buf, ptr, ret.len);
      ptr += ret.len;
      remaining_len -= ret.len;
  }

  assert(remaining_len == 0 && "Data layout mismatch in mac_dec_func_def_plain");
  
  return ret;
}