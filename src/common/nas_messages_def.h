/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the Apache License, Version 2.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
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

//WARNING: Do not include this header directly. Use intertask_interface.h instead.

/*! \file nas_messages_def.h
  \brief
  \author Sebastien ROUX, Lionel Gauthier
  \company Eurecom
  \email: lionel.gauthier@eurecom.fr
*/

MESSAGE_DEF(NAS_PDN_CONNECTIVITY_REQ,           MESSAGE_PRIORITY_MED,   itti_nas_pdn_connectivity_req_t, nas_pdn_connectivity_req)
MESSAGE_DEF(NAS_INITIAL_UE_MESSAGE,             MESSAGE_PRIORITY_MED,   itti_nas_initial_ue_message_t,   nas_initial_ue_message)
MESSAGE_DEF(NAS_CONNECTION_ESTABLISHMENT_CNF,   MESSAGE_PRIORITY_MED,   itti_nas_conn_est_cnf_t,         nas_conn_est_cnf)
MESSAGE_DEF(NAS_CONNECTION_RELEASE_IND,         MESSAGE_PRIORITY_MED,   itti_nas_conn_rel_ind_t,         nas_conn_rel_ind)
MESSAGE_DEF(NAS_UPLINK_DATA_IND,                MESSAGE_PRIORITY_MED,   itti_nas_ul_data_ind_t,          nas_ul_data_ind)
MESSAGE_DEF(NAS_DOWNLINK_DATA_REQ,              MESSAGE_PRIORITY_MED,   itti_nas_dl_data_req_t,          nas_dl_data_req)
MESSAGE_DEF(NAS_DOWNLINK_DATA_CNF,              MESSAGE_PRIORITY_MED,   itti_nas_dl_data_cnf_t,          nas_dl_data_cnf)
MESSAGE_DEF(NAS_DOWNLINK_DATA_REJ,              MESSAGE_PRIORITY_MED,   itti_nas_dl_data_rej_t,          nas_dl_data_rej)
MESSAGE_DEF(NAS_ERAB_SETUP_REQ,                 MESSAGE_PRIORITY_MED,   itti_nas_erab_setup_req_t,       nas_erab_setup_req)
MESSAGE_DEF(NAS_ERAB_RELEASE_REQ,               MESSAGE_PRIORITY_MED,   itti_nas_erab_release_req_t,     nas_erab_release_req)

/* NAS layer -> MME app messages */
MESSAGE_DEF(NAS_AUTHENTICATION_PARAM_REQ,       MESSAGE_PRIORITY_MED,   itti_nas_auth_param_req_t,       nas_auth_param_req)
MESSAGE_DEF(NAS_DETACH_REQ,       		MESSAGE_PRIORITY_MED,   itti_nas_detach_req_t,           	nas_detach_req)
MESSAGE_DEF(NAS_PDN_CONFIG_REQ,                 MESSAGE_PRIORITY_MED,   itti_nas_pdn_config_req_t,       nas_pdn_config_req)

/* MME app -> NAS layer messages */
MESSAGE_DEF(NAS_PDN_CONNECTIVITY_RSP,           MESSAGE_PRIORITY_MED,   itti_nas_pdn_connectivity_rsp_t,  nas_pdn_connectivity_rsp)
MESSAGE_DEF(NAS_PDN_CONNECTIVITY_FAIL,          MESSAGE_PRIORITY_MED,   itti_nas_pdn_connectivity_fail_t, nas_pdn_connectivity_fail)
MESSAGE_DEF(NAS_PDN_CONFIG_RSP,                 MESSAGE_PRIORITY_MED,   itti_nas_pdn_config_rsp_t,       nas_pdn_config_rsp)
MESSAGE_DEF(NAS_PDN_CONFIG_FAIL,                MESSAGE_PRIORITY_MED,   itti_nas_pdn_config_fail_t,      nas_pdn_config_fail)
MESSAGE_DEF(NAS_SIGNALLING_CONNECTION_REL_IND,  MESSAGE_PRIORITY_MED,   itti_nas_signalling_connection_rel_ind_t, nas_signalling_connection_rel_ind)

/** todo: for multi-pdn. */
MESSAGE_DEF(NAS_PDN_DISCONNECT_REQ,             MESSAGE_PRIORITY_MED,   itti_nas_pdn_disconnect_req_t,   nas_pdn_disconnect_req)
MESSAGE_DEF(NAS_PDN_DISCONNECT_RSP,             MESSAGE_PRIORITY_MED,   itti_nas_pdn_disconnect_rsp_t,   nas_pdn_disconnect_rsp)


/** S10 Context Transfer (TAU). */
MESSAGE_DEF(NAS_CONTEXT_REQ,                    MESSAGE_PRIORITY_MED,   itti_nas_context_req_t,         nas_context_req)
MESSAGE_DEF(NAS_CONTEXT_RES,                    MESSAGE_PRIORITY_MED,   itti_nas_context_res_t,         nas_context_res)
MESSAGE_DEF(NAS_CONTEXT_FAIL,                   MESSAGE_PRIORITY_MED,   itti_nas_context_fail_t,        nas_context_fail)

MESSAGE_DEF(NAS_IMPLICIT_DETACH_UE_IND,         MESSAGE_PRIORITY_MED,   itti_nas_implicit_detach_ue_ind_t, nas_implicit_detach_ue_ind)


