/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance ...
 */

#include "mac_sm_agent.h"
#include "dec/mac_dec_generic.h"
#include "mac_sm_id.h"
#include "enc/mac_enc_generic.h"
#include "enc/mac_enc_plain.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 結構定義
// ---------------------------------------------------------------------------
typedef struct {
  sm_agent_t base;
  mac_enc_plain_t enc; 
} sm_mac_agent_t;

// ---------------------------------------------------------------------------
// 1. 處理訂閱 (Subscription)
// ---------------------------------------------------------------------------
static
sm_ag_if_ans_subs_t on_subscription_mac_sm_ag(sm_agent_t const* sm_agent, const sm_subs_data_t* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);

  // 強制設定回報週期為 1000ms
  sm_ag_if_ans_subs_t ans = {.type = PERIODIC_SUBSCRIPTION_FLRC}; 
  ans.per.t.ms = 1000; 

  printf("[OAI-E2-AGENT] >>> MAC SM Subscription Success! Interval: 1000ms <<<\n");
  return ans;
}

// ---------------------------------------------------------------------------
// 2. 處理數據回報 (Indication) - gNB -> RIC
// ---------------------------------------------------------------------------
static
exp_ind_data_t on_indication_mac_sm_ag(sm_agent_t const* sm_agent, void* act_def)
{
  assert(sm_agent != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*)sm_agent;

  exp_ind_data_t ret = {.has_value = true};

  // A. 填充 Indication Header (dummy)
  mac_ind_hdr_t hdr = {.dummy = 0 };
  byte_array_t ba_hdr = mac_enc_ind_hdr(&sm->enc, &hdr);
  ret.data.ind_hdr = ba_hdr.buf;
  ret.data.len_hdr = ba_hdr.len;

  // B. 讀取並填充 Indication Message (UE Stats)
  mac_ind_data_t mac = {0};
  
  // 調用 OAI 內部的讀取函式 (read_mac_sm)
  if(sm->base.io.read_ind(&mac) == false) {
      return (exp_ind_data_t){.has_value = false};
  }

  // 編碼成二進制發送
  byte_array_t ba = mac_enc_ind_msg(&sm->enc, &mac.msg);
  // printf("[OAI-E2-AGENT] Indication Sent: %u bytes (%u UEs)\n", (unsigned int)ba.len, mac.msg.len_ue_stats);
  
  ret.data.ind_msg = ba.buf;
  ret.data.len_msg = ba.len;
  ret.data.call_process_id = NULL;
  ret.data.len_cpid = 0;

  // C. 釋放 read_ind 產生的臨時記憶體
  free_mac_ind_hdr(&mac.hdr);
  free_mac_ind_msg(&mac.msg);
  if(mac.proc_id) free_mac_call_proc_id(mac.proc_id);

  return ret;
}

// ---------------------------------------------------------------------------
// 3. 處理控制指令 (Control) - RIC -> gNB (切片控制核心)
// ---------------------------------------------------------------------------
static
sm_ctrl_out_data_t on_control_mac_sm_ag(sm_agent_t const* sm_agent, sm_ctrl_req_data_t const* data)
{
  assert(sm_agent != NULL);
  assert(data != NULL);
  sm_mac_agent_t* sm = (sm_mac_agent_t*) sm_agent;

  // A. 解碼來自 xApp 的切片配置
  // mac_dec_ctrl_msg 會為 msg.slices 分配記憶體 (calloc)
  mac_ctrl_msg_t msg = mac_dec_ctrl_msg(&sm->enc, data->len_msg, data->ctrl_msg);
  
  printf("[OAI-E2-AGENT] <<< Received Control! Type: %u, Slices: %u >>>\n", msg.type, msg.len_slices);

  // B. 準備傳遞給 OAI 內部的結構
  mac_ctrl_req_data_t mac_ctrl = {0};
  mac_ctrl.hdr.dummy = 0;
  mac_ctrl.msg = msg; // 直接將解碼後的內容（含 slices 指標）傳給 OAI

  // C. 寫入 OAI (執行真正的切片比例修改)
  // 注意：在 OAI 同步環境中，write_ctrl 執行完代表 OAI 已經讀取完數據
  sm->base.io.write_ctrl(&mac_ctrl);

  // D. 關鍵：執行完後才釋放解碼產生的記憶體
  // 這會 free(msg.slices)，防止記憶體洩漏
  free_mac_ctrl_msg(&msg);

  // E. 回傳結果 (這裡通常不帶資料)
  sm_ctrl_out_data_t ret = {0};
  ret.len_out = 0;
  ret.ctrl_out = NULL;

  return ret;
}

// ---------------------------------------------------------------------------
// 4. E2 Setup & 其他
// ---------------------------------------------------------------------------
static
sm_e2_setup_data_t on_e2_setup_mac_sm_ag(sm_agent_t const* sm_agent)
{
  assert(sm_agent != NULL);
  size_t const sz = strnlen(SM_MAC_STR, 256);
  sm_e2_setup_data_t setup = {.len_rfd = sz}; 
  setup.ran_fun_def = calloc(1, sz);
  assert(setup.ran_fun_def != NULL);
  memcpy(setup.ran_fun_def, SM_MAC_STR , sz);
  return setup;
}

static
sm_ric_service_update_data_t on_ric_service_update_mac_sm_ag(sm_agent_t const* sm_agent)
{
  return (sm_ric_service_update_data_t){0};
}

static
void free_mac_sm_ag(sm_agent_t* sm_agent)
{
  assert(sm_agent != NULL);
  free(sm_agent);
}

// 5. SM 資訊定義
static char const* def_mac_sm_ag(void) { return SM_MAC_STR; }
static uint16_t id_mac_sm_ag(void) { return SM_MAC_ID; }
static uint16_t rev_mac_sm_ag (void) { return SM_MAC_REV; }
static char const* oid_mac_sm_ag (void) { return SM_MAC_OID; }

// ---------------------------------------------------------------------------
// 構造函式：建立 MAC SM Agent 插件
// ---------------------------------------------------------------------------
sm_agent_t* make_mac_sm_agent(sm_io_ag_ran_t io)
{
  sm_mac_agent_t* sm = calloc(1, sizeof(sm_mac_agent_t));
  assert(sm != NULL && "Memory exhausted!!!");

  // 映射 OAI 內部的數據接口 (來自 ran_func_mac.c)
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