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

// [Fix 1] 強制定義 PLAIN，解決 "No encryption type selected" 錯誤
#ifndef PLAIN
#define PLAIN
#endif

#include "mac_sm_ric.h"
#include "mac_sm_id.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "enc/mac_enc_generic.h"
#include "dec/mac_dec_generic.h"

typedef struct{
  sm_ric_t base;
  // [Fix 2] 直接宣告 enc 成員，移除 #ifdef 判斷，確保結構成員存在
  mac_enc_plain_t enc;
} sm_mac_ric_t;

// ==========================================
// 1. Subscription Procedure
// ==========================================

static
sm_subs_data_t on_subscription_mac_sm_ric(sm_ric_t const* sm_ric, void* cmd)
{
  assert(sm_ric != NULL); 
  assert(cmd != NULL); 

  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;  
 
  mac_sub_data_t mac = {0}; 

  // 解析訂閱參數
  const int max_str_sz = 10;
  if(strncmp(cmd, "1_ms", max_str_sz) == 0 ){
    mac.et.ms = 1;
  } else if (strncmp(cmd, "2_ms", max_str_sz) == 0 ) {
    mac.et.ms = 2;
  } else if (strncmp(cmd, "5_ms", max_str_sz) == 0 ) {
    mac.et.ms = 5;
  } else if (strncmp(cmd, "10_ms", max_str_sz) == 0 ) {
    mac.et.ms = 10;
  } else if (strncmp(cmd, "100_ms", max_str_sz) == 0 ) {
    mac.et.ms = 100;
  } else {
    mac.et.ms = 1000; 
  }
  
  // Encode Event trigger (現在 sm->enc 肯定存在了)
  const byte_array_t ba = mac_enc_event_trigger(&sm->enc, &mac.et); 

  sm_subs_data_t data = {0}; 
  data.event_trigger = ba.buf;
  data.len_et = ba.len;
  data.action_def = NULL;
  data.len_ad = 0;

  return data;
}

// ==========================================
// 2. Indication Procedure
// ==========================================

static
sm_ag_if_rd_ind_t on_indication_mac_sm_ric(sm_ric_t const* sm_ric, sm_ind_data_t const* data)
{
  assert(sm_ric != NULL); 
  assert(data != NULL); 
  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;  

  sm_ag_if_rd_ind_t rd_if = {0};
  rd_if.type = MAC_STATS_V0;

  // Decode Header & Message
  rd_if.mac.hdr = mac_dec_ind_hdr(&sm->enc, data->len_hdr, data->ind_hdr);
  rd_if.mac.msg = mac_dec_ind_msg(&sm->enc, data->len_msg, data->ind_msg);
  rd_if.mac.proc_id = NULL;

  return rd_if;
}

// ==========================================
// 3. Control Request
// ==========================================

static
sm_ctrl_req_data_t ric_on_control_req_mac_sm_ric(sm_ric_t const* sm_ric, void* ctrl)
{
  assert(sm_ric != NULL); 
  assert(ctrl != NULL); 
  
  mac_ctrl_req_data_t const* req = (mac_ctrl_req_data_t const*)ctrl;
  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;  

  sm_ctrl_req_data_t ret_data = {0};  

  // Encode Header
  byte_array_t ba_hdr = mac_enc_ctrl_hdr(&sm->enc, &req->hdr);
  ret_data.ctrl_hdr = ba_hdr.buf;
  ret_data.len_hdr = ba_hdr.len;

  // Encode Message (支援 2D Slice 參數)
  byte_array_t ba_msg = mac_enc_ctrl_msg(&sm->enc, &req->msg);
  ret_data.ctrl_msg = ba_msg.buf;
  ret_data.len_msg = ba_msg.len;

  return ret_data;
}

static
sm_ag_if_ans_ctrl_t ric_on_control_out_mac_sm_ric(sm_ric_t const* sm_ric, const sm_ctrl_out_data_t * out)
{
  assert(sm_ric != NULL); 
  assert(out != NULL);

  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;  

  sm_ag_if_ans_ctrl_t ag_if = {0};
  ag_if.type = MAC_AGENT_IF_CTRL_ANS_V0;
  
  ag_if.mac = mac_dec_ctrl_out(&sm->enc, out->len_out, out->ctrl_out);

  return ag_if;
}

// ==========================================
// 4. E2 Setup Procedure
// ==========================================

static
sm_ag_if_rd_e2setup_t ric_on_e2_setup_mac_sm_ric(sm_ric_t const* sm_ric, sm_e2_setup_data_t const* setup)
{
  assert(sm_ric != NULL); 
  assert(setup != NULL); 

  sm_ag_if_rd_e2setup_t dst = {0};
  dst.type = MAC_AGENT_IF_E2_SETUP_ANS_V0;
  
  dst.mac.func_def.len = setup->len_rfd;
  if(dst.mac.func_def.len > 0){
    dst.mac.func_def.buf = calloc(dst.mac.func_def.len, sizeof(uint8_t));
    assert(dst.mac.func_def.buf != NULL && "Memory exhausted");
    memcpy(dst.mac.func_def.buf, setup->ran_fun_def, setup->len_rfd);
  }

  return dst;
}

// ==========================================
// 5. Service Update Procedure
// ==========================================

static
sm_ag_if_rd_rsu_t on_ric_service_update_mac_sm_ric(sm_ric_t const* sm_ric, sm_ric_service_update_data_t const* rsu)
{
  assert(sm_ric != NULL); 
  assert(rsu != NULL); 
  sm_ag_if_rd_rsu_t dst = {0};
  return dst;
}

// ==========================================
// Memory Management
// ==========================================

static
void free_mac_sm_ric(sm_ric_t* sm_ric)
{
  assert(sm_ric != NULL);
  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;
  free(sm);
}

static
void free_subs_data_mac_sm_ric(void* msg)
{
}

static
void free_ind_data_mac_sm_ric(void* msg)
{
  assert(msg != NULL);
  sm_ag_if_rd_ind_t* rd_ind = (sm_ag_if_rd_ind_t*)msg;

  if (rd_ind->type == MAC_STATS_V0) {
      free_mac_ind_hdr(&rd_ind->mac.hdr);
      free_mac_ind_msg(&rd_ind->mac.msg);
      if (rd_ind->mac.proc_id) free_mac_call_proc_id(rd_ind->mac.proc_id);
  }
}

static
void free_ctrl_req_data_mac_sm_ric(void* msg)
{
}

static
void free_ctrl_out_data_mac_sm_ric(void* msg)
{
}

static
void free_e2_setup_mac_sm_ric(void* msg)
{
  if(msg == NULL) return;
  sm_ag_if_rd_e2setup_t* rd = (sm_ag_if_rd_e2setup_t*)msg;
  if(rd->mac.func_def.buf != NULL){
    free(rd->mac.func_def.buf);
  }
}

static
void free_ric_service_update_mac_sm_ric(void* msg)
{
}

// ==========================================
// Factory Function
// ==========================================

__attribute__((visibility("default")))
sm_ric_t* make_mac_sm_ric(void)
{
  sm_mac_ric_t* sm = calloc(1, sizeof(sm_mac_ric_t));
  assert(sm != NULL && "Memory exhausted");
  
  *((uint16_t*)&sm->base.ran_func_id) = SM_MAC_ID;

  sm->base.free_sm = free_mac_sm_ric;

  sm->base.alloc.free_subs_data_msg = free_subs_data_mac_sm_ric; 
  sm->base.alloc.free_ind_data = free_ind_data_mac_sm_ric ; 
  sm->base.alloc.free_ctrl_req_data = free_ctrl_req_data_mac_sm_ric; 
  sm->base.alloc.free_ctrl_out_data = free_ctrl_out_data_mac_sm_ric; 
  sm->base.alloc.free_e2_setup = free_e2_setup_mac_sm_ric; 
  sm->base.alloc.free_ric_service_update = free_ric_service_update_mac_sm_ric; 

  sm->base.proc.on_subscription = on_subscription_mac_sm_ric; 
  sm->base.proc.on_indication = on_indication_mac_sm_ric;
  sm->base.proc.on_control_req = ric_on_control_req_mac_sm_ric;
  sm->base.proc.on_control_out = ric_on_control_out_mac_sm_ric;
  sm->base.proc.on_e2_setup = ric_on_e2_setup_mac_sm_ric;
  sm->base.proc.on_ric_service_update = on_ric_service_update_mac_sm_ric; 
  
  sm->base.handle = NULL;

  assert(strlen(SM_MAC_STR) < sizeof(sm->base.ran_func_name));
  memcpy(sm->base.ran_func_name, SM_MAC_STR, strlen(SM_MAC_STR));

  return &sm->base;
}

uint16_t id_sm_mac_ric(sm_ric_t const* sm_ric)
{
  assert(sm_ric != NULL);
  sm_mac_ric_t* sm = (sm_mac_ric_t*)sm_ric;
  return sm->base.ran_func_id;
}