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

#include "mac_enc_plain.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ==========================================
// 1. RIC Event Trigger Definition
// ==========================================

byte_array_t mac_enc_event_trigger_plain(mac_event_trigger_t const* event_trigger)
{
  assert(event_trigger != NULL);
  byte_array_t ba = {0};
 
  ba.len = sizeof(event_trigger->ms);
  ba.buf = malloc(ba.len);
  assert(ba.buf != NULL && "Memory exhausted");

  memcpy(ba.buf, &event_trigger->ms, ba.len);

  return ba;
}

// ==========================================
// 2. RIC Action Definition 
// ==========================================

byte_array_t mac_enc_action_def_plain(mac_action_def_t const* action_def)
{
  assert(action_def != NULL);
  byte_array_t ba = {0};
  
  ba.len = sizeof(action_def->dummy);
  ba.buf = malloc(ba.len);
  assert(ba.buf != NULL);
  
  memcpy(ba.buf, &action_def->dummy, ba.len);
  
  return ba;
}

// ==========================================
// 3. RIC Indication Header 
// ==========================================

byte_array_t mac_enc_ind_hdr_plain(mac_ind_hdr_t const* ind_hdr)
{
  assert(ind_hdr != NULL);

  byte_array_t ba = {0};

  ba.len = sizeof(mac_ind_hdr_t);
  ba.buf = calloc(ba.len, sizeof(uint8_t));
  assert(ba.buf != NULL && "memory exhausted");
  memcpy(ba.buf, ind_hdr, ba.len);

  return ba;
}

// ==========================================
// 4. RIC Indication Message (感知層序列化)
// ==========================================

byte_array_t mac_enc_ind_msg_plain(mac_ind_msg_t const* ind_msg)
{
  assert(ind_msg != NULL);

  byte_array_t ba = {0};

  // 計算總長度：時間戳 + UE數量 + UE統計數據陣列
  // sizeof(mac_ue_stats_impl_t) 已經包含了你在 .h 中新增的 dl_buffer_info 等欄位
  const uint32_t len = sizeof(ind_msg->tstamp) + 
                       sizeof(ind_msg->len_ue_stats) + 
                       (sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats);
                      
  ba.buf = calloc(1, len); 
  assert(ba.buf != NULL);

  uint8_t* ptr = ba.buf;

  // 1. 寫入時間戳 (tstamp)
  memcpy(ptr, &ind_msg->tstamp, sizeof(ind_msg->tstamp));
  ptr += sizeof(ind_msg->tstamp);

  // 2. 寫入 UE 數量 (len_ue_stats)
  memcpy(ptr, &ind_msg->len_ue_stats, sizeof(ind_msg->len_ue_stats));
  ptr += sizeof(ind_msg->len_ue_stats);

  // 3. 寫入 UE 統計數據陣列 (Payload)
  if (ind_msg->len_ue_stats > 0 && ind_msg->ue_stats != NULL) {
      size_t stats_size = sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats;
      memcpy(ptr, ind_msg->ue_stats, stats_size);
      ptr += stats_size;
  }

  assert(ptr == ba.buf + len && "Data layout mismatch in mac_enc_ind_msg_plain");

  ba.len = len;
  return ba;
}

// ==========================================
// 5. RIC Call Process ID 
// ==========================================

byte_array_t mac_enc_call_proc_id_plain(mac_call_proc_id_t const* call_proc_id)
{
  assert(call_proc_id != NULL);
  byte_array_t ba = {0};
  
  ba.len = sizeof(mac_call_proc_id_t);
  ba.buf = malloc(ba.len);
  assert(ba.buf != NULL);
  
  memcpy(ba.buf, call_proc_id, ba.len);
  return ba;
}

// ==========================================
// 6. RIC Control Header 
// ==========================================

byte_array_t mac_enc_ctrl_hdr_plain(mac_ctrl_hdr_t const* ctrl_hdr)
{
  assert(ctrl_hdr != NULL);
  byte_array_t ba = {0};
  ba.len = sizeof(mac_ctrl_hdr_t);
  ba.buf = calloc(ba.len ,sizeof(uint8_t)); 
  assert(ba.buf != NULL);

  memcpy(ba.buf, ctrl_hdr, ba.len);

  return ba;
}

// ==========================================
// 7. RIC Control Message (控制層序列化 - 論文核心)
// ==========================================

byte_array_t mac_enc_ctrl_msg_plain(mac_ctrl_msg_t const* ctrl_msg)
{
  assert(ctrl_msg != NULL);
  byte_array_t ba = {0};

  // 1. 計算所需總長度
  // 基本欄位：Type (uint8) + Slice數量 (uint32)
  uint32_t len = sizeof(ctrl_msg->type) + sizeof(ctrl_msg->len_slices);
  
  // 動態部分：Slices 參數陣列 (包含 prb_quota, slot_mask)
  if (ctrl_msg->len_slices > 0) {
      len += ctrl_msg->len_slices * sizeof(mac_slice_params_t);
  }

  ba.len = len;
  ba.buf = calloc(1, len);
  assert(ba.buf != NULL && "Memory exhausted in mac_enc_ctrl_msg_plain");

  uint8_t* ptr = ba.buf;

  // 2. 序列化 Control Type
  memcpy(ptr, &ctrl_msg->type, sizeof(ctrl_msg->type));
  ptr += sizeof(ctrl_msg->type);

  // 3. 序列化 Slice 數量
  memcpy(ptr, &ctrl_msg->len_slices, sizeof(ctrl_msg->len_slices));
  ptr += sizeof(ctrl_msg->len_slices);

  // 4. 序列化 Slices 參數內容
  // 這確保了 slot_mask 等 2D 控制參數被正確打包
  if (ctrl_msg->len_slices > 0 && ctrl_msg->slices != NULL) {
      size_t slices_size = ctrl_msg->len_slices * sizeof(mac_slice_params_t);
      memcpy(ptr, ctrl_msg->slices, slices_size);
      ptr += slices_size;
  }

  assert(ptr == ba.buf + len && "Data layout mismatch in mac_enc_ctrl_msg_plain");

  return ba;
}

// ==========================================
// 8. RIC Control Outcome 
// ==========================================

byte_array_t mac_enc_ctrl_out_plain(mac_ctrl_out_t const* ctrl) 
{
  assert(ctrl != NULL);
  byte_array_t ba = {0};
  
  ba.len = sizeof(mac_ctrl_out_t);
  ba.buf = malloc(ba.len);
  assert(ba.buf != NULL);
  
  memcpy(ba.buf, ctrl, ba.len);
  
  return ba;
}

// ==========================================
// 9. RAN Function Definition 
// ==========================================

byte_array_t mac_enc_func_def_plain(mac_func_def_t const* func)
{
  assert(func != NULL);
  byte_array_t ba = {0};
  
  // 序列化：長度 + 內容
  uint32_t total_len = sizeof(func->len) + func->len;
  
  ba.len = total_len;
  ba.buf = calloc(1, total_len);
  assert(ba.buf != NULL);
  
  uint8_t* ptr = ba.buf;
  memcpy(ptr, &func->len, sizeof(func->len));
  ptr += sizeof(func->len);
  
  if (func->len > 0 && func->buf != NULL) {
      memcpy(ptr, func->buf, func->len);
  }
  
  return ba;
}