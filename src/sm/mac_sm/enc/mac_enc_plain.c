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



#include "mac_enc_plain.h"

#include <assert.h>
#include <stdlib.h>


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
  assert(0!=0 && "Not implemented");

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

byte_array_t mac_enc_ind_msg_plain(mac_ind_msg_t const* ind_msg)
{
  assert(ind_msg != NULL);

  byte_array_t ba = {0};
  const uint32_t len = sizeof(ind_msg->len_ue_stats) 
                      + sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats
                      + sizeof(ind_msg->tstamp); 
  ba.buf = calloc(1, len); 
  assert(ba.buf != NULL);

  memcpy(ba.buf, &ind_msg->len_ue_stats, sizeof(ind_msg->len_ue_stats));
  void* ptr = ba.buf + sizeof(ind_msg->len_ue_stats);

  for(uint32_t i = 0; i < ind_msg->len_ue_stats; ++i){
    memcpy(ptr, &ind_msg->ue_stats[i], sizeof(ind_msg->ue_stats[0])); 
    ptr += sizeof(ind_msg->ue_stats[0]);
  }

  memcpy(ptr, &ind_msg->tstamp, sizeof(ind_msg->tstamp));
  ptr += sizeof(ind_msg->tstamp);

  assert(ptr == ba.buf + len && "Data layout mismacth");

  ba.len = len;
  return ba;
}


byte_array_t mac_enc_call_proc_id_plain(mac_call_proc_id_t const* call_proc_id)
{
  assert(0!=0 && "Not implemented");

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

byte_array_t mac_enc_ctrl_msg_plain(mac_ctrl_msg_t const* ctrl_msg)
{
  assert(ctrl_msg != NULL);
  byte_array_t ba = {0};

  // 序列化邏輯 (Serialization)
  // 1. 計算總長度: Type(1) + Len(1) + SliceData(...)
  size_t slice_struct_size = sizeof(mac_slice_params_t);
  size_t total_len = sizeof(uint8_t) + sizeof(uint8_t) + 
                     (ctrl_msg->len_slices * slice_struct_size);

  ba.len = total_len;
  ba.buf = calloc(1, total_len);
  assert(ba.buf != NULL && "Memory exhausted");

  uint8_t* ptr = ba.buf;

  // 2. 寫入 Type (uint8_t)
  memcpy(ptr, &ctrl_msg->type, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // 3. 寫入 Slice 數量 (uint8_t)
  memcpy(ptr, &ctrl_msg->len_slices, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // 4. 寫入切片陣列資料 (Payload)
  if (ctrl_msg->len_slices > 0 && ctrl_msg->slices != NULL) {
      memcpy(ptr, ctrl_msg->slices, ctrl_msg->len_slices * slice_struct_size);
  }

  return ba;
}

byte_array_t mac_enc_ctrl_out_plain(mac_ctrl_out_t const* ctrl) 
{
  assert(ctrl != NULL);
  byte_array_t ba = {0};

  // 移除 assert，直接複製內容
  ba.len = sizeof(mac_ctrl_out_t);
  ba.buf = calloc(1, ba.len);
  assert(ba.buf != NULL && "Memory exhausted");

  memcpy(ba.buf, ctrl, ba.len);

  return ba;
}

byte_array_t mac_enc_func_def_plain(mac_func_def_t const* func)
{
  // 這個目前用不到，保持 Not implemented 沒關係，但函式必須存在
  assert(0!=0 && "Not implemented");

  assert(func != NULL);
  byte_array_t ba = {0};
  return ba;
}