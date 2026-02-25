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

#include "mac_data_ie.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ==========================================
// RIC Event Trigger Definition
// ==========================================

void free_mac_event_trigger(mac_event_trigger_t* src)
{
  assert(src != NULL);
  // Event Trigger is scalar, nothing to free
}

mac_event_trigger_t cp_mac_event_trigger(mac_event_trigger_t const* src)
{
  assert(src != NULL);
  mac_event_trigger_t et = {0};
  et.ms = src->ms; 
  return et;
}

bool eq_mac_event_trigger(mac_event_trigger_t const* m0, mac_event_trigger_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);
  return m0->ms == m1->ms;
}

// ==========================================
// RIC Action Definition 
// ==========================================

void free_mac_action_def(mac_action_def_t* src)
{
  assert(src != NULL);
}

mac_action_def_t cp_mac_action_def(mac_action_def_t const* src)
{
  assert(src != NULL);
  mac_action_def_t ad = {0};
  ad.dummy = src->dummy;
  return ad;
}

bool eq_mac_action_def(mac_action_def_t const* m0, mac_action_def_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);
  return m0->dummy == m1->dummy;
}

// ==========================================
// RIC Indication Header 
// ==========================================

void free_mac_ind_hdr(mac_ind_hdr_t* src)
{
  assert(src != NULL);
}

mac_ind_hdr_t cp_mac_ind_hdr(mac_ind_hdr_t const* src)
{
  assert(src != NULL);
  mac_ind_hdr_t dst = {0}; 
  dst.dummy = src->dummy;
  return dst;
}

bool eq_mac_ind_hdr(mac_ind_hdr_t const* m0, mac_ind_hdr_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);
  return m0->dummy == m1->dummy;
}

// ==========================================
// RIC Indication Message 
// ==========================================

void free_mac_ind_msg(mac_ind_msg_t* src)
{
  assert(src != NULL);
  if(src->len_ue_stats > 0 && src->ue_stats != NULL){
    free(src->ue_stats);
  }
}

mac_ind_msg_t cp_mac_ind_msg(mac_ind_msg_t const* src)
{
  assert(src != NULL);
  mac_ind_msg_t dst = {0};

  dst.tstamp = src->tstamp;
  dst.len_ue_stats = src->len_ue_stats;

  if(dst.len_ue_stats > 0){
    dst.ue_stats = calloc(dst.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(dst.ue_stats != NULL && "Memory exhausted");
    // Directly copy the array of structs. This works because mac_ue_stats_impl_t
    // contains only scalar values (no pointers). If pointers are added later,
    // a loop with deep copy is needed.
    memcpy(dst.ue_stats, src->ue_stats, dst.len_ue_stats * sizeof(mac_ue_stats_impl_t));
  }

  return dst;
}

bool eq_mac_ind_msg(mac_ind_msg_t const* m0, mac_ind_msg_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);

  if(m0->len_ue_stats != m1->len_ue_stats || m0->tstamp != m1->tstamp)
    return false;

  if(m0->len_ue_stats > 0) {
    if (memcmp(m0->ue_stats, m1->ue_stats, m0->len_ue_stats * sizeof(mac_ue_stats_impl_t)) != 0) {
        return false;
    }
  }
  return true;
}

// ==========================================
// RIC Call Process ID 
// ==========================================

void free_mac_call_proc_id(mac_call_proc_id_t* src)
{
  // Normally freed by the caller if allocated dynamically
  (void)src; 
}

mac_call_proc_id_t cp_mac_call_proc_id(mac_call_proc_id_t const* src)
{
  assert(src != NULL); 
  mac_call_proc_id_t dst = {0};
  dst.dummy = src->dummy;
  return dst;
}

bool eq_mac_call_proc_id(mac_call_proc_id_t const* m0, mac_call_proc_id_t const* m1)
{
  if(m0 == m1) return true;
  if(m0 == NULL || m1 == NULL) return false;
  return m0->dummy == m1->dummy;
}

// ==========================================
// RIC Control Header 
// ==========================================

void free_mac_ctrl_hdr(mac_ctrl_hdr_t* src)
{
  assert(src != NULL);
  // No dynamic memory to free
}

mac_ctrl_hdr_t cp_mac_ctrl_hdr(mac_ctrl_hdr_t const* src)
{
  assert(src != NULL);
  mac_ctrl_hdr_t ret = {0};
  ret.dummy = src->dummy;
  return ret;
}

bool eq_mac_ctrl_hdr(mac_ctrl_hdr_t const* m0, mac_ctrl_hdr_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);
  return m0->dummy == m1->dummy;
}

// ==========================================
// RIC Control Message (The Core Logic for 2D Control)
// ==========================================

void free_mac_ctrl_msg(mac_ctrl_msg_t* src)
{
  assert(src != NULL);
  // [Memory Management] Important for 2D Control
  if (src->slices != NULL && src->len_slices > 0) {
      free(src->slices);
      src->slices = NULL;
  }
}

mac_ctrl_msg_t cp_mac_ctrl_msg(mac_ctrl_msg_t const* src)
{
  assert(src != NULL);
  mac_ctrl_msg_t ret = {0};
  
  ret.type = src->type;
  ret.len_slices = src->len_slices;

  if (ret.len_slices > 0) {
      ret.slices = calloc(ret.len_slices, sizeof(mac_slice_params_t));
      assert(ret.slices != NULL && "Memory exhausted in cp_mac_ctrl_msg");
      
      // [2D Control] Copy slice params including slot_mask and prb_quota
      // We use memcpy for efficiency as mac_slice_params_t is flat
      memcpy(ret.slices, src->slices, ret.len_slices * sizeof(mac_slice_params_t));
  }

  return ret;
}

bool eq_mac_ctrl_msg(mac_ctrl_msg_t const* m0, mac_ctrl_msg_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);

  if (m0->type != m1->type) return false;
  if (m0->len_slices != m1->len_slices) return false;

  if (m0->len_slices > 0) {
      // Compare the content of slice configuration
      if (memcmp(m0->slices, m1->slices, m0->len_slices * sizeof(mac_slice_params_t)) != 0) {
          return false;
      }
  }

  return true;
}

// ==========================================
// RIC Control Outcome 
// ==========================================

void free_mac_ctrl_out(mac_ctrl_out_t* src)
{
  assert(src != NULL);
}

mac_ctrl_out_t cp_mac_ctrl_out(mac_ctrl_out_t const* src)
{
  assert(src != NULL);
  mac_ctrl_out_t ret = {0}; 
  ret.ans = src->ans;
  return ret;
}

bool eq_mac_ctrl_out(mac_ctrl_out_t const* m0, mac_ctrl_out_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);
  return m0->ans == m1->ans;
}

// ==========================================
// RIC Control Request Data Wrapper
// ==========================================

void free_mac_ctrl_req_data(mac_ctrl_req_data_t* src)
{
  assert(src != NULL);
  free_mac_ctrl_hdr(&src->hdr);
  free_mac_ctrl_msg(&src->msg);
}

mac_ctrl_req_data_t cp_mac_ctrl_req_data(mac_ctrl_req_data_t const* src)
{
  assert(src != NULL);
  mac_ctrl_req_data_t dst = {0};

  dst.hdr = cp_mac_ctrl_hdr(&src->hdr);
  dst.msg = cp_mac_ctrl_msg(&src->msg);

  return dst;
}

bool eq_mac_ctrl_req_data(mac_ctrl_req_data_t const* m0, mac_ctrl_req_data_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);

  if (eq_mac_ctrl_hdr(&m0->hdr, &m1->hdr) == false) return false;
  if (eq_mac_ctrl_msg(&m0->msg, &m1->msg) == false) return false;

  return true;
}

// ==========================================
// RIC Control Outcome Data Wrapper
// ==========================================

void free_mac_ctrl_out_data(mac_ctrl_out_data_t* src)
{
  assert(src != NULL);
  free_mac_ctrl_hdr(&src->hdr);
  free_mac_ctrl_out(&src->out);
}

mac_ctrl_out_data_t cp_mac_ctrl_out_data(mac_ctrl_out_data_t const* src)
{
  assert(src != NULL);
  mac_ctrl_out_data_t dst = {0};
  
  dst.hdr = cp_mac_ctrl_hdr(&src->hdr);
  dst.out = cp_mac_ctrl_out(&src->out);

  return dst;
}

bool eq_mac_ctrl_out_data(mac_ctrl_out_data_t const* m0, mac_ctrl_out_data_t const* m1)
{
  assert(m0 != NULL);
  assert(m1 != NULL);

  if (eq_mac_ctrl_hdr(&m0->hdr, &m1->hdr) == false) return false;
  if (eq_mac_ctrl_out(&m0->out, &m1->out) == false) return false;

  return true;
}

// ==========================================
// RAN Function Definition 
// ==========================================

void free_mac_func_def(mac_func_def_t* src)
{
  assert(src != NULL);
  // buffer is a uint8_t array, so simple free is enough
  if(src->buf) free(src->buf);
}

mac_func_def_t cp_mac_func_def(mac_func_def_t const* src)
{
  assert(src != NULL);

  mac_func_def_t dst = {0};
  dst.len = src->len;

  if(src->len > 0){
    dst.buf = calloc(dst.len, sizeof(uint8_t)); 
    assert(dst.buf != NULL && "Memory exhausted");
    memcpy(dst.buf, src->buf, dst.len);
  }

  return dst;
}

bool eq_mac_func_def(mac_func_def_t const* m0, mac_func_def_t const* m1)
{
  if(m0 == m1) return true;
  if(m0 == NULL || m1 == NULL) return false;
  if(m0->len != m1->len) return false;
  return memcmp(m0->buf, m1->buf, m0->len) == 0;
}

// ==========================================
// RIC Indication Data Wrapper
// ==========================================

mac_ind_data_t cp_mac_ind_data(mac_ind_data_t const* src)
{
  assert(src != NULL);
  mac_ind_data_t dst = {0};
  
  dst.hdr = cp_mac_ind_hdr(&src->hdr);
  dst.msg = cp_mac_ind_msg(&src->msg);
  
  if(src->proc_id != NULL){
    dst.proc_id = malloc(sizeof(mac_call_proc_id_t)); 
    assert(dst.proc_id != NULL && "Memory exhausted");
    *dst.proc_id = cp_mac_call_proc_id(src->proc_id);
  }

  return dst;
}

void free_mac_ind_data(mac_ind_data_t* ind)
{
  if (ind == NULL) return;
  free_mac_ind_hdr(&ind->hdr);
  free_mac_ind_msg(&ind->msg);
  if (ind->proc_id) {
      free_mac_call_proc_id(ind->proc_id);
      free(ind->proc_id);
  }
}