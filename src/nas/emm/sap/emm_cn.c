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

/*****************************************************************************

  Source      emm_cn.c

  Version     0.1

  Date        2013/12/05

  Product     NAS stack

  Subsystem   EPS Core Network

  Author      Sebastien Roux, Lionel GAUTHIER

  Description

*****************************************************************************/

#include <pthread.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "bstrlib.h"

#include "log.h"
#include "msc.h"
#include "gcc_diag.h"
#include "dynamic_memory_check.h"
#include "assertions.h"
#include "commonDef.h"
#include "common_types.h"
#include "common_defs.h"
#include "3gpp_24.007.h"
#include "3gpp_24.008.h"
#include "3gpp_29.274.h"
#include "mme_app_ue_context.h"
#include "emm_cn.h"
#include "emm_sap.h"
#include "emm_proc.h"
#include "emm_cause.h"

#include "esm_send.h"
#include "esm_proc.h"
#include "esm_cause.h"
#include "assertions.h"
#include "emm_data.h"
#include "esm_sap.h"
#include "3gpp_requirements_24.301.h"
#include "mme_app_defs.h"
#include "mme_app_apn_selection.h"
#include "nas_itti_messaging.h"

extern int emm_cn_wrapper_attach_accept (emm_data_context_t * emm_context);
extern int emm_cn_wrapper_tracking_area_update_accept (emm_data_context_t * emm_context);

static int _emm_cn_authentication_res (emm_cn_auth_res_t * const msg);
static int _emm_cn_authentication_fail (const emm_cn_auth_fail_t * msg);
static int _emm_cn_deregister_ue (const mme_ue_s1ap_id_t ue_id);
static int _emm_cn_pdn_config_res (emm_cn_pdn_config_res_t * msg_pP);
static int _emm_cn_pdn_config_fail (const emm_cn_pdn_config_fail_t * msg_pP);
static int _emm_cn_pdn_connectivity_res (emm_cn_pdn_res_t * msg_pP);
static int _emm_cn_context_res (const emm_cn_context_res_t * const msg);
static int _emm_cn_context_fail (const emm_cn_context_fail_t * msg);
/*
   String representation of EMMCN-SAP primitives
*/
static const char                      *_emm_cn_primitive_str[] = {
  "EMM_CN_AUTHENTICATION_PARAM_RES",
  "EMM_CN_AUTHENTICATION_PARAM_FAIL",
  "EMMCN_CONTEXT_RES",
  "EMMCN_CONTEXT_FAIL",
  "EMM_CN_DEREGISTER_UE",
  "EMM_CN_PDN_CONFIG_RES",
  "EMM_CN_PDN_CONFIG_FAIL",
  "EMM_CN_PDN_CONNECTIVITY_RES",
  "EMM_CN_PDN_CONNECTIVITY_FAIL",
  "EMM_CN_PDN_DISCONNECT_RES",
  "EMM_CN_ACTIVATE_DEDICATED_BEARER_REQ",
  "EMM_CN_DEACTIVATE_DEDICATED_BEARER_REQ",
  "EMMCN_IMPLICIT_DETACH_UE",
  "EMMCN_SMC_PROC_FAIL",
};


//------------------------------------------------------------------------------
static int _emm_cn_authentication_res (emm_cn_auth_res_t * const msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  emm_data_context_t                          *emm_context = NULL;
  int                                     rc = RETURNerror;

  /*
   * We received security vector from HSS. Try to setup security with UE
   */
  emm_context = emm_data_context_get(&_emm_data, msg->ue_id);

  if (emm_context) {
    nas_auth_info_proc_t * auth_info_proc = get_nas_cn_procedure_auth_info(emm_context);

    if (auth_info_proc) {
      for (int i = 0; i < msg->nb_vectors; i++) {
        auth_info_proc->vector[i] = msg->vector[i];
        msg->vector[i] = NULL;
      }
      auth_info_proc->nb_vectors = msg->nb_vectors;
      rc = (*auth_info_proc->success_notif)(emm_context);
    } else {
      OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find Auth_info procedure associated to UE id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    }
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_authentication_fail (const emm_cn_auth_fail_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  emm_data_context_t                     *emm_context = NULL;
  int                                     rc = RETURNerror;

  /*
   * We received security vector from HSS. Try to setup security with UE
   */
  emm_context = emm_data_context_get(&_emm_data, msg->ue_id);

  nas_auth_info_proc_t * auth_info_proc = get_nas_cn_procedure_auth_info(emm_context);

  if (auth_info_proc) {
    auth_info_proc->nas_cause = msg->cause;
    rc = (*auth_info_proc->failure_notif)(emm_context);
  } else {
    OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find Auth_info procedure associated to UE id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_smc_fail (const emm_cn_smc_fail_t * msg)
{
  int                                     rc = RETURNerror;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  rc = emm_proc_attach_reject (msg->ue_id, msg->emm_cause);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_deregister_ue (const mme_ue_s1ap_id_t ue_id)
{
  int                                     rc = RETURNok;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - " "TODO deregister UE " MME_UE_S1AP_ID_FMT ", following procedure is a test\n", ue_id);
  emm_detach_request_ies_t * params = calloc(1, sizeof(*params));
  params->type         = EMM_DETACH_TYPE_EPS;
  params->switch_off   = false;
  params->is_native_sc = false;
  params->ksi          = 0;
  emm_proc_detach_request (ue_id, params);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_implicit_detach_ue (const uint32_t ue_id, const emm_proc_detach_type_t detach_type, const int emm_cause)
{
  int                                     rc = RETURNok;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_DEBUG (LOG_NAS_EMM, "EMM-PROC Implicit Detach udId " MME_UE_S1AP_ID_FMT "\n", ue_id);
  emm_detach_request_ies_t  params = {0};
//  //params.decode_status
//  //params.guti = NULL;
//  //params.imei = NULL;
//  //params.imsi = NULL;
//  params.is_native_sc = true;
//  params.ksi = 0;
//  params.switch_off = true;
//  params.type = EMM_DETACH_TYPE_EPS;

  /** No detach procedure for implicit detach. Set the UE state into EMM-DEREGISTER-INITIATED state. */
  emm_proc_detach(ue_id, detach_type, emm_cause);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_pdn_config_res (emm_cn_pdn_config_res_t * msg_pP)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;
  struct emm_data_context_s              *emm_context = NULL;
  ue_context_t                           *ue_context;
  pdn_context_t                          *pdn_context = NULL;
  bool                                    is_pdn_connectivity = false;
  esm_sap_t                               esm_sap = {0};
  pdn_cid_t                               pdn_cid = 0;
  ebi_t                                   default_ebi = 0;

  ue_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, msg_pP->ue_id);
  emm_context = emm_data_context_get(&_emm_data, msg_pP->ue_id);

  if (emm_context == NULL || ue_context == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg_pP->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  /*
   * Get the APN from the UE requested data or the first APN from handover
   * Todo: multiple APN handover!
   */
  bstring apn = NULL;
  if(emm_context->esm_ctx.esm_proc_data){ /**< In case of S10 TAU the PDN Contexts should already be established. */
    apn = emm_context->esm_ctx.esm_proc_data->apn;
    if(!apn){
      OAILOG_INFO(LOG_NAS_EMM, "EMMCN-SAP  - " "No APN set in the ESM proc data for UE Id " MME_UE_S1AP_ID_FMT ". Taking default APN. ...\n", msg_pP->ue_id);
      /** Check if any PDN contexts exists (handover/idle tau). */
      if(RB_EMPTY(&ue_context->pdn_contexts)){
        /** Neither a PDN context exists nor an APN is set. */
        apn_configuration_t *default_apn_config = &ue_context->apn_config_profile.apn_configuration[ue_context->apn_config_profile.context_identifier];
        apn = blk2bstr(default_apn_config->service_selection, default_apn_config->service_selection_length);
      }
    }
  }else{
    /** Check if any PDN contexts exists (handover/idle tau). */
    if(RB_EMPTY(&ue_context->pdn_contexts)){
      /** Neither a PDN context exists nor an APN is set. */
      apn_configuration_t *default_apn_config = &ue_context->apn_config_profile.apn_configuration[ue_context->apn_config_profile.context_identifier];
      apn = blk2bstr(default_apn_config->service_selection, default_apn_config->service_selection_length);
    }
  }

  /** Inform the ESM about the PDN Config Response. */
  esm_sap.primitive = ESM_PDN_CONFIG_RES;
  esm_sap.is_standalone = false;
  esm_sap.ue_id = emm_context->ue_id;
  esm_sap.ctx = emm_context;
  esm_sap.recv = NULL;
  esm_sap.data.pdn_config_res.pdn_cid             = &pdn_cid;     /**< Only used for initial attach and initial TAU. */
  esm_sap.data.pdn_config_res.default_ebi         = &default_ebi; /**< Only used for initial attach and initial TAU. */
  esm_sap.data.pdn_config_res.is_pdn_connectivity = &is_pdn_connectivity; /**< Default Bearer Id of default APN. */
  esm_sap.data.pdn_config_res.imsi                = msg_pP->imsi64; /**< Context Identifier of default APN. */
  esm_sap.data.pdn_config_res.apn                 = apn; /**< Context Identifier of default APN. Will be NULL in case of multi APN or none given. */

  rc = esm_sap_send(&esm_sap);

  if(rc != RETURNok){
    // todo: checking if TAU_ACCEPT/TAU_REJECT is already sent or not..
    OAILOG_INFO(LOG_NAS_EMM, "EMMCN-SAP  - " "Error processing PDN Config response for UE Id " MME_UE_S1AP_ID_FMT "...\n", msg_pP->ue_id);
    /** Error processing PDN Config Response. */
    emm_cn_t emm_cn = {0};
    emm_cn_pdn_config_fail_t emm_cn_pdn_config_fail = {0};
    emm_cn.u.emm_cn_pdn_config_fail = &emm_cn_pdn_config_fail;
    emm_cn.primitive = EMMCN_PDN_CONFIG_FAIL;
    emm_cn.u.emm_cn_pdn_config_fail->ue_id = msg_pP->ue_id;
    rc = emm_cn_send(&emm_cn);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
  }

  /** PDN Context and Bearer Contexts (default) are created with this. */

  if (!is_pdn_connectivity) { /**< We may have just created  a PDN context, but no connectivity yet. Assume pdn_connectivity is established when a PDN_Context already existed. */
    nas_itti_pdn_connectivity_req (emm_context->esm_ctx.esm_proc_data->pti, msg_pP->ue_id, pdn_cid, default_ebi, emm_context->_imsi64, &emm_context->_imsi,
        emm_context->esm_ctx.esm_proc_data, emm_context->esm_ctx.esm_proc_data->request_type);
  } else {
    /*
     * We already have an ESM PDN connectivity (due handover).
     * Like it is the case in PDN_CONNECTIVITY_RES, check the status and respond.
     */
    if (is_nas_specific_procedure_tau_running(emm_context)){


      OAILOG_INFO(LOG_NAS_EMM, "EMMCN-SAP  - " "Tracking Area Update Procedure is running. PDN Connectivity is already established with ULA. Continuing with the accept procedure for id " MME_UE_S1AP_ID_FMT "...\n", msg_pP->ue_id);
      // todo: checking if TAU_ACCEPT/TAU_REJECT is already sent or not..
      /*
       * Send tracking area update accept message to the UE
       */
      rc = emm_cn_wrapper_tracking_area_update_accept(emm_context);

      /** We will set the UE into COMMON-PROCEDURE-INITIATED state inside this method. */
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
    }else{
      OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "TAU procedure is not running for UE associated to id " MME_UE_S1AP_ID_FMT ". ULA received and PDN Connectivity is there. Performing an implicit detach..\n", msg_pP->ue_id);
//     todo: better way to handle this? _emm_cn_implicit_detach_ue(msg_pP->ue_id);
      // todo: rejecting any specific procedures?
      OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
    }
  }
  // todo: unlock UE contexts
//    unlock_ue_contexts(ue_context);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNok);
}

//------------------------------------------------------------------------------
static int _emm_cn_pdn_config_fail (const emm_cn_pdn_config_fail_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;
  struct emm_data_context_s              *emm_ctx_p = NULL;
  ESM_msg                                 esm_msg = {.header = {0}};
  int                                     esm_cause;
  emm_ctx_p = emm_data_context_get (&_emm_data, msg->ue_id);
  if (emm_ctx_p == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }
  memset (&esm_msg, 0, sizeof (ESM_msg));

  // Map S11 cause to ESM cause
  esm_cause = ESM_CAUSE_NETWORK_FAILURE; // todo: error for SAEGW error and HSS ULA error?

  if(is_nas_specific_procedure_attach_running(emm_ctx_p)){
    if(emm_ctx_p->esm_ctx.esm_proc_data){
      rc = esm_send_pdn_connectivity_reject (emm_ctx_p->esm_ctx.esm_proc_data->pti, &esm_msg.pdn_connectivity_reject, esm_cause);
      /*
       * Encode the returned ESM response message
       */
      uint8_t                             emm_cn_sap_buffer[EMM_CN_SAP_BUFFER_SIZE];
      int size = esm_msg_encode (&esm_msg, emm_cn_sap_buffer, EMM_CN_SAP_BUFFER_SIZE);
      OAILOG_INFO (LOG_NAS_EMM, "ESM encoded MSG size %d\n", size);

      if (size > 0) {
        nas_emm_attach_proc_t  *attach_proc = get_nas_specific_procedure_attach(emm_ctx_p);
        /*
         * Setup the ESM message container
         */
        attach_proc->esm_msg_out = blk2bstr(emm_cn_sap_buffer, size);
        rc = emm_proc_attach_reject (msg->ue_id, EMM_CAUSE_ESM_FAILURE);
       }else{
         // todo: wtf
         DevAssert(0);
       }
       emm_context_unlock(emm_ctx_p);
       OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);

    }
  }else if (is_nas_specific_procedure_tau_running(emm_ctx_p)){
      /** Trigger a TAU Reject. */
    nas_emm_tau_proc_t  *tau_proc = get_nas_specific_procedure_tau(emm_ctx_p);

//    emm_sap_t                               emm_sap = {0};
//
//    memset(&emm_sap, 0, sizeof(emm_sap_t));
//
//    emm_sap.primitive = EMMREG_TAU_REJ;
//    emm_sap.u.emm_reg.ue_id = emm_ctx_p->ue_id;
//    emm_sap.u.emm_reg.ctx  = emm_ctx_p;
//    emm_sap.u.emm_reg.u.tau.proc = tau_proc;
//    emm_sap.u.emm_reg.notify= true;
//    emm_sap.u.emm_reg.free_proc = true;
//
//    MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMREG_COMMON_PROC_REQ ue id " MME_UE_S1AP_ID_FMT " ", emm_ctx_p->ue_id);
//    rc = emm_proc_attach_reject (msg->ue_id, EMM_CAUSE_ESM_FAILURE);

//    rc = emm_sap_send (&emm_sap);

    rc = emm_proc_tracking_area_update_reject(msg->ue_id, EMM_CAUSE_ESM_FAILURE);

    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
 }else{
    // tood: checking for ESM procedure or some other better way?
    DevAssert(0);
  }
  //  emm_context_unlock(emm_ctx_p);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_pdn_connectivity_res (emm_cn_pdn_res_t * msg_pP)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;
  struct emm_data_context_s              *emm_context = NULL;
  struct pdn_context_s                   *pdn_context = NULL;
  esm_proc_pdn_type_t                     esm_pdn_type = ESM_PDN_TYPE_IPV4;
  ESM_msg                                 esm_msg = {.header = {0}};
  EpsQualityOfService                     qos = {0};
  bstring                                 rsp = NULL;
  bool                                    is_standalone = false;    // warning hardcoded
  bool                                    triggered_by_ue = true;  // warning hardcoded

  ue_context_t *ue_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, msg_pP->ue_id);
  emm_context = emm_data_context_get(&_emm_data, msg_pP->ue_id);

  if (ue_context == NULL || emm_context == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg_pP->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  /** Get the PDN context (should exist right now). */
  /** Check if a tunnel already exists depending on the flag. */
//  pdn_context_t pdn_context_key = {.context_identifier = msg_pP->pdn_cid}; // todo: should be also retrievable from pdn_cid alone (or else get esm_proc_data --> apn stands there, too).
//    keyTunnel.ipv4AddrRemote = pUlpReq->u_api_info.initialReqInfo.peerIp;
  mme_app_get_pdn_context(ue_context, msg_pP->pdn_cid, msg_pP->ebi, NULL, &pdn_context);
  nas_emm_attach_proc_t                  *attach_proc = get_nas_specific_procedure_attach(emm_context);

  if(!attach_proc){
    /** Check if it is a TAU procedure, else it is standalone. */
    if (is_nas_specific_procedure_tau_running(emm_context)){
      /** Continue with TAU Accept. */
      rc = emm_cn_wrapper_tracking_area_update_accept(emm_context);
      /** We will set the UE into COMMON-PROCEDURE-INITIATED state inside this method. */
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
    }
    OAILOG_INFO (LOG_NAS_EMM, "EMM  -  Neither TAU nor ATTACH procedure is running for ue " MME_UE_S1AP_ID_FMT ". Assuming standalone. \n", emm_context->ue_id);
    is_standalone = true;
  }

  DevAssert(pdn_context); // todo: here, we should trigger an attach_reject with PDN_Connectivity reject!

  memset (&esm_msg, 0, sizeof (ESM_msg));

  switch (msg_pP->pdn_type) {
  case IPv4:
    OAILOG_INFO (LOG_NAS_EMM, "EMM  -  esm_pdn_type = ESM_PDN_TYPE_IPV4\n");
    esm_pdn_type = ESM_PDN_TYPE_IPV4;
    break;

  case IPv6:
    OAILOG_INFO (LOG_NAS_EMM, "EMM  -  esm_pdn_type = ESM_PDN_TYPE_IPV6\n");
    esm_pdn_type = ESM_PDN_TYPE_IPV6;
    break;

  case IPv4_AND_v6:
    OAILOG_INFO (LOG_NAS_EMM, "EMM  -  esm_pdn_type = ESM_PDN_TYPE_IPV4V6\n");
    esm_pdn_type = ESM_PDN_TYPE_IPV4V6;
    break;

  default:
    OAILOG_INFO (LOG_NAS_EMM, "EMM  -  esm_pdn_type = ESM_PDN_TYPE_IPV4 (forced to default)\n");
    esm_pdn_type = ESM_PDN_TYPE_IPV4;
  }

  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qci       = %u \n", msg_pP->qci);
  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qos.qci   = %u \n", msg_pP->qos.qci);
  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qos.mbrUL = %u \n", msg_pP->qos.mbrUL);
  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qos.mbrDL = %u \n", msg_pP->qos.mbrDL);
  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qos.gbrUL = %u \n", msg_pP->qos.gbrUL);
  OAILOG_INFO (LOG_NAS_EMM, "EMM  -  qos.gbrDL = %u \n", msg_pP->qos.gbrDL);
  qos.bitRatesPresent = 0;
  qos.bitRatesExtPresent = 0;
//#pragma message "Some work to do here about qos"
  qos.qci = msg_pP->qci;
  qos.bitRates.maxBitRateForUL = 0;     //msg_pP->qos.mbrUL;
  qos.bitRates.maxBitRateForDL = 0;     //msg_pP->qos.mbrDL;
  qos.bitRates.guarBitRateForUL = 0;    //msg_pP->qos.gbrUL;
  qos.bitRates.guarBitRateForDL = 0;    //msg_pP->qos.gbrDL;
  qos.bitRatesExt.maxBitRateForUL = 0;
  qos.bitRatesExt.maxBitRateForDL = 0;
  qos.bitRatesExt.guarBitRateForUL = 0;
  qos.bitRatesExt.guarBitRateForDL = 0;

  /*
   * Return default EPS bearer context request message
   */
  rc = esm_send_activate_default_eps_bearer_context_request (msg_pP->pti, msg_pP->ebi,      //msg_pP->ebi,
                                                             &esm_msg.activate_default_eps_bearer_context_request,
                                                             pdn_context->apn_subscribed,
                                                             &msg_pP->pco,
                                                             esm_pdn_type, msg_pP->pdn_addr,
                                                             &qos, ESM_CAUSE_SUCCESS);
  clear_protocol_configuration_options(&msg_pP->pco); // todo: here or in the ITTI_FREE function
  if (rc != RETURNerror) {
    /*
     * Encode the returned ESM response message
     */
    char                                    emm_cn_sap_buffer[EMM_CN_SAP_BUFFER_SIZE];
    int                                     size = esm_msg_encode (&esm_msg, (uint8_t *) emm_cn_sap_buffer,
                                                                   EMM_CN_SAP_BUFFER_SIZE);

    OAILOG_INFO (LOG_NAS_EMM, "ESM encoded MSG size %d\n", size);

    if (size > 0) {
      rsp = blk2bstr(emm_cn_sap_buffer, size);
    }

    /*
     * Complete the relevant ESM procedure
     */
    rc = esm_proc_default_eps_bearer_context_request (is_standalone, emm_context, msg_pP->ebi,        //0, //ESM_EBI_UNASSIGNED, //msg->ebi,
                                                      &rsp, triggered_by_ue);

    if (rc != RETURNok) {
      /*
       * Return indication that ESM procedure failed
       */
//      unlock_ue_contexts(ue_context);
      OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
    }
  } else {
    OAILOG_INFO (LOG_NAS_EMM, "ESM send activate_default_eps_bearer_context_request failed\n");
  }

  /*************************************************************************/
  /*
   * END OF CODE THAT WAS IN esm_sap.c/_esm_sap_recv()
   */
  /*************************************************************************/
  if (attach_proc) {
    /*
     * Setup the ESM message container
     */
    attach_proc->esm_msg_out = rsp;

    /*
     * Send attach accept message to the UE
     */
    rc = emm_cn_wrapper_attach_accept (emm_context);

    if (rc != RETURNerror) {
      if (IS_EMM_CTXT_PRESENT_OLD_GUTI(emm_context) &&
          (memcmp(&emm_context->_old_guti, &emm_context->_guti, sizeof(emm_context->_guti)))) {
        /*
         * Implicit GUTI reallocation;
         * Notify EMM that common procedure has been initiated
         * LG: TODO check this, seems very suspicious
         */
        emm_sap_t                               emm_sap = {0};

        emm_sap.primitive = EMMREG_COMMON_PROC_REQ;
        emm_sap.u.emm_reg.ue_id = msg_pP->ue_id;
        emm_sap.u.emm_reg.ctx  = emm_context;

        MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMREG_COMMON_PROC_REQ ue id " MME_UE_S1AP_ID_FMT " ", msg_pP->ue_id);

        rc = emm_sap_send (&emm_sap);
      }
    }
  } else {
    OAILOG_ERROR(LOG_NAS_EMM, "This is an error, for no other procedure NAS layer should be informed about the PDN Connectivity. It happens before NAS for UE Id " MME_UE_S1AP_ID_FMT". \n", msg_pP->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
  }
//  unlock_ue_contexts(ue_context);

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_pdn_connectivity_fail (const emm_cn_pdn_fail_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;
  struct emm_data_context_s              *emm_ctx_p = NULL;
  ESM_msg                                 esm_msg = {.header = {0}};
  int                                     esm_cause; 
  emm_ctx_p = emm_data_context_get (&_emm_data, msg->ue_id);
  if (emm_ctx_p == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }
  memset (&esm_msg, 0, sizeof (ESM_msg));
  
  // Map S11 cause to ESM cause
  switch (msg->cause) {
    case CAUSE_CONTEXT_NOT_FOUND:
      esm_cause = ESM_CAUSE_REQUEST_REJECTED_BY_GW; 
      break;
    case CAUSE_INVALID_MESSAGE_FORMAT:
      esm_cause = ESM_CAUSE_REQUEST_REJECTED_BY_GW; 
      break;
    case CAUSE_SERVICE_NOT_SUPPORTED:                  
      esm_cause = ESM_CAUSE_SERVICE_OPTION_NOT_SUPPORTED;
      break;
    case CAUSE_SYSTEM_FAILURE:                        
      esm_cause = ESM_CAUSE_NETWORK_FAILURE; 
      break;
    case CAUSE_NO_RESOURCES_AVAILABLE:           
      esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES; 
      break;
    case CAUSE_ALL_DYNAMIC_ADDRESSES_OCCUPIED:  
      esm_cause = ESM_CAUSE_INSUFFICIENT_RESOURCES;
      break;
    default: 
      esm_cause = ESM_CAUSE_REQUEST_REJECTED_BY_GW; 
      break;
  }

  if(is_nas_specific_procedure_attach_running(emm_ctx_p)){
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Sending Attach/PDN Connectivity Reject message to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);

    rc = esm_send_pdn_connectivity_reject (msg->pti, &esm_msg.pdn_connectivity_reject, esm_cause);
    /*
     * Encode the returned ESM response message
     */
    uint8_t                             emm_cn_sap_buffer[EMM_CN_SAP_BUFFER_SIZE];
    int size = esm_msg_encode (&esm_msg, emm_cn_sap_buffer, EMM_CN_SAP_BUFFER_SIZE);
    OAILOG_INFO (LOG_NAS_EMM, "ESM encoded MSG size %d\n", size);

    if (size > 0) {
      nas_emm_attach_proc_t  *attach_proc = get_nas_specific_procedure_attach(emm_ctx_p);
      /*
       * Setup the ESM message container
       */
      if(attach_proc){
        /** Sending the PDN connection reject inside a Attach Reject to the UE. */
        attach_proc->esm_msg_out = blk2bstr(emm_cn_sap_buffer, size);
        rc = emm_proc_attach_reject (msg->ue_id, EMM_CAUSE_ESM_FAILURE);
      }else{
        // todo: send the pdn disconnect reject as a standalone message to the UE.
        // todo: must clean the created pdn_context elements (no bearers should exist).
      }
    }
  }else if (is_nas_specific_procedure_tau_running(emm_ctx_p)){
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Sending TAU Reject message to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);

    nas_emm_tau_proc_t  *tau_proc = get_nas_specific_procedure_tau(emm_ctx_p);
    rc = emm_proc_tracking_area_update_reject(msg->ue_id, EMM_CAUSE_ESM_FAILURE);
  }else{
    // todo: multi apn pdn connectivity failure case!
    DevAssert(0);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_pdn_disconnect_res(const emm_cn_pdn_disconnect_res_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;
  struct emm_data_context_s              *emm_context = NULL;
  ESM_msg                                 esm_msg = {.header = {0}};
  int                                     esm_cause;
  esm_sap_t                               esm_sap = {0};
  bool                                    pdn_local_delete = false;

  ue_context_t *ue_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, msg->ue_id);
  DevAssert(ue_context); // todo:

  emm_context = emm_data_context_get (&_emm_data, msg->ue_id);
  if (emm_context == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }
  memset (&esm_msg, 0, sizeof (ESM_msg));

//  /** Inform the ESM layer about the PDN session removal. */
//  esm_sap.primitive = ESM_DEFAULT_EPS_BEARER_CONTEXT_ACTIVATE_CNF;
//  esm_sap.is_standalone = false;
//  esm_sap.ue_id = ue_id;
//  esm_sap.recv = esm_msg_pP;
//  esm_sap.ctx = emm_data_context;

  // todo: check if detach_proc is active or UE is in DEREGISTRATION_INITIATED state, dn's

  /*
   * Check if there is a specific detach procedure running.
   * In that case, check if other PDN contexts exist. Delete those sessions, too.
   * Finally send Detach Accept back to the UE.
   */
  nas_emm_detach_proc_t  *detach_proc = get_nas_specific_procedure_detach(emm_context);
  // todo: implicit detach ? specific procedure?
  if(detach_proc || EMM_DEREGISTERED_INITIATED == emm_fsm_get_state(emm_context)){
    OAILOG_INFO (LOG_NAS_EMM, "Detach procedure ongoing for UE " MME_UE_S1AP_ID_FMT". Performing local PDN context deletion. \n", emm_context->ue_id);
    pdn_local_delete = true;
  }
  /** Check for the number of pdn connections left. */
  if(!emm_context->esm_ctx.n_pdns){
    OAILOG_INFO (LOG_NAS_EMM, "No more PDN contexts exist for UE " MME_UE_S1AP_ID_FMT". Continuing with detach procedure. \n", emm_context->ue_id);
    /** If a specific detach procedure is running, check if it is switch, off, if so, send detach accept back. */
    if(detach_proc){
      if (!detach_proc->ies->switch_off) {
        /*
         * Normal detach without UE switch-off
         */
        emm_sap_t                               emm_sap = {0};
        emm_as_data_t                          *emm_as = &emm_sap.u.emm_as.u.data;

        MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMM_AS_NAS_INFO_DETACH ue id " MME_UE_S1AP_ID_FMT " ", ue_id);
        /*
         * Setup NAS information message to transfer
         */
        emm_as->nas_info = EMM_AS_NAS_INFO_DETACH;
        emm_as->nas_msg = NULL;
        /*
         * Set the UE identifier
         */
        emm_as->ue_id = emm_context->ue_id;
        /*
         * Setup EPS NAS security data
         */
        emm_as_set_security_data (&emm_as->sctx, &emm_context->_security, false, true);
        /*
         * Notify EMM-AS SAP that Detach Accept message has to
         * be sent to the network
         */
        emm_sap.primitive = EMMAS_DATA_REQ;
        rc = emm_sap_send (&emm_sap);
      }
    }
    /** Detach the UE. */
    if (rc != RETURNerror) {

      mme_ue_s1ap_id_t old_ue_id = emm_context->ue_id;

      emm_sap_t                               emm_sap = {0};

      /*
       * Notify EMM FSM that the UE has been implicitly detached
       */
      MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMREG_DETACH_CNF ue id " MME_UE_S1AP_ID_FMT " ", ue_id);
      emm_sap.primitive = EMMREG_DETACH_CNF;
      emm_sap.u.emm_reg.ue_id = emm_context->ue_id;
      emm_sap.u.emm_reg.ctx = emm_context;
      emm_sap.u.emm_reg.free_proc = true;
      rc = emm_sap_send (&emm_sap);
      // Notify MME APP to remove the remaining MME_APP and S1AP contexts..
      nas_itti_detach_req(old_ue_id);
      // todo: review unlock
//      unlock_ue_contexts(ue_context);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);

    } else{
      // todo:
      DevAssert(0);
    }
  }else{
    OAILOG_INFO (LOG_NAS_EMM, "%d more PDN contexts exist for UE " MME_UE_S1AP_ID_FMT". Continuing with PDN Disconnect procedure. \n", emm_context->esm_ctx.n_pdns, emm_context->ue_id);
    /*
     * Send Disconnect Request to the ESM layer.
     * It will disconnect the first PDN which is pending deactivation.
     * This can be called at detach procedure with multiple PDNs, too.
     *
     * If detach procedure, or MME initiated PDN context deactivation procedure, we will first locally remove the PDN context.
     * The remaining PDN contexts will not be those who need NAS messaging to be purged, but the ones, who were not purged locally at all.
     */
    if(!pdn_local_delete){
      esm_sap.primitive = ESM_PDN_DISCONNECT_CNF;
      esm_sap.is_standalone = true;
      esm_sap.ue_id = emm_context->ue_id;
      esm_sap.ctx = emm_context;
      esm_sap.recv = NULL;
      esm_sap.data.pdn_disconnect.local_delete = pdn_local_delete; // pdn_context->default_ebi; /**< Default Bearer Id of default APN. */
      esm_sap_send(&esm_sap);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
    }else{
      OAILOG_INFO (LOG_NAS_ESM, "ue_id=" MME_UE_S1AP_ID_FMT " EMM-PROC  - Non Standalone & local detach triggered PDN deactivation. \n", emm_context->ue_id);
      esm_sap_t                               esm_sap = {0};
      /** Send Disconnect Request to all PDNs. */
      pdn_context_t * pdn_context = RB_MIN(PdnContexts, &ue_context->pdn_contexts);

      esm_sap.primitive = ESM_PDN_DISCONNECT_REQ;
      esm_sap.is_standalone = false;
      esm_sap.ue_id = emm_context->ue_id;
      esm_sap.ctx = emm_context;
      esm_sap.recv = emm_context->esm_msg;
      esm_sap.data.pdn_disconnect.default_ebi = pdn_context->default_ebi; /**< Default Bearer Id of default APN. */
      esm_sap.data.pdn_disconnect.cid         = pdn_context->context_identifier; /**< Context Identifier of default APN. */
      esm_sap.data.pdn_disconnect.local_delete = true;                            /**< Local deletion of PDN contexts. */
      esm_sap_send(&esm_sap);
      /*
       * Remove the contexts and send S11 Delete Session Request to the SAE-GW.
       * Will continue with the detach procedure when S11 Delete Session Response arrives.
       */

      /**
       * Not waiting for a response. Will assume that the session is correctly purged.. Continuing with the detach and assuming that the SAE-GW session is purged.
       * Assuming, that in 5G AMF/SMF structure, it is like in PCRF, sending DELETE_SESSION_REQUEST and not caring about the response. Assuming the session is deactivated.
       */
      OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNok);
    }
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_activate_dedicated_bearer_req (emm_cn_activate_dedicated_bearer_req_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;
  /** Like PDN Config Response, directly forwarded to ESM. */
  // forward to ESM
  esm_sap_t                               esm_sap = {0};

  emm_data_context_t *emm_context = emm_data_context_get( &_emm_data, msg->ue_id);

  esm_sap.primitive = ESM_DEDICATED_EPS_BEARER_CONTEXT_ACTIVATE_REQ;
  esm_sap.ctx           = emm_context;
  esm_sap.is_standalone = true;
  esm_sap.ue_id         = msg->ue_id;
  memcpy((void*)&esm_sap.data.eps_dedicated_bearer_context_activate, msg, sizeof(emm_cn_activate_dedicated_bearer_req_t)); // todo: pointer directly?

  MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_ESM_MME, NULL, 0, "0 ESM_DEDICATED_EPS_BEARER_CONTEXT_ACTIVATE_REQ ue id " MME_UE_S1AP_ID_FMT /*" ebi %u"*/,
      esm_sap.ue_id,/*esm_sap.data.eps_dedicated_bearer_context_activate.ebi*/);

  rc = esm_sap_send (&esm_sap);
  msg->bcs_to_be_created = NULL;

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_deactivate_dedicated_bearer_req (emm_cn_deactivate_dedicated_bearer_req_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;
  /** Like PDN Config Response, directly forwarded to ESM. */
  // forward to ESM
  esm_sap_t                               esm_sap = {0};

  emm_data_context_t *emm_context = emm_data_context_get( &_emm_data, msg->ue_id);

  esm_sap.primitive = ESM_DEDICATED_EPS_BEARER_CONTEXT_DEACTIVATE_REQ;
  esm_sap.ctx           = emm_context;
  esm_sap.is_standalone = true;
  esm_sap.ue_id         = msg->ue_id;
  memcpy((void*)&esm_sap.data.eps_dedicated_bearer_context_deactivate, msg, sizeof(emm_cn_deactivate_dedicated_bearer_req_t)); // todo: pointer directly?

  MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_ESM_MME, NULL, 0, "0 ESM_DEDICATED_EPS_BEARER_CONTEXT_DEACTIVATE_REQ ue id " MME_UE_S1AP_ID_FMT /*" ebi %u"*/,
      esm_sap.ue_id,/*esm_sap.data.eps_dedicated_bearer_context_activate.ebi*/);

  rc = esm_sap_send (&esm_sap);

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_context_res (const emm_cn_context_res_t * msg)
{
  emm_data_context_t                     *emm_context = NULL;
  int                                     rc = RETURNerror;
  /**
   * We received security vector from source MME.
   * Directly using received security parameters.
   * If we received already security parameters like UE network capability, ignoring the parameters received from the source MME and using the UE parameters.
   */
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  emm_context = emm_data_context_get (&_emm_data, msg->ue_id);
  if (emm_context == NULL) { /**< We assume that an MME_APP UE context also should not exist here. */
    OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  nas_ctx_req_proc_t * ctx_req_proc = get_nas_cn_procedure_ctx_req(emm_context);

  /** Update the context request procedure. */
  if (!ctx_req_proc) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find context request procedure associated to UE id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
  } else {
    // todo: PDN Connections IE from here or else?
    rc = (*ctx_req_proc->success_notif)(emm_context);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_cn_context_fail (const emm_cn_context_fail_t * msg)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  emm_data_context_t                     *emm_ctx = NULL;
  int                                     rc = RETURNerror;

  /**
   * An UE could or could not exist. We need to check. If it exists, it needs to be purged.
   * Since no UE context is established yet, we don't have security/no bearers.0
   * If the message is received after the timeout, the MME_APP context also should not exist.
   * If the MME_APP context existed at that point in time, it will later be removed.
   * Just discard the message then.
   */
  emm_ctx = emm_data_context_get (&_emm_data, msg->ue_id);
  if (emm_ctx == NULL) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find UE associated to id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
    /**
     * In this case, don't wait for the timer to remove the rest! Assume no timers exist.
     * Purge the rest of the UE context (MME_APP etc.).
     */
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  nas_ctx_req_proc_t * ctx_req_proc = get_nas_cn_procedure_ctx_req(emm_ctx);

  if (ctx_req_proc) {
    ctx_req_proc->nas_cause = msg->cause; // todo: better handling
    rc = (*ctx_req_proc->failure_notif)(emm_ctx);
    nas_delete_cn_procedure(emm_ctx, ctx_req_proc);

  } else {
    OAILOG_ERROR (LOG_NAS_EMM, "EMM-PROC  - " "Failed to find context request procedure associated to UE id " MME_UE_S1AP_ID_FMT "...\n", msg->ue_id);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNok);
}

//------------------------------------------------------------------------------
int emm_cn_send (const emm_cn_t * msg)
{
  int                                     rc = RETURNerror;
  emm_cn_primitive_t                      primitive = msg->primitive;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_INFO (LOG_NAS_EMM, "EMMCN-SAP - Received primitive %s (%d)\n", _emm_cn_primitive_str[primitive - _EMMCN_START - 1], primitive);

  switch (primitive) {
  case _EMMCN_AUTHENTICATION_PARAM_RES:
    rc = _emm_cn_authentication_res (msg->u.auth_res);
    break;

  case _EMMCN_AUTHENTICATION_PARAM_FAIL:
    rc = _emm_cn_authentication_fail (msg->u.auth_fail);
    break;

  case EMMCN_DEREGISTER_UE:
    rc = _emm_cn_deregister_ue (msg->u.deregister.ue_id);
    break;

  case EMMCN_PDN_CONFIG_RES:
    rc = _emm_cn_pdn_config_res (msg->u.emm_cn_pdn_config_res);
    break;

  case EMMCN_PDN_CONFIG_FAIL:
    rc = _emm_cn_pdn_config_fail (msg->u.emm_cn_pdn_config_fail);
    break;

  case EMMCN_PDN_CONNECTIVITY_RES:
    rc = _emm_cn_pdn_connectivity_res (msg->u.emm_cn_pdn_res);
    break;

  case EMMCN_PDN_CONNECTIVITY_FAIL:
    rc = _emm_cn_pdn_connectivity_fail (msg->u.emm_cn_pdn_fail);
    break;

  case EMMCN_PDN_DISCONNECT_RES:
    rc = _emm_cn_pdn_disconnect_res (msg->u.emm_cn_pdn_disconnect_res);
    break;
  
  case EMMCN_ACTIVATE_DEDICATED_BEARER_REQ:
    rc = _emm_cn_activate_dedicated_bearer_req (msg->u.activate_dedicated_bearer_req);
    break;

  case EMMCN_DEACTIVATE_DEDICATED_BEARER_REQ:
    rc = _emm_cn_deactivate_dedicated_bearer_req (msg->u.deactivate_dedicated_bearer_req);
    break;

  case EMMCN_IMPLICIT_DETACH_UE:
    rc = _emm_cn_implicit_detach_ue (msg->u.emm_cn_implicit_detach.ue_id, msg->u.emm_cn_implicit_detach.detach_type, msg->u.emm_cn_implicit_detach.emm_cause);
    break;

  case EMMCN_SMC_PROC_FAIL:
    rc = _emm_cn_smc_fail (msg->u.smc_fail);
    break;

    /** S10 Context Response information. */
  case EMMCN_CONTEXT_RES:
    rc = _emm_cn_context_res (msg->u.context_res);
    break;
  case EMMCN_CONTEXT_FAIL:
    rc = _emm_cn_context_fail (msg->u.context_fail);
    break;

  default:
    /*
     * Other primitives are forwarded to the Access Stratum
     */
    rc = RETURNerror;
    break;
  }

  if (rc != RETURNok) {
    OAILOG_ERROR (LOG_NAS_EMM, "EMMCN-SAP - Failed to process primitive %s (%d)\n", _emm_cn_primitive_str[primitive - _EMMCN_START - 1], primitive);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}
