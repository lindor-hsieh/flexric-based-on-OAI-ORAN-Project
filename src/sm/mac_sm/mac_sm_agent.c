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

#include "mac_sm_agent.h"
#include "dec/mac_dec_generic.h"
#include "mac_sm_id.h"
#include "enc/mac_enc_generic.h"
#include "enc/mac_enc_plain.h" // [修正] 明確引入
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// [移除 defer]
// #include "../../util/alg_ds/alg/defer.h" 

// [暴力修正] 直接定義 enc
typedef struct{
  sm_agent_t base;
  mac_enc_plain_t enc; 
} sm_mac_agent_t;


static
sm_ag_if_ans_subs_t on_subscription_mac_sm_ag(sm_agent_t const* sm_agent, const sm_subs_data_t* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);

  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;
  
  mac_event_trigger_t ev = mac_dec_event_trigger(&sm->enc, data->len_et, data->event_trigger);

  sm_ag_if_ans_subs_t ans = {.type = PERIODIC_SUBSCRIPTION_FLRC}; 
  ans.per.t.ms = ev.ms;
  return ans;
}

static
exp_ind_data_t on_indication_mac_sm_ag(sm_agent_t const* sm_agent, void* act_def)
{
  assert(sm_agent != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;

  exp_ind_data_t ret = {.has_value = true};

  // Fill Indication Header
  mac_ind_hdr_t hdr = {.dummy = 0 };
  byte_array_t ba_hdr = mac_enc_ind_hdr(&sm->enc, &hdr);
  ret.data.ind_hdr = ba_hdr.buf;
  ret.data.len_hdr = ba_hdr.len;

  // Fill Indication Message 
  mac_ind_data_t mac = {0};
  
  // [修正] 移除 defer，手動檢查錯誤並釋放
  if(sm->base.io.read_ind(&mac) == false) {
      // 讀取失敗，直接返回 (注意：read_ind 失敗通常不會分配記憶體)
      return (exp_ind_data_t){.has_value = false};
  }

  // Encode the message
  byte_array_t ba = mac_enc_ind_msg(&sm->enc, &mac.msg);
  ret.data.ind_msg = ba.buf;
  ret.data.len_msg = ba.len;
  ret.data.call_process_id = NULL;
  ret.data.len_cpid = 0;

  // [修正] 手動釋放 read_ind 分配的記憶體
  free_mac_ind_hdr(&mac.hdr);
  free_mac_ind_msg(&mac.msg);
  free_mac_call_proc_id(mac.proc_id);

  return ret;
}

static
sm_ctrl_out_data_t on_control_mac_sm_ag(sm_agent_t const* sm_agent, sm_ctrl_req_data_t const* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*) sm_agent;

  mac_ctrl_hdr_t hdr = mac_dec_ctrl_hdr(&sm->enc, data->len_hdr, data->ctrl_hdr);
  mac_ctrl_msg_t msg = mac_dec_ctrl_msg(&sm->enc, data->len_msg, data->ctrl_msg);
  
  mac_ctrl_req_data_t mac_ctrl = {0};
  mac_ctrl.hdr.dummy = hdr.dummy;
  
  // [安全複製]
  mac_ctrl.msg.type = msg.type;
  if (msg.type == 0) {
      mac_ctrl.msg.len_slices = msg.len_slices;
      mac_ctrl.msg.slices = msg.slices; 
  }

  // 寫入 OAI
  sm->base.io.write_ctrl(&mac_ctrl);

  // 清理
  free_mac_ctrl_msg(&msg);

  sm_ctrl_out_data_t ret = {0};
  ret.len_out = 0;
  ret.ctrl_out = NULL;

  return ret;
}

static
sm_e2_setup_data_t on_e2_setup_mac_sm_ag(sm_agent_t const* sm_agent)
{
  assert(sm_agent != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;
  (void)sm;

  sm_e2_setup_data_t setup = {.len_rfd = 0, .ran_fun_def = NULL }; 

  size_t const sz = strnlen(SM_MAC_STR, 256);
  setup.len_rfd = sz;
  setup.ran_fun_def = calloc(1, sz);
  assert(setup.ran_fun_def != NULL);
  memcpy(setup.ran_fun_def, SM_MAC_STR , sz);
  
  return setup;
}

static
 sm_ric_service_update_data_t on_ric_service_update_mac_sm_ag(sm_agent_t const* sm_agent)
{
  assert(sm_agent != NULL);
  sm_ric_service_update_data_t dst = {0}; 
  (void)sm_agent;
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
static char const* def_mac_sm_ag(void) { return SM_MAC_STR; }
static uint16_t id_mac_sm_ag(void) { return SM_MAC_ID; }
static uint16_t rev_mac_sm_ag (void) { return SM_MAC_REV; }
static char const* oid_mac_sm_ag (void) { return SM_MAC_OID; }

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