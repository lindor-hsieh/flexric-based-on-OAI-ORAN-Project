/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance ...
 * (保留原始 License 宣告)
 */

#include "mac_enc_plain.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// 1. Event Trigger 編碼 (用於 RIC Subscription)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 2. Action Definition 編碼 (通常為空)
// ---------------------------------------------------------------------------
byte_array_t mac_enc_action_def_plain(mac_action_def_t const* action_def)
{
  assert(action_def != NULL);
  byte_array_t ba = {0}; // Plain 模式下通常不使用此欄位
  return ba;
}

// ---------------------------------------------------------------------------
// 3. Indication Header 編碼 (gNB -> RIC)
// ---------------------------------------------------------------------------
byte_array_t mac_enc_ind_hdr_plain(mac_ind_hdr_t const* ind_hdr)
{
  assert(ind_hdr != NULL);
  byte_array_t ba = {0};

  ba.len = sizeof(mac_ind_hdr_t);
  ba.buf = calloc(1, ba.len);
  assert(ba.buf != NULL && "Memory exhausted");
  memcpy(ba.buf, ind_hdr, ba.len);

  return ba;
}

// ---------------------------------------------------------------------------
// 4. Indication Message 編碼 (gNB -> RIC) - 傳送 UE 統計與 MCS
// ---------------------------------------------------------------------------
byte_array_t mac_enc_ind_msg_plain(mac_ind_msg_t const* ind_msg)
{
  assert(ind_msg != NULL);

  // [DEBUG] 幫助確認 gNB 正在發送數據，這對於解決 "UE Count 0" 非常重要
  // printf("[OAI-E2] >>> Encoding MAC Indication with %u UEs <<<\n", (unsigned int)ind_msg->len_ue_stats);

  byte_array_t ba = {0};
  
  // 計算總長度：數量欄位(4) + (UE結構大小 * 數量) + 時間戳記(8)
  const uint32_t len = sizeof(ind_msg->len_ue_stats) 
                      + (sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats)
                      + sizeof(ind_msg->tstamp); 
                      
  ba.buf = calloc(1, len); 
  assert(ba.buf != NULL && "Memory exhausted");

  uint8_t* ptr = ba.buf;

  // A. 拷貝 UE 數量
  memcpy(ptr, &ind_msg->len_ue_stats, sizeof(ind_msg->len_ue_stats));
  ptr += sizeof(ind_msg->len_ue_stats);

  // B. 拷貝每個 UE 的數據 (包含 dl_mcs1, rnti 等)
  if (ind_msg->len_ue_stats > 0) {
    size_t const sz_array = sizeof(mac_ue_stats_impl_t) * ind_msg->len_ue_stats;
    memcpy(ptr, ind_msg->ue_stats, sz_array);
    ptr += sz_array;
  }

  // C. 拷貝時間戳記 (最後 8 bytes)
  memcpy(ptr, &ind_msg->tstamp, sizeof(ind_msg->tstamp));
  ptr += sizeof(ind_msg->tstamp);

  assert(ptr == ba.buf + len && "Encoding Mismatch: Indication Message");

  ba.len = len;
  return ba;
}

// ---------------------------------------------------------------------------
// 5. Control Header 編碼 (RIC -> gNB)
// ---------------------------------------------------------------------------
byte_array_t mac_enc_ctrl_hdr_plain(mac_ctrl_hdr_t const* ctrl_hdr)
{
  assert(ctrl_hdr != NULL);
  byte_array_t ba = {0};
  ba.len = sizeof(mac_ctrl_hdr_t);
  ba.buf = calloc(1, ba.len); 
  assert(ba.buf != NULL);
  memcpy(ba.buf, ctrl_hdr, ba.len);
  return ba;
}

// ---------------------------------------------------------------------------
// 6. Control Message 編碼 (RIC -> gNB) - 關鍵：傳送切片配置
// ---------------------------------------------------------------------------
byte_array_t mac_enc_ctrl_msg_plain(mac_ctrl_msg_t const* ctrl_msg)
{
  assert(ctrl_msg != NULL);
  byte_array_t ba = {0};

  // 序列化邏輯：[Type: 1 byte] + [Len: 4 bytes] + [Array: Len * StructSize]
  size_t total_len = sizeof(uint8_t); 

  if (ctrl_msg->type == 0) { // Slice Config Type
      total_len += sizeof(uint32_t); // len_slices
      total_len += ctrl_msg->len_slices * sizeof(mac_slice_params_t);
  }

  ba.len = total_len;
  ba.buf = calloc(1, total_len); 
  assert(ba.buf != NULL && "Memory exhausted");

  uint8_t* ptr = ba.buf;

  // A. 寫入控制類型
  memcpy(ptr, &ctrl_msg->type, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // B. 如果是切片設定，寫入陣列數據
  if (ctrl_msg->type == 0) {
      // 寫入陣列長度
      memcpy(ptr, &ctrl_msg->len_slices, sizeof(uint32_t));
      ptr += sizeof(uint32_t);

      // 寫入整個切片參數陣列 (ID, Percentage)
      if (ctrl_msg->len_slices > 0) {
          size_t const sz_slices = ctrl_msg->len_slices * sizeof(mac_slice_params_t);
          memcpy(ptr, ctrl_msg->slices, sz_slices);
          ptr += sz_slices;
      }
  }

  assert(ptr == ba.buf + total_len && "Encoding Logic Error: Control Message");
  return ba;
}

// ---------------------------------------------------------------------------
// 7. 其他預留函式 (通常保持空實作)
// ---------------------------------------------------------------------------
byte_array_t mac_enc_call_proc_id_plain(mac_call_proc_id_t const* call_proc_id)
{
  assert(call_proc_id != NULL);
  return (byte_array_t){0};
}

byte_array_t mac_enc_ctrl_out_plain(mac_ctrl_out_t const* ctrl) 
{
  assert(ctrl != NULL);
  return (byte_array_t){0};
}

byte_array_t mac_enc_func_def_plain(mac_func_def_t const* func)
{
  assert(func != NULL);
  byte_array_t ba = {0};
  if(func->len > 0) {
      ba.len = func->len;
      ba.buf = calloc(1, ba.len);
      memcpy(ba.buf, func->buf, ba.len);
  }
  return ba;
}