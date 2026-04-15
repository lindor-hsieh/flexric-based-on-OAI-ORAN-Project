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

#include "map_e2_node_sockaddr.h"

#include "../util/alg_ds/ds/lock_guard/lock_guard.h"
#include "../util/alg_ds/alg/alg.h"
#include "../lib/ep/sctp_msg.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline
void free_sctp_info(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  sctp_info_t* s = (sctp_info_t*)value;
  free(s);
}

static
void free_global_e2_node(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  global_e2_node_id_t* id = (global_e2_node_id_t*)value;
  free_global_e2_node_id(id); 
  free(id);
}

void init_map_e2_node_sad(map_e2_node_sockaddr_t* m)
{
  assert(m != NULL);

  pthread_mutexattr_t *mtx_attr = NULL;
#ifdef DEBUG
  *mtx_attr = PTHREAD_MUTEX_ERRORCHECK; 
#endif

  int rc = pthread_mutex_init(&m->mtx, mtx_attr);
  assert(rc == 0);

//  const size_t key_sz = sizeof(global_e2_node_id_t);
//  assoc_init(&m->tree, key_sz, cmp_global_e2_node_id_wrapper, free_sctp_info);

  const size_t key_sz_1 = sizeof(global_e2_node_id_t);
  const size_t key_sz_2 = sizeof(sctp_info_t);

  bi_map_init(&m->map, key_sz_1, key_sz_2, cmp_global_e2_node_id_wrapper, cmp_sctp_info_wrapper, free_sctp_info, free_global_e2_node);

}

void free_map_e2_node_sad(map_e2_node_sockaddr_t* m)
{
  assert(m != NULL);

  int rc = pthread_mutex_destroy(&m->mtx);
  assert(rc == 0);

  bi_map_free(&m->map);
}

void add_map_e2_node_sad(map_e2_node_sockaddr_t* m, global_e2_node_id_t const* id, sctp_info_t const* s)
{
  assert(m != NULL);
  assert(id != NULL);
  assert(s != NULL);

  lock_guard(&m->mtx);

  // If this DU is already registered (e.g., reconnecting after a crash), remove the stale
  // entry first. In Release mode the #if DEBUG duplicate check is disabled, so without
  // this guard bi_map_insert would create a second entry for the same nb_id — subsequent
  // find_map_e2_node_sad calls would then hit the old/invalid sctp_info.
  assoc_rb_tree_t* left = &m->map.left;
  void* it  = assoc_front(left);
  void* end = assoc_end(left);
  it = find_if(left, it, end, (global_e2_node_id_t*)id, eq_global_e2_node_id_wrapper);
  if (it != end) {
    printf("[NEAR-RIC]: E2 Node nb_id=%u already registered (reconnect), replacing sctp_info\n",
           id->nb_id.nb_id);
    // Use the stored key pointer for exact extraction (avoids traversal ambiguity).
    global_e2_node_id_t* stored_key = (global_e2_node_id_t*)assoc_key(left, it);
    sctp_info_t* old_s = bi_map_extract_left(&m->map, stored_key, sizeof(global_e2_node_id_t),
                                              free_global_e2_node_id_wrapper);
    free(old_s);
  }

  // Need a copy as global_e2_node_id_t may have allocated memory in the heap that will be freed by caller
  global_e2_node_id_t id_cp = cp_global_e2_node_id(id);
  bi_map_insert(&m->map, &id_cp, sizeof(global_e2_node_id_t), s, sizeof(sctp_info_t));
}

sctp_info_t* rm_map_e2_node_sad(map_e2_node_sockaddr_t* m, global_e2_node_id_t* id)
{
  assert(m != NULL);
  assert(id != NULL);

  lock_guard(&m->mtx);

  sctp_info_t* s = bi_map_extract_left(&m->map, id, sizeof(global_e2_node_id_t), free_global_e2_node_id_wrapper);
  return s;

//  sctp_info_t* s = assoc_extract(&m->tree, id);
//  free(s);
}

global_e2_node_id_t* rm_map_sad_e2_node(map_e2_node_sockaddr_t* m, sctp_info_t const* s)
{
  assert(m != NULL);
  assert(s != NULL);

  lock_guard(&m->mtx);

  // Check existence first — in Release mode asserts in assoc_rb_tree_extract are disabled,
  // so extracting a missing key would silently corrupt the RB-tree.
  assoc_rb_tree_t* right = &m->map.right;
  void* it  = assoc_front(right);
  void* end = assoc_end(right);
  it = find_if(right, it, end, (sctp_info_t*)s, eq_sctp_info_wrapper);
  if (it == end) {
    printf("[NEAR-RIC]: WARNING: SCTP shutdown addr (port=%u) not found in ep->e2_nodes, skipping removal\n",
           (unsigned)ntohs(s->addr.sin_port));
    return NULL;
  }

  void (*free_fn)(void*) = NULL;
  global_e2_node_id_t* id = bi_map_extract_right(&m->map, (sctp_info_t*) s, sizeof(sctp_info_t), free_fn);
  return id;
}

static
bool eq_assoc_id_wrapper(void const* sctp_info_v, void const* assoc_id_v)
{
  sctp_info_t const* s = (sctp_info_t const*)sctp_info_v;
  sctp_assoc_t const* assoc_id = (sctp_assoc_t const*)assoc_id_v;
  return s->sri.sinfo_assoc_id == *assoc_id;
}

// Remove by SCTP association ID — more reliable than IP:port for shutdown notifications.
global_e2_node_id_t* rm_map_sad_e2_node_by_assoc(map_e2_node_sockaddr_t* m, sctp_assoc_t assoc_id)
{
  assert(m != NULL);

  lock_guard(&m->mtx);

  assoc_rb_tree_t* right = &m->map.right;
  void* it  = assoc_front(right);
  void* end = assoc_end(right);
  it = find_if(right, it, end, &assoc_id, eq_assoc_id_wrapper);
  if (it == end) {
    printf("[NEAR-RIC]: WARNING: SCTP assoc_id=%u not found in ep->e2_nodes during shutdown\n",
           (unsigned)assoc_id);
    return NULL;
  }

  // Get the stored sctp_info_t key pointer from the right-tree iterator,
  // then pass it as the extraction key (avoids relying on unreliable IP:port).
  sctp_info_t* s = (sctp_info_t*)assoc_key(right, it);

  void (*free_fn)(void*) = NULL;
  global_e2_node_id_t* id = bi_map_extract_right(&m->map, s, sizeof(sctp_info_t), free_fn);
  return id;
}

sctp_info_t find_map_e2_node_sad(map_e2_node_sockaddr_t* m, global_e2_node_id_t const* id)
{
  assert(m != NULL);
  assert(id != NULL);

  lock_guard(&m->mtx);

  assoc_rb_tree_t* tree = &m->map.left;  

  void* it = assoc_front(tree);
  void* end = assoc_end(tree);

  it = find_if(tree, it, end, (global_e2_node_id_t*)id, eq_global_e2_node_id_wrapper);
  if (it == end) {
    printf("[NEAR-RIC]: WARNING: E2 Node nb_id=%u not found in ep->e2_nodes (already removed?), dropping send\n",
           id->nb_id.nb_id);
    sctp_info_t zero = {0};
    return zero;
  }

  sctp_info_t* s = assoc_value(tree, it);

  //printf("[NEAR-RIC]: nb_id %d port = %d  \n", id->nb_id.nb_id, s->addr.sin_port);

  return *s;
}

