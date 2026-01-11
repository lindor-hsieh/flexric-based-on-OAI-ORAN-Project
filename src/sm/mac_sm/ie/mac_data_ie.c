/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance ...
 */

#include "mac_data_ie.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../util/alg_ds/alg/eq_float.h"

//////////////////////////////////////
// 1. RIC Event Trigger Definition
/////////////////////////////////////

void free_mac_event_trigger(mac_event_trigger_t* src) { (void)src; }

mac_event_trigger_t cp_mac_event_trigger(mac_event_trigger_t const* src)
{
  assert(src != NULL);
  mac_event_trigger_t et = {.ms = src->ms};
  return et;
}

bool eq_mac_event_trigger(mac_event_trigger_t const* m0, mac_event_trigger_t const* m1)
{
  assert(m0 != NULL && m1 != NULL);
  return m0->ms == m1->ms;
}

//////////////////////////////////////
// 2. RIC Action Definition 
/////////////////////////////////////

void free_mac_action_def(mac_action_def_t* src) { (void)src; }

mac_action_def_t cp_mac_action_def(mac_action_def_t* src)
{
  assert(src != NULL);
  mac_action_def_t ad = {.dummy = src->dummy};
  return ad;
}

bool eq_mac_action_def(mac_action_def_t const* m0, mac_action_def_t const* m1)
{
  assert(m0 != NULL && m1 != NULL);
  return m0->dummy == m1->dummy;
}

//////////////////////////////////////
// 3. RIC Indication Header 
/////////////////////////////////////

void free_mac_ind_hdr(mac_ind_hdr_t* src) { (void)src; }

mac_ind_hdr_t cp_mac_ind_hdr(mac_ind_hdr_t const* src)
{
  assert(src != NULL);
  mac_ind_hdr_t dst = {.dummy = src->dummy}; 
  return dst;
}

bool eq_mac_ind_hdr(mac_ind_hdr_t* m0, mac_ind_hdr_t* m1)
{
  assert(m0 != NULL && m1 != NULL);
  return m0->dummy == m1->dummy;
}

//////////////////////////////////////
// 4. RIC Indication Message (UE Stats)
/////////////////////////////////////

void free_mac_ind_msg(mac_ind_msg_t* src)
{
  assert(src != NULL);
  if(src->len_ue_stats > 0 && src->ue_stats != NULL){
    free(src->ue_stats);
  }
}

mac_ue_stats_impl_t cp_mac_ue_stats_impl(mac_ue_stats_impl_t const* src)
{
  assert(src != NULL);
  mac_ue_stats_impl_t dst;
  memset(&dst, 0, sizeof(mac_ue_stats_impl_t));
  memcpy(&dst, src, sizeof(mac_ue_stats_impl_t));
  return dst;
}

mac_ind_msg_t cp_mac_ind_msg(mac_ind_msg_t const* src)
{
  assert(src != NULL);
  mac_ind_msg_t dst = {.len_ue_stats = src->len_ue_stats, .tstamp = src->tstamp};
  if(dst.len_ue_stats > 0){
    dst.ue_stats = calloc(dst.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(dst.ue_stats != NULL && "Memory exhausted");
    memcpy(dst.ue_stats, src->ue_stats, dst.len_ue_stats * sizeof(mac_ue_stats_impl_t));
  }
  return dst;
}

bool eq_mac_ind_msg(mac_ind_msg_t* m0, mac_ind_msg_t* m1)
{
  assert(m0 != NULL && m1 != NULL);
  if(m0->len_ue_stats != m1->len_ue_stats || m0->tstamp != m1->tstamp) return false;
  if(m0->len_ue_stats > 0) {
    return memcmp(m0->ue_stats, m1->ue_stats, m0->len_ue_stats * sizeof(mac_ue_stats_impl_t)) == 0;
  }
  return true;
}

//////////////////////////////////////
// 5. RIC Control Message
/////////////////////////////////////

void free_mac_ctrl_msg(mac_ctrl_msg_t* src)
{
  assert(src != NULL);
  if (src->slices != NULL) { 
      free(src->slices);
      src->slices = NULL;
  }
}

mac_ctrl_msg_t cp_mac_ctrl_msg(mac_ctrl_msg_t* src)
{
  assert(src != NULL);
  mac_ctrl_msg_t dst = {.type = src->type, .len_slices = src->len_slices};
  if (dst.len_slices > 0) {
      dst.slices = calloc(dst.len_slices, sizeof(mac_slice_params_t));
      assert(dst.slices != NULL);
      memcpy(dst.slices, src->slices, dst.len_slices * sizeof(mac_slice_params_t));
  }
  return dst;
}

bool eq_mac_ctrl_msg(mac_ctrl_msg_t* m0, mac_ctrl_msg_t* m1)
{
  assert(m0 != NULL && m1 != NULL);
  if (m0->type != m1->type || m0->len_slices != m1->len_slices) return false;
  if (m0->len_slices > 0) {
      return memcmp(m0->slices, m1->slices, m0->len_slices * sizeof(mac_slice_params_t)) == 0;
  }
  return true;
}

//////////////////////////////////////
// 6. RAN Function Definition (修正重點)
/////////////////////////////////////

void free_mac_func_def(mac_func_def_t* src)
{
  assert(src != NULL);
  if(src->buf) free(src->buf);
}

mac_func_def_t cp_mac_func_def(mac_func_def_t const* src)
{
  assert(src != NULL);
  mac_func_def_t dst = {.len = src->len};
  if(src->len > 0){
    dst.buf = calloc(src->len, 1);
    assert(dst.buf != NULL);
    memcpy(dst.buf, src->buf, src->len);
  }
  return dst;
}

// [核心修復] 補上漏掉的 eq 函式
bool eq_mac_func_def(mac_func_def_t const* m0, mac_func_def_t const* m1)
{
  if(m0 == m1) return true;
  if(m0 == NULL || m1 == NULL) return false;
  if(m0->len != m1->len) return false;
  return memcmp(m0->buf, m1->buf, m0->len) == 0;
}

//////////////////////////////////////
// 7. 其他 Boilerplate
/////////////////////////////////////

void free_mac_call_proc_id(mac_call_proc_id_t* src) { if(src) free(src); }
mac_call_proc_id_t cp_mac_call_proc_id(mac_call_proc_id_t* src) { 
  assert(src != NULL); return (mac_call_proc_id_t){.dummy = src->dummy}; 
}
bool eq_mac_call_proc_id(mac_call_proc_id_t* m0, mac_call_proc_id_t* m1) {
  if(m0 == m1) return true; if(!m0 || !m1) return false; return m0->dummy == m1->dummy;
}

void free_mac_ctrl_hdr(mac_ctrl_hdr_t* src) { (void)src; }
mac_ctrl_hdr_t cp_mac_ctrl_hdr(mac_ctrl_hdr_t* src) { 
  assert(src != NULL); return (mac_ctrl_hdr_t){.dummy = src->dummy}; 
}
bool eq_mac_ctrl_hdr(mac_ctrl_hdr_t* m0, mac_ctrl_hdr_t* m1) {
  assert(m0 != NULL && m1 != NULL); return m0->dummy == m1->dummy;
}

void free_mac_ctrl_out(mac_ctrl_out_t* src) { (void)src; }
mac_ctrl_out_t cp_mac_ctrl_out(mac_ctrl_out_t* src) { 
  assert(src != NULL); return (mac_ctrl_out_t){.ans = src->ans}; 
}
bool eq_mac_ctrl_out(mac_ctrl_out_t* m0, mac_ctrl_out_t* m1) {
  assert(m0 != NULL && m1 != NULL); return m0->ans == m1->ans;
}

mac_ind_data_t cp_mac_ind_data(mac_ind_data_t const* src)
{
  assert(src != NULL);
  mac_ind_data_t dst = {.hdr = cp_mac_ind_hdr(&src->hdr), .msg = cp_mac_ind_msg(&src->msg)};
  if(src->proc_id) {
    dst.proc_id = malloc(sizeof(mac_call_proc_id_t));
    *dst.proc_id = cp_mac_call_proc_id(src->proc_id);
  }
  return dst;
}

void free_mac_ind_data(mac_ind_data_t* ind)
{
  if(!ind) return;
  free_mac_ind_hdr(&ind->hdr);
  free_mac_ind_msg(&ind->msg);
  if(ind->proc_id) free_mac_call_proc_id(ind->proc_id);
}