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


#include "endpoint_iapp.h"
#include <arpa/inet.h>   // for inet_pton
#include <assert.h>      // for assert
#include <errno.h>       // for errno
#include <pthread.h>     // for pthread_mutex_lock/unlock
//#include <linux/sctp.h>  // for sctp_event_subscribe, SCTP_AUTOCLOSE, SCTP_E...
#include <netinet/sctp.h>
#include <netinet/in.h>  // for sockaddr_in, IPPROTO_SCTP, htons, sockaddr_in6
#include <stdio.h>       // for printf, NULL, size_t
#include <stdlib.h>      // for malloc
#include <string.h>      // for memset, strlen, strncpy
#include <strings.h>     // for bzero
#include <sys/socket.h>  // for setsockopt, AF_INET, bind, listen, socket

static
const int SERVER_LISTEN_QUEUE_SIZE = 32;

static
int init_sctp_conn_server(const char* addr, int port)
{
  errno = 0;
  struct sockaddr_in server4_addr = {.sin_family = AF_INET,
                                     .sin_port = htons(port)};

  if(inet_pton(AF_INET, addr, &server4_addr.sin_addr) != 1){
    // Error occurred
    struct sockaddr_in6 server6_addr;
    memset(&server6_addr, 0, sizeof(struct sockaddr_in6));
    if(inet_pton(AF_INET6, addr, &server6_addr.sin6_addr) == 1){
      assert(0!=0 && "IPv6 not supported");
    }
    assert(0!=0 && "Incorrect IP address string.");
  }

  const int server_fd = socket (AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  assert(server_fd != -1);

  const size_t addr_len = sizeof(server4_addr);
  struct sockaddr* server_addr = (struct sockaddr*)&server4_addr;
  int rc = bind(server_fd, server_addr, addr_len);
  if(rc == -1){
    printf("errno = %d\n", errno);
  }
  assert(rc != -1);

  struct sctp_event_subscribe evnts = {.sctp_data_io_event = 1, 
                                       /*.sctp_shutdown_event = 1,*/ 
                                       .sctp_send_failure_event = 1
                                       /*.sctp_association_event = 1*/
                                       /*.sctp_peer_error_event = 1,
                                       */ };

  rc = setsockopt (server_fd, IPPROTO_SCTP, SCTP_EVENTS, &evnts, sizeof (evnts));
  assert(rc != -1);

  const int close_time = 0; // No automatic close https://www.rfc-editor.org/rfc/pdfrfc/rfc6458.txt.pdf p. 65
  setsockopt(server_fd, IPPROTO_SCTP, SCTP_AUTOCLOSE, &close_time, sizeof(close_time));

  const int no_delay = 1;
  setsockopt(server_fd, IPPROTO_SCTP, SCTP_NODELAY, &no_delay, sizeof(no_delay));

  rc = listen(server_fd, SERVER_LISTEN_QUEUE_SIZE);
  assert(rc != -1);
  assert(errno == 0);
  return server_fd;
}

void e2ap_init_ep_iapp(e2ap_ep_iapp_t* ep, const char* addr, int port)
{
  assert(ep != NULL);
  assert(addr != NULL);
  assert(strlen(addr) < 16);
  assert(port > 0 && port < 65535);

  e2ap_ep_init(&ep->base); 

  *(int*)(&ep->base.fd) = init_sctp_conn_server(addr, port);
  *(int*)(&ep->base.port) = port;
  strncpy((char*)(&ep->base.addr), addr, 16);

  init_map_xapps_sad(&ep->xapps);
}

sctp_msg_t e2ap_recv_msg_iapp(e2ap_ep_iapp_t* ep)
{
  assert(ep != NULL);

  sctp_msg_t rcv = e2ap_recv_sctp_msg(&ep->base);

  return rcv;
}

// Send a message to a specific xApp identified by sinfo_assoc_id.
// When assoc_id != 0, the kernel routes by association (ignores msg_name),
// so all xApps sharing the same source IP are correctly distinguished.
// When assoc_id == 0, falls back to msg_name address routing (xApp-to-RIC direction).
static void send_iapp_by_assoc(int fd, pthread_mutex_t* mtx,
                                byte_array_t ba,
                                struct sctp_sndrcvinfo const* sri)
{
  struct iovec iov = {.iov_base = (void*)ba.buf, .iov_len = ba.len};
  char cbuf[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];
  memset(cbuf, 0, sizeof(cbuf));
  struct msghdr mhdr = {0};
  mhdr.msg_iov        = &iov;
  mhdr.msg_iovlen     = 1;
  // Do NOT set msg_name when assoc_id is known: setting msg_name with port=0
  // alongside a valid assoc_id can trigger EINVAL on Linux ≥ 5.x SCTP.
  mhdr.msg_name       = NULL;
  mhdr.msg_namelen    = 0;
  mhdr.msg_control    = cbuf;
  mhdr.msg_controllen = sizeof(cbuf);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mhdr);
  cmsg->cmsg_level = IPPROTO_SCTP;
  cmsg->cmsg_type  = SCTP_SNDRCV;
  cmsg->cmsg_len   = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
  struct sctp_sndrcvinfo* sndrcv = (struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
  sndrcv->sinfo_stream   = sri->sinfo_stream;
  sndrcv->sinfo_flags    = sri->sinfo_flags;
  sndrcv->sinfo_ppid     = sri->sinfo_ppid;
  sndrcv->sinfo_assoc_id = sri->sinfo_assoc_id;

  pthread_mutex_lock(mtx);
  ssize_t const rc = sendmsg(fd, &mhdr, 0);
  pthread_mutex_unlock(mtx);
  if (rc == -1) {
    printf("[iApp]: sendmsg failed (assoc_id=%d errno=%d)\n",
           sri->sinfo_assoc_id, errno);
  }
}

void e2ap_send_bytes_iapp(const e2ap_ep_iapp_t* ep, int xapp_id, byte_array_t ba)
{
  assert(ba.buf && ba.len > 0);
  assert(ep != NULL);

  sctp_msg_t msg = {.ba = ba};
  msg.info = find_map_xapps_sad((map_xapps_sockaddr_t*)&ep->xapps, xapp_id);

  // Use assoc_id routing so MAC indications reach the correct xApp even when
  // multiple xApps share the same source IP (macvlan-br).
  send_iapp_by_assoc(ep->base.fd, &((e2ap_ep_iapp_t*)ep)->base.mtx,
                     ba, &msg.info.sri);
}


void e2ap_send_sctp_msg_iapp(const e2ap_ep_iapp_t* ep, sctp_msg_t* msg)
{
  assert(ep != NULL);
  assert(msg->ba.buf && msg->ba.len > 0);

  send_iapp_by_assoc(ep->base.fd, &((e2ap_ep_iapp_t*)ep)->base.mtx,
                     msg->ba, &msg->info.sri);
}


void e2ap_free_ep_iapp(e2ap_ep_iapp_t* ep)
{
  assert(ep != NULL);

  e2ap_ep_free(&ep->base);
  free_map_xapps_sad(&ep->xapps);
}

void e2ap_reg_sock_addr_iapp(e2ap_ep_iapp_t* ep, uint16_t xapp_id,   sctp_info_t* s)
{
  assert(ep != NULL);
  assert(s != NULL);

  add_map_xapps_sad(&ep->xapps, xapp_id, s);
}

