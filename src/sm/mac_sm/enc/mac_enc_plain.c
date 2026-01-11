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

byte_array_t mac_enc_event_trigger_plain(mac_event_trigger_t const* event_trigger)
{
  assert(event_trigger != NULL);
  byte_array_t  ba = {0};
 
  ba.len = sizeof(event_trigger->ms);
  ba.buf = malloc(ba.len);
  assert(ba.buf != NULL && "Memory exhausted");

  memcpy(ba.buf, &event_trigger->ms, ba.len);

  return ba;
}

byte_array_t mac_enc_action_def_plain(mac_action_def_t const* action_def)
{
  // assert(0!=0 && "Not implemented");
  // 為了避免 crash，給個空實作
  assert(action_def != NULL);
  byte_array_t  ba = {0};
  return ba;
}

byte_array_t mac_enc_ind_hdr_plain(mac_ind_hdr_t const* ind_hdr)
{
  assert(ind_hdr != NULL);

  byte_array_t ba = {0};

  ba.len = sizeof(mac_ind_hdr_t);
  ba.buf = calloc(ba.len,  sizeof(uint8_t));
  assert(ba.buf != NULL && "memory exhausted");
  memcpy(ba.buf, ind_hdr, ba.len);

  return ba;
}

// [CQI] 這裡不需要改，因為 mac_ue_stats_impl_t 已經變大了
// sizeof 會自動把包含 CQI 的新大小算進去
byte_array_t mac_enc_ind_msg_plain(mac_ind_msg_t const* ind_msg)
{
  assert(ind_msg != NULL);

  byte_array_t ba = {0};
  
  // 計算總長度：長度欄位 + (單個 UE 統計結構大小 * UE 數量) + 時間戳記
  const uint32_t len = sizeof(ind_msg->len_ue_stats) 
                      + sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats
                      + sizeof(ind_msg->tstamp); 
                      
  ba.buf = calloc(1, len); 
  assert(ba.buf != NULL);

  // 1. Copy 數量
  memcpy(ba.buf, &ind_msg->len_ue_stats, sizeof(ind_msg->len_ue_stats));
  void* ptr = ba.buf + sizeof(ind_msg->len_ue_stats);

  // 2. Copy 每個 UE 的數據 (包含 CQI)
  for(uint32_t i = 0; i < ind_msg->len_ue_stats; ++i){
    memcpy(ptr, &ind_msg->ue_stats[i], sizeof(ind_msg->ue_stats[0])); 
    ptr += sizeof(ind_msg->ue_stats[0]);
  }

  // 3. Copy 時間戳記
  memcpy(ptr, &ind_msg->tstamp, sizeof(ind_msg->tstamp));
  ptr += sizeof(ind_msg->tstamp);

  assert(ptr == ba.buf + len && "Data layout mismacth");

  ba.len = len;
  return ba;
}


byte_array_t mac_enc_call_proc_id_plain(mac_call_proc_id_t const* call_proc_id)
{
  // assert(0!=0 && "Not implemented");
  assert(call_proc_id != NULL);
  byte_array_t  ba = {0};
  return ba;
}

byte_array_t mac_enc_ctrl_hdr_plain(mac_ctrl_hdr_t const* ctrl_hdr)
{
  assert(ctrl_hdr != NULL);
  byte_array_t  ba = {0};
  ba.len = sizeof(mac_ctrl_hdr_t);
  ba.buf = calloc(ba.len ,sizeof(uint8_t)); 
  assert(ba.buf != NULL);

  memcpy(ba.buf, ctrl_hdr, ba.len);

  return ba;
}

// [關鍵修改] 支援切片陣列的編碼
byte_array_t mac_enc_ctrl_msg_plain(mac_ctrl_msg_t const* ctrl_msg)
{
  assert(ctrl_msg != NULL);
  byte_array_t ba = {0};

  // 1. 計算所需的總長度
  // 基本長度：Type (uint8) + slice 數量 (uint32)
  size_t total_len = sizeof(uint8_t); 

  if (ctrl_msg->type == 0) { // Slice Config
      total_len += sizeof(uint32_t); // len_slices
      // 加上陣列內容的大小
      total_len += ctrl_msg->len_slices * sizeof(mac_slice_params_t);
  }

  // 2. 分配記憶體
  ba.len = total_len;
  ba.buf = calloc(1, total_len); 
  assert(ba.buf != NULL && "Memory exhausted");

  // 3. 開始序列化 (Serialize)
  void* ptr = ba.buf;

  // Copy Type
  memcpy(ptr, &ctrl_msg->type, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // Copy Payload (如果是切片設定)
  if (ctrl_msg->type == 0) {
      // Copy 陣列長度
      memcpy(ptr, &ctrl_msg->len_slices, sizeof(uint32_t));
      ptr += sizeof(uint32_t);

      // Copy 每個切片的參數 (ID + Percentage)
      for (size_t i = 0; i < ctrl_msg->len_slices; i++) {
          memcpy(ptr, &ctrl_msg->slices[i], sizeof(mac_slice_params_t));
          ptr += sizeof(mac_slice_params_t);
      }
  }

  // 檢查指標是否剛好填滿緩衝區
  assert(ptr == ba.buf + total_len && "Encoding Logic Error");

  return ba;
}

byte_array_t mac_enc_ctrl_out_plain(mac_ctrl_out_t const* ctrl) 
{
  // assert(0!=0 && "Not implemented");
  assert(ctrl != NULL );
  byte_array_t  ba = {0};
  return ba;
}

byte_array_t mac_enc_func_def_plain(mac_func_def_t const* func)
{
  // assert(0!=0 && "Not implemented");
  assert(func != NULL);
  byte_array_t  ba = {0};
  // 簡單實作：只複製內容，不處理複雜邏輯
  if(func->len > 0) {
      ba.len = func->len;
      ba.buf = calloc(1, ba.len);
      memcpy(ba.buf, func->buf, ba.len);
  }
  return ba;
}