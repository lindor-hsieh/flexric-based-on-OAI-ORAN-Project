/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance ...
 * (保留原始 License 宣告)
 */

#include "mac_dec_plain.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// 1. Event Trigger 解碼 (gNB 端接收)
// ---------------------------------------------------------------------------
mac_event_trigger_t mac_dec_event_trigger_plain(size_t len, uint8_t const ev_tr[len])
{
  mac_event_trigger_t ev = {0};
  if (len >= sizeof(ev.ms)) {
      memcpy(&ev.ms, ev_tr, sizeof(ev.ms));
  }
  return ev;
}

// ---------------------------------------------------------------------------
// 2. Action Definition 解碼
// ---------------------------------------------------------------------------
mac_action_def_t mac_dec_action_def_plain(size_t len, uint8_t const action_def[len])
{
  mac_action_def_t act_def = {0};
  return act_def;
}

// ---------------------------------------------------------------------------
// 3. Indication Header 解碼
// ---------------------------------------------------------------------------
mac_ind_hdr_t mac_dec_ind_hdr_plain(size_t len, uint8_t const ind_hdr[len])
{
  assert(len == sizeof(mac_ind_hdr_t)); 
  mac_ind_hdr_t ret = {0};
  memcpy(&ret, ind_hdr, len);
  return ret;
}

// ---------------------------------------------------------------------------
// 4. Indication Message 解碼 (RIC 端解碼 gNB 傳來的 UE 數據)
// ---------------------------------------------------------------------------
mac_ind_msg_t mac_dec_ind_msg_plain(size_t len, uint8_t const ind_msg[len])
{
  mac_ind_msg_t ret = {0};
  uint8_t const* ptr = ind_msg;

  // A. 讀取 UE 數量 (4 bytes)
  assert(len >= sizeof(ret.len_ue_stats));
  memcpy(&ret.len_ue_stats, ptr, sizeof(ret.len_ue_stats));
  ptr += sizeof(ret.len_ue_stats);

  // B. 分配記憶體並讀取 UE 統計陣列 (包含 dl_mcs1, rnti, wb_cqi 等)
  if(ret.len_ue_stats > 0){
    size_t const sz_array = ret.len_ue_stats * sizeof(mac_ue_stats_impl_t);
    ret.ue_stats = calloc(ret.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(ret.ue_stats != NULL && "Memory exhausted!");
    
    memcpy(ret.ue_stats, ptr, sz_array);
    ptr += sz_array; 
  }

  // C. 讀取時間戳記 (最後 8 bytes)
  assert(ptr + sizeof(ret.tstamp) <= ind_msg + len);
  memcpy(&ret.tstamp, ptr, sizeof(ret.tstamp));
  ptr += sizeof(ret.tstamp);

  // 最終驗證：確保解碼長度與封包總長度精準對齊
  assert(ptr == ind_msg + len && "Indication Message: Data layout mismatch");

  return ret;
}

// ---------------------------------------------------------------------------
// 5. Control Header 解碼
// ---------------------------------------------------------------------------
mac_ctrl_hdr_t mac_dec_ctrl_hdr_plain(size_t len, uint8_t const ctrl_hdr[len])
{
  assert(len == sizeof(mac_ctrl_hdr_t)); 
  mac_ctrl_hdr_t ret = {0};
  memcpy(&ret, ctrl_hdr, len);
  return ret;
}

// ---------------------------------------------------------------------------
// 6. Control Message 解碼 (gNB 端還原 xApp 傳來的切片配置)
// ---------------------------------------------------------------------------
mac_ctrl_msg_t mac_dec_ctrl_msg_plain(size_t len, uint8_t const ctrl_msg[len])
{
  mac_ctrl_msg_t ret = {0};
  uint8_t const* ptr = ctrl_msg;

  // A. 讀取控制類型 (1 byte)
  assert(len >= sizeof(uint8_t));
  memcpy(&ret.type, ptr, sizeof(uint8_t));
  ptr += sizeof(uint8_t);

  // B. 如果是 Slice Config (Type 0)，還原切片陣列
  if (ret.type == 0) {
      // 讀取切片數量 (4 bytes)
      assert(ptr + sizeof(uint32_t) <= ctrl_msg + len);
      memcpy(&ret.len_slices, ptr, sizeof(uint32_t));
      ptr += sizeof(uint32_t);

      // 為切片參數分配空間並拷貝數據
      if (ret.len_slices > 0) {
          size_t const sz_slices = ret.len_slices * sizeof(mac_slice_params_t);
          ret.slices = calloc(ret.len_slices, sizeof(mac_slice_params_t));
          assert(ret.slices != NULL && "Memory exhausted");

          memcpy(ret.slices, ptr, sz_slices);
          ptr += sz_slices;
      }
  }

  // 驗證解碼完整性
  assert(ptr == ctrl_msg + len && "Control Message: Data layout mismatch");

  return ret;
}

// ---------------------------------------------------------------------------
// 7. 其他預留解碼函式
// ---------------------------------------------------------------------------
mac_call_proc_id_t mac_dec_call_proc_id_plain(size_t len, uint8_t const call_proc_id[len])
{
  mac_call_proc_id_t ret = {0};
  return ret;
}

mac_ctrl_out_t mac_dec_ctrl_out_plain(size_t len, uint8_t const ctrl_out[len]) 
{
  mac_ctrl_out_t ret = {0};
  return ret;
}

mac_func_def_t mac_dec_func_def_plain(size_t len, uint8_t const func_def[len])
{
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