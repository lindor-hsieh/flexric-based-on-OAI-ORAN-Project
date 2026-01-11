/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance ...
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
mac_action_def_t cp_mac_action_def(mac_action_def_t* src);
bool eq_mac_action_def(mac_action_def_t const* m0, mac_action_def_t const* m1);

//////////////////////////////////////
// 3. RIC Indication Header 
/////////////////////////////////////

typedef struct {
  uint32_t dummy;  
} mac_ind_hdr_t;

void free_mac_ind_hdr(mac_ind_hdr_t* src); 
mac_ind_hdr_t cp_mac_ind_hdr(mac_ind_hdr_t const* src);
bool eq_mac_ind_hdr(mac_ind_hdr_t* m0, mac_ind_hdr_t* m1);

//////////////////////////////////////
// 4. RIC Indication Message 
/////////////////////////////////////

typedef struct {
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
  uint32_t rnti;
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
  uint8_t padding[6];
} mac_ue_stats_impl_t;

typedef struct {
  uint32_t len_ue_stats;
  mac_ue_stats_impl_t* ue_stats;
  int64_t tstamp;
} mac_ind_msg_t;

void free_mac_ind_msg(mac_ind_msg_t* src); 
mac_ind_msg_t cp_mac_ind_msg(mac_ind_msg_t const* src);
bool eq_mac_ind_msg(mac_ind_msg_t* m0, mac_ind_msg_t* m1);

//////////////////////////////////////
// 5. RIC Control Message 
/////////////////////////////////////

typedef struct {
  uint32_t id;       
  float percentage;  
} mac_slice_params_t;

typedef struct {
  uint8_t type;      
  uint32_t len_slices;
  mac_slice_params_t* slices; 
} mac_ctrl_msg_t;

void free_mac_ctrl_msg(mac_ctrl_msg_t* src); 
mac_ctrl_msg_t cp_mac_ctrl_msg(mac_ctrl_msg_t* src);
bool eq_mac_ctrl_msg(mac_ctrl_msg_t* m0, mac_ctrl_msg_t* m1);

//////////////////////////////////////
// 6. RAN Function Definition (重點：必須包含這三個宣告)
/////////////////////////////////////

typedef struct {
  size_t len;
  uint8_t* buf;
} mac_func_def_t;

// --- 這裡是關鍵的宣告 ---
void free_mac_func_def(mac_func_def_t* src); 
mac_func_def_t cp_mac_func_def(mac_func_def_t const* src);
bool eq_mac_func_def(mac_func_def_t const* m0, mac_func_def_t const* m1);

//////////////////////////////////////
// 7. 其他 E2 過程相關
/////////////////////////////////////

typedef struct { uint32_t dummy; } mac_call_proc_id_t;
typedef struct { uint32_t dummy; } mac_ctrl_hdr_t;

typedef enum { MAC_CTRL_OUT_OK, MAC_CTRL_OUT_FAIL } mac_ctrl_out_e;
typedef struct { mac_ctrl_out_e ans; } mac_ctrl_out_t;

typedef struct {
  mac_event_trigger_t et; 
  mac_action_def_t* ad;
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

typedef struct { mac_ctrl_out_t* out; } mac_ctrl_out_data_t;

typedef struct { mac_func_def_t func_def; } mac_e2_setup_data_t;
typedef struct { mac_func_def_t func_def; } mac_ric_service_update_t;

// 基礎組合函式原型
void free_mac_ind_data(mac_ind_data_t* ind);
mac_ind_data_t cp_mac_ind_data(mac_ind_data_t const* src);

#ifdef __cplusplus
}
#endif

#endif