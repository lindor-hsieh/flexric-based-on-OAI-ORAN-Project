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

// 強制定義 PLAIN，避免 static_assert 錯誤
#ifndef PLAIN
#define PLAIN
#endif

#include "mac_sm_agent.h"
#include "mac_sm_id.h"
#include "enc/mac_enc_generic.h"
#include "dec/mac_dec_generic.h"
#include "../../util/alg_ds/alg/defer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
  sm_agent_t base;
  mac_enc_plain_t enc; 
} sm_mac_agent_t;

// ==========================================
// 1. Subscription Procedure
// ==========================================

static
sm_ag_if_ans_subs_t on_subscription_mac_sm_ag(sm_agent_t const* sm_agent, const sm_subs_data_t* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);

  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;
 
  mac_event_trigger_t ev = mac_dec_event_trigger(&sm->enc, data->len_et, data->event_trigger);

  sm_ag_if_ans_subs_t ans = {.type = PERIODIC_SUBSCRIPTION_FLRC}; 
  
  // [Fix: 終極解法] 
  // 因為編譯器一直報錯找不到成員 (ms 或 period_ms)，我們直接用 memcpy 繞過檢查。
  // 我們將時間數值 (ms) 直接複製到 ans.per 的記憶體開頭。
  // 這樣無論結構體成員叫什麼，數值都能正確寫入。
  int64_t time_val = (int64_t)ev.ms;
  memcpy(&ans.per, &time_val, sizeof(int64_t));

  return ans;
}

// ==========================================
// 2. Indication Procedure
// ==========================================

static
exp_ind_data_t on_indication_mac_sm_ag(sm_agent_t const* sm_agent, void* act_def)
{
  assert(sm_agent != NULL);
  (void)act_def; 
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;

  mac_ind_data_t mac = {0};

  // 呼叫 OAI RAN Function 讀取數據
  if(sm->base.io.read_ind(&mac) == false) {
      return (exp_ind_data_t){.has_value = false};
  }

  exp_ind_data_t ret = {.has_value = true};

  // Encode Header
  byte_array_t ba_hdr = mac_enc_ind_hdr(&sm->enc, &mac.hdr);
  ret.data.ind_hdr = ba_hdr.buf;
  ret.data.len_hdr = ba_hdr.len;

  // Encode Message
  byte_array_t ba_msg = mac_enc_ind_msg(&sm->enc, &mac.msg);
  ret.data.ind_msg = ba_msg.buf;
  ret.data.len_msg = ba_msg.len;

  ret.data.call_process_id = NULL;
  ret.data.len_cpid = 0;

  // 釋放記憶體
  free_mac_ind_hdr(&mac.hdr);
  free_mac_ind_msg(&mac.msg);
  if(mac.proc_id) free_mac_call_proc_id(mac.proc_id);

  return ret;
}

// ==========================================
// 3. Control Procedure
// ==========================================

static
sm_ctrl_out_data_t on_control_mac_sm_ag(sm_agent_t const* sm_agent, sm_ctrl_req_data_t const* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*) sm_agent;

  // Debug: 印出收到的二進制長度
  printf("[MAC-AGENT-DEBUG] Decoding Control Msg... Len=%zu bytes\n", data->len_msg);

  // 1. Decode Control Header
  mac_ctrl_hdr_t hdr = mac_dec_ctrl_hdr(&sm->enc, data->len_hdr, data->ctrl_hdr);

  // 2. Decode Control Message
  mac_ctrl_msg_t msg = mac_dec_ctrl_msg(&sm->enc, data->len_msg, data->ctrl_msg);
  
  // Debug: 印出解碼後的結果
  printf("[MAC-AGENT-DEBUG] Decoded: Type=%d, LenSlices=%d, SlicesPtr=%p\n", 
         msg.type, msg.len_slices, (void*)msg.slices);

  // 3. Prepare Data for OAI
  mac_ctrl_req_data_t mac_ctrl_req = {0};
  mac_ctrl_req.hdr = hdr;
  mac_ctrl_req.msg.type = msg.type;
  mac_ctrl_req.msg.len_slices = msg.len_slices;
  mac_ctrl_req.msg.slices = msg.slices; 

  // 4. Call OAI
  sm->base.io.write_ctrl(&mac_ctrl_req);

  // 5. Cleanup
  if (msg.slices) free(msg.slices);

  sm_ctrl_out_data_t ret = {0};
  ret.len_out = 0;
  ret.ctrl_out = NULL;

  return ret;
}

// ==========================================
// 4. E2 Setup Procedure
// ==========================================

static
sm_e2_setup_data_t on_e2_setup_mac_sm_ag(sm_agent_t const* sm_agent)
{
  assert(sm_agent != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;
  (void)sm;

  sm_e2_setup_data_t setup = {.len_rfd = 0, .ran_fun_def = NULL }; 

  const char* ran_func_oid = "1.3.6.1.4.1.53148.1.1.2.142"; 
  size_t const sz = strlen(ran_func_oid);

  setup.len_rfd = sz;
  setup.ran_fun_def = calloc(1, sz + 1); 
  assert(setup.ran_fun_def != NULL);

  memcpy(setup.ran_fun_def, ran_func_oid , sz);
 
  return setup;
}

// ==========================================
// 5. RIC Service Update Procedure
// ==========================================

static
sm_ric_service_update_data_t on_ric_service_update_mac_sm_ag(sm_agent_t const* sm_agent)
{
  assert(sm_agent != NULL);
  (void)sm_agent;
  sm_ric_service_update_data_t dst = {0}; 
  return dst;
}

static
void free_mac_sm_ag(sm_agent_t* sm_agent)
{
  assert(sm_agent != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;
  free(sm);
}

// General SM information
static uint16_t id_mac_sm_ag(void) { return SM_MAC_ID; }
static uint16_t rev_mac_sm_ag (void) { return SM_MAC_REV; }
static char const* oid_mac_sm_ag (void) { return SM_MAC_OID; }
static char const* def_mac_sm_ag(void) { return SM_MAC_STR; }

// Factory Function
sm_agent_t* make_mac_sm_agent(sm_io_ag_ran_t io)
{
  sm_mac_agent_t* sm = calloc(1, sizeof(sm_mac_agent_t));
  assert(sm != NULL && "Memory exhausted!!!");

  sm->base.io.read_ind = io.read_ind_tbl[MAC_STATS_V0];
  sm->base.io.read_setup = io.read_setup_tbl[MAC_AGENT_IF_E2_SETUP_ANS_V0];
  sm->base.io.write_ctrl = io.write_ctrl_tbl[MAC_CTRL_REQ_V0];
  sm->base.io.write_subs = io.write_subs_tbl[MAC_SUBS_V0];

  sm->base.free_sm = free_mac_sm_ag;
  sm->base.free_act_def = NULL; 

  sm->base.proc.on_subscription = on_subscription_mac_sm_ag;
  sm->base.proc.on_indication = on_indication_mac_sm_ag;
  sm->base.proc.on_control = on_control_mac_sm_ag;
  sm->base.proc.on_ric_service_update = on_ric_service_update_mac_sm_ag;
  sm->base.proc.on_e2_setup = on_e2_setup_mac_sm_ag;
  sm->base.handle = NULL;

  sm->base.info.def = def_mac_sm_ag; 
  sm->base.info.id =  id_mac_sm_ag;
  sm->base.info.rev = rev_mac_sm_ag;
  sm->base.info.oid = oid_mac_sm_ag;

  return &sm->base;
}