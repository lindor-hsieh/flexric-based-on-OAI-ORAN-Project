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

mac_event_trigger_t mac_dec_event_trigger_plain(size_t len, uint8_t const ev_tr[len])
{
  mac_event_trigger_t ev = {0};
  // 簡單檢查長度是否足夠
  if (len >= sizeof(ev.ms)) {
      memcpy(&ev.ms, ev_tr, sizeof(ev.ms));
  }
  return ev;
}

mac_action_def_t mac_dec_action_def_plain(size_t len, uint8_t const action_def[len])
{
  // assert(0!=0 && "Not implemented");
  // 防止崩潰，回傳空結構
  mac_action_def_t act_def = {0};
  return act_def;
}

mac_ind_hdr_t mac_dec_ind_hdr_plain(size_t len, uint8_t const ind_hdr[len])
{
  assert(len == sizeof(mac_ind_hdr_t)); 
  mac_ind_hdr_t ret = {0};
  memcpy(&ret, ind_hdr, len);
  return ret;
}

// [CQI] 自動適配：因為 .h 檔結構變大了，這裡的 sizeof 會自動處理 CQI
mac_ind_msg_t mac_dec_ind_msg_plain(size_t len, uint8_t const ind_msg[len])
{
  mac_ind_msg_t ret = {0};

  // 1. 讀取 UE 數量
  const size_t len_ue_count = sizeof(ret.len_ue_stats);
  assert(len >= len_ue_count);
  memcpy(&ret.len_ue_stats, ind_msg, len_ue_count);

  // 2. 分配記憶體
  if(ret.len_ue_stats > 0){
    ret.ue_stats = calloc(ret.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(ret.ue_stats != NULL && "Memory exhausted!");
  }
  
  // 3. 讀取每個 UE 的數據 (包含 CQI)
  void* ptr = (void*)&ind_msg[len_ue_count];
  
  for(uint32_t i = 0; i < ret.len_ue_stats; ++i){
    // 這裡的 sizeof(mac_ue_stats_impl_t) 已經包含了 wb_cqi
    memcpy(&ret.ue_stats[i], ptr, sizeof(mac_ue_stats_impl_t));
    ptr += sizeof(mac_ue_stats_impl_t); 
  }

  // 4. 讀取時間戳記
  memcpy(&ret.tstamp, ptr, sizeof(ret.tstamp));
  ptr += sizeof(ret.tstamp);

  // 驗證讀取的總長度是否與輸入長度一致
  assert(ptr == (void*)ind_msg + len && "data layout mismatch");

  return ret;
}

mac_call_proc_id_t mac_dec_call_proc_id_plain(size_t len, uint8_t const call_proc_id[len])
{
  // assert(0!=0 && "Not implemented");
  mac_call_proc_id_t ret = {0};
  return ret;
}

mac_ctrl_hdr_t mac_dec_ctrl_hdr_plain(size_t len, uint8_t const ctrl_hdr[len])
{
  assert(len == sizeof(mac_ctrl_hdr_t)); 
  mac_ctrl_hdr_t ret = {0};
  memcpy(&ret, ctrl_hdr, len);
  return ret;
}

// [關鍵修改] 支援切片陣列的解碼
mac_ctrl_msg_t mac_dec_ctrl_msg_plain(size_t len, uint8_t const ctrl_msg[len])
{
  mac_ctrl_msg_t ret = {0};
  void* ptr = (void*)ctrl_msg;

  // 1. 讀取 Type (uint8_t)
  memcpy(&ret.type, ptr, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // 2. 根據 Type 處理 payload
  if (ret.type == 0) { // Slice Config
      // 讀取陣列長度 (uint32_t)
      memcpy(&ret.len_slices, ptr, sizeof(uint32_t));
      ptr += sizeof(uint32_t);

      // 分配 slice 陣列記憶體
      if (ret.len_slices > 0) {
          ret.slices = calloc(ret.len_slices, sizeof(mac_slice_params_t));
          assert(ret.slices != NULL && "Memory exhausted");

          // 逐一讀取切片參數
          for (uint32_t i = 0; i < ret.len_slices; i++) {
              memcpy(&ret.slices[i], ptr, sizeof(mac_slice_params_t));
              ptr += sizeof(mac_slice_params_t);
          }
      }
  }

  // 檢查是否剛好讀完 (Optional)
  // assert(ptr == (void*)ctrl_msg + len);

  return ret;
}

mac_ctrl_out_t mac_dec_ctrl_out_plain(size_t len, uint8_t const ctrl_out[len]) 
{
  // assert(0!=0 && "Not implemented");
  mac_ctrl_out_t ret = {0};
  return ret;
}

mac_func_def_t mac_dec_func_def_plain(size_t len, uint8_t const func_def[len])
{
  // 簡單實作：分配並複製
  mac_func_def_t ret = {0};
  if (len > 0) {
      ret.len = len;
      ret.buf = calloc(1, len);
      if (ret.buf) {
          memcpy(ret.buf, func_def, len);
      }
  }
  return ret;
}