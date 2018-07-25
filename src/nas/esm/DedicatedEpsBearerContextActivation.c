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
  Source      DedicatedEpsBearerContextActivation.c

  Version     0.1

  Date        2013/07/16

  Product     NAS stack

  Subsystem   EPS Session Management

  Author      Frederic Maurel

  Description Defines the dedicated EPS bearer context activation ESM
        procedure executed by the Non-Access Stratum.

        The purpose of the dedicated EPS bearer context activation
        procedure is to establish an EPS bearer context with specific
        QoS and TFT between the UE and the EPC.

        The procedure is initiated by the network, but may be requested
        by the UE by means of the UE requested bearer resource alloca-
        tion procedure or the UE requested bearer resource modification
        procedure.
        It can be part of the attach procedure or be initiated together
        with the default EPS bearer context activation procedure when
        the UE initiated stand-alone PDN connectivity procedure.

*****************************************************************************/
#include <pthread.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "bstrlib.h"

#include "log.h"
#include "dynamic_memory_check.h"
#include "common_types.h"
#include "assertions.h"
#include "3gpp_24.007.h"
#include "3gpp_24.008.h"
#include "3gpp_29.274.h"
#include "emm_data.h"
#include "mme_app_ue_context.h"
#include "esm_proc.h"
#include "commonDef.h"
#include "esm_cause.h"
#include "esm_ebr.h"
#include "esm_ebr_context.h"
#include "emm_sap.h"
#include "mme_config.h"
#include "nas_itti_messaging.h"
#include "mme_app_defs.h"

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
   Internal data handled by the dedicated EPS bearer context activation
   procedure in the MME
   --------------------------------------------------------------------------
*/
/*
   Timer handlers
*/
static void _dedicated_eps_bearer_activate_t3485_handler (void *);

/* Maximum value of the activate dedicated EPS bearer context request
   retransmission counter */
#define DEDICATED_EPS_BEARER_ACTIVATE_COUNTER_MAX   5

static int _dedicated_eps_bearer_activate (emm_data_context_t * emm_context, ebi_t ebi, STOLEN_REF bstring *msg);

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
      Dedicated EPS bearer context activation procedure executed by the MME
   --------------------------------------------------------------------------
*/
/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_dedicated_eps_bearer_context()                   **
 **                                                                        **
 ** Description: Allocates resources required for activation of a dedica-  **
 **      ted EPS bearer context.                                   **
 **                                                                        **
 ** Inputs:  ue_id:      UE local identifier                        **
 **          pid:       PDN connection identifier                  **
 **      esm_qos:   EPS bearer level QoS parameters            **
 **      tft:       Traffic flow template parameters           **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     ebi:       EPS bearer identity assigned to the new    **
 **             dedicated bearer context                   **
 **      default_ebi:   EPS bearer identity of the associated de-  **
 **             fault EPS bearer context                   **
 **      esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
esm_proc_dedicated_eps_bearer_context (
  emm_data_context_t * emm_context,
  ebi_t  default_ebi,
  const proc_tid_t   pti,                  // todo: Will always be 0 for network initiated bearer establishment.
  const pdn_cid_t    pdn_cid,              // todo: Per APN for now.
  bearer_context_to_be_created_t *bc_tbc,
  esm_cause_t *esm_cause)
{
  OAILOG_FUNC_IN (LOG_NAS_ESM);
  mme_ue_s1ap_id_t                        ue_id = emm_context->ue_id;
  ebi_t                                   ded_ebi = 0;
  pdn_context_t                          *pdn_context = NULL;
  OAILOG_INFO (LOG_NAS_ESM, "ESM-PROC  - Dedicated EPS bearer context activation " "(ue_id=" MME_UE_S1AP_ID_FMT ", pid=%d)\n",
      ue_id, pdn_cid);

  ue_context_t                        *ue_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, emm_context->ue_id);
  mme_app_get_pdn_context(ue_context, pdn_cid, default_ebi, NULL, &pdn_context);
  if(!pdn_context){
    OAILOG_ERROR(LOG_NAS_EMM, "EMMCN-SAP  - " "No PDN context was found for UE " MME_UE_S1AP_ID_FMT" for cid %d and default ebi %d to assign dedicated bearers.\n", ue_context->mme_ue_s1ap_id, pdn_cid, default_ebi);
    OAILOG_FUNC_RETURN (LOG_NAS_ESM, RETURNerror);
  }
  /*
   * No bearer context is assigned yet. Create a new EPS bearer context procedure.
   * Reserve EPS bearer contexts into  the procedure.
   * Assign new EPS bearer context.
   * Put it into the list of session bearers.
   */
  ded_ebi = esm_ebr_assign (emm_context, ESM_EBI_UNASSIGNED, pdn_context);
  if (ded_ebi != ESM_EBI_UNASSIGNED) {
    /*
     * Create default EPS bearer context.
     * Null as Bearer Level QoS
     */
    bearer_qos_t bearer_qos = {.qci = bc_tbc->bearer_level_qos.qci};
    bearer_qos.pci = bc_tbc->bearer_level_qos.pci;
    bearer_qos.pvi = bc_tbc->bearer_level_qos.pvi;
    bearer_qos.pl  = bc_tbc->bearer_level_qos.pl;

    struct fteid_set_s fteid_set;
    fteid_set.s1u_fteid = &bc_tbc->s1u_sgw_fteid;
    fteid_set.s5_fteid  = &bc_tbc->s5_s8_u_pgw_fteid;
    ded_ebi = esm_ebr_context_create (emm_context, pti, pdn_context, ded_ebi, &fteid_set, IS_DEFAULT_BEARER_NO, &bearer_qos,
        &bc_tbc->tft,
        &bc_tbc->pco);
    /** Check the EBI. */
    DevAssert(ded_ebi != ESM_EBI_UNASSIGNED);

    bc_tbc->eps_bearer_id = ded_ebi;
    OAILOG_INFO(LOG_NAS_ESM, "ESM-PROC  - Successfully reserved bearer with ebi %d. \n", bc_tbc->eps_bearer_id);
    OAILOG_FUNC_RETURN (LOG_NAS_ESM, RETURNok);
  }else{
    OAILOG_INFO(LOG_NAS_ESM, "ESM-PROC  - Error assigning bearer context for ue " MME_UE_S1AP_ID_FMT ". \n", ue_context->mme_ue_s1ap_id);
    OAILOG_FUNC_RETURN (LOG_NAS_ESM, RETURNerror);
  }
 }

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_dedicated_eps_bearer_context_request()           **
 **                                                                        **
 ** Description: Initiates the dedicated EPS bearer context activation pro-**
 **      cedure                                                    **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.2.2                           **
 **      The MME initiates the dedicated EPS bearer context activa-**
 **      tion procedure by sending an ACTIVATE DEDICATED EPS BEA-  **
 **      RER CONTEXT REQUEST message, starting timer T3485 and en- **
 **      tering state BEARER CONTEXT ACTIVE PENDING.               **
 **                                                                        **
 ** Inputs:  is_standalone: Not used (always true)                     **
 **      ue_id:      UE lower layer identifier                  **
 **      ebi:       EPS bearer identity                        **
 **      msg:       Encoded ESM message to be sent             **
 **      ue_triggered:  true if the EPS bearer context procedure   **
 **             was triggered by the UE                    **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
esm_proc_dedicated_eps_bearer_context_request (
  bool is_standalone,
  emm_data_context_t * emm_context,
  ebi_t ebi,
  STOLEN_REF bstring *msg,
  bool ue_triggered)
{
  OAILOG_FUNC_IN (LOG_NAS_ESM);
  int                                     rc = RETURNok;
  mme_ue_s1ap_id_t                        ue_id = emm_context->ue_id;

  OAILOG_INFO (LOG_NAS_ESM, "ESM-PROC  - Initiate dedicated EPS bearer context " "activation (ue_id=" MME_UE_S1AP_ID_FMT ", ebi=%d)\n",
      ue_id, ebi);
  /*
   * Send activate dedicated EPS bearer context request message and
   * * * * start timer T3485
   */
  rc = _dedicated_eps_bearer_activate (emm_context, ebi, msg);

  if (rc != RETURNerror) {
    /*
     * Set the EPS bearer context state to ACTIVE PENDING
     */
    rc = esm_ebr_set_status (emm_context, ebi, ESM_EBR_ACTIVE_PENDING, ue_triggered);

    if (rc != RETURNok) {
      /*
       * The EPS bearer context was already in ACTIVE PENDING state
       */
      OAILOG_WARNING (LOG_NAS_ESM, "ESM-PROC  - EBI %d was already ACTIVE PENDING\n", ebi);
    }
  }

  OAILOG_FUNC_RETURN (LOG_NAS_ESM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_dedicated_eps_bearer_context_accept()            **
 **                                                                        **
 ** Description: Performs dedicated EPS bearer context activation procedu- **
 **      re accepted by the UE.                                    **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.2.3                           **
 **      Upon receipt of the ACTIVATE DEDICATED EPS BEARER CONTEXT **
 **      ACCEPT message, the MME shall stop the timer T3485 and    **
 **      enter the state BEARER CONTEXT ACTIVE.                    **
 **                                                                        **
 ** Inputs:  ue_id:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
esm_proc_dedicated_eps_bearer_context_accept (
  emm_data_context_t * emm_context,
  ebi_t ebi,
  esm_cause_t *esm_cause)
{
  OAILOG_FUNC_IN (LOG_NAS_ESM);
  int                                     rc = RETURNerror;
  mme_ue_s1ap_id_t                        ue_id = emm_context->ue_id;

  OAILOG_INFO (LOG_NAS_ESM, "ESM-PROC  - Dedicated EPS bearer context activation " "accepted by the UE (ue_id=" MME_UE_S1AP_ID_FMT ", ebi=%d)\n",
      ue_id, ebi);
  /*
   * Stop T3485 timer.
   * Will also check if the bearer exists. If not (E-RAB Setup Failure), the message will be dropped.
   */
  rc = esm_ebr_stop_timer (emm_context, ebi);

  if (rc != RETURNerror) {
    /*
     * Set the EPS bearer context state to ACTIVE
     */
    rc = esm_ebr_set_status (emm_context, ebi, ESM_EBR_ACTIVE, false);

    if (rc != RETURNok) {
      /*
       * The EPS bearer context was already in ACTIVE state
       */
      OAILOG_WARNING (LOG_NAS_ESM, "ESM-PROC  - EBI %d was already ACTIVE\n", ebi);
      *esm_cause = ESM_CAUSE_PROTOCOL_ERROR;
    }
    /*
     * No need for an ESM procedure.
     * Just set the status to active and inform the MME_APP layer.
     */
    nas_itti_activate_bearer_cnf(ue_id, ebi);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_ESM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    esm_proc_dedicated_eps_bearer_context_reject()            **
 **                                                                        **
 ** Description: Performs dedicated EPS bearer context activation procedu- **
 **      re not accepted by the UE.                                **
 **                                                                        **
 **      3GPP TS 24.301, section 6.4.2.4                           **
 **      Upon receipt of the ACTIVATE DEDICATED EPS BEARER CONTEXT **
 **      REJECT message, the MME shall stop the timer T3485, enter **
 **      the state BEARER CONTEXT INACTIVE and abort the dedicated **
 **      EPS bearer context activation procedure.                  **
 **      The MME also requests the lower layer to release the ra-  **
 **      dio resources that were established during the dedicated  **
 **      EPS bearer context activation.                            **
 **                                                                        **
 ** Inputs:  ue_id:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     esm_cause: Cause code returned upon ESM procedure     **
 **             failure                                    **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
esm_proc_dedicated_eps_bearer_context_reject (
  emm_data_context_t * emm_context,
  ebi_t ebi,
  esm_cause_t *esm_cause)
{
  int                                     rc;
  mme_ue_s1ap_id_t                        ue_id  = emm_context->ue_id;
  pdn_cid_t                               pdn_ci = PROCEDURE_TRANSACTION_IDENTITY_UNASSIGNED;

  OAILOG_FUNC_IN (LOG_NAS_ESM);
  OAILOG_WARNING (LOG_NAS_ESM, "ESM-PROC  - Dedicated EPS bearer context activation " "not accepted by the UE (ue_id=" MME_UE_S1AP_ID_FMT ", ebi=%d)\n",
      ue_id, ebi);
  /*
   * Stop T3485 timer if running.
   * Will also check if the bearer exists. If not (E-RAB Setup Failure), the message will be dropped.
   */
  rc = esm_ebr_stop_timer (emm_context, ebi);

  if (rc != RETURNerror) {
    /*
     * Release the dedicated EPS bearer context and enter state INACTIVE
     */
    ebi = esm_ebr_context_release (emm_context, ebi, pdn_ci, false);
    if (ebi == ESM_EBI_UNASSIGNED) {
      OAILOG_WARNING (LOG_NAS_ESM, "ESM-PROC  - Failed to release EPS bearer context\n");
      OAILOG_FUNC_RETURN (LOG_NAS_ESM, rc);
    }

    if (rc != RETURNok) {
      /*
       * Failed to release the dedicated EPS bearer context
       */
      *esm_cause = ESM_CAUSE_PROTOCOL_ERROR;
    }
    nas_itti_activate_bearer_rej(ue_id, ebi);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_ESM, rc);
}


/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
                Timer handlers
   --------------------------------------------------------------------------
*/
/****************************************************************************
 **                                                                        **
 ** Name:    _dedicated_eps_bearer_activate_t3485_handler()            **
 **                                                                        **
 ** Description: T3485 timeout handler                                     **
 **                                                                        **
 **              3GPP TS 24.301, section 6.4.2.6, case a                   **
 **      On the first expiry of the timer T3485, the MME shall re- **
 **      send the ACTIVATE DEDICATED EPS BEARER CONTEXT REQUEST    **
 **      and shall reset and restart timer T3485. This retransmis- **
 **      sion is repeated four times, i.e. on the fifth expiry of  **
 **      timer T3485, the MME shall abort the procedure, release   **
 **      any resources allocated for this activation and enter the **
 **      state BEARER CONTEXT INACTIVE.                            **
 **                                                                        **
 ** Inputs:  args:      handler parameters                         **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                       **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static void _dedicated_eps_bearer_activate_t3485_handler (void *args)
{
  OAILOG_FUNC_IN (LOG_NAS_ESM);
  int                                     rc;

  /*
   * Get retransmission timer parameters data
   */
  esm_ebr_timer_data_t                   *esm_ebr_timer_data = (esm_ebr_timer_data_t *) (args);

  if (esm_ebr_timer_data) {
    /*
     * Increment the retransmission counter
     */
    esm_ebr_timer_data->count += 1;
    OAILOG_WARNING (LOG_NAS_ESM, "ESM-PROC  - T3485 timer expired (ue_id=" MME_UE_S1AP_ID_FMT ", ebi=%d), " "retransmission counter = %d\n",
        esm_ebr_timer_data->ue_id, esm_ebr_timer_data->ebi, esm_ebr_timer_data->count);

    if (esm_ebr_timer_data->count < DEDICATED_EPS_BEARER_ACTIVATE_COUNTER_MAX) {
      /*
       * Re-send activate dedicated EPS bearer context request message
       * * * * to the UE
       */
      bstring b = bstrcpy(esm_ebr_timer_data->msg);
      rc = _dedicated_eps_bearer_activate (esm_ebr_timer_data->ctx, esm_ebr_timer_data->ebi, &b);
    } else {
      /*
       * The maximum number of activate dedicated EPS bearer context request
       * message retransmission has exceed
       */
      pdn_cid_t                               pid = MAX_APN_PER_UE;

      /*
       * Release the dedicated EPS bearer context and enter state INACTIVE
       */
      rc = esm_proc_eps_bearer_context_deactivate (esm_ebr_timer_data->ctx, true, esm_ebr_timer_data->ebi, pid, NULL);

      if (rc != RETURNerror) {
        /*
         * Stop timer T3485
         */
        rc = esm_ebr_stop_timer (esm_ebr_timer_data->ctx, esm_ebr_timer_data->ebi);
      }
    }
    if (esm_ebr_timer_data->msg) {
      bdestroy_wrapper (&esm_ebr_timer_data->msg);
    }
    free_wrapper ((void**)&esm_ebr_timer_data);
  }

  OAILOG_FUNC_OUT (LOG_NAS_ESM);
}

/*
   --------------------------------------------------------------------------
                MME specific local functions
   --------------------------------------------------------------------------
*/

/****************************************************************************
 **                                                                        **
 ** Name:    _dedicated_eps_bearer_activate()                          **
 **                                                                        **
 ** Description: Sends ACTIVATE DEDICATED EPS BEREAR CONTEXT REQUEST mes-  **
 **      sage and starts timer T3485                               **
 **                                                                        **
 ** Inputs:  ue_id:      UE local identifier                        **
 **      ebi:       EPS bearer identity                        **
 **      msg:       Encoded ESM message to be sent             **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    T3485                                      **
 **                                                                        **
 ***************************************************************************/
static int _dedicated_eps_bearer_activate (
  emm_data_context_t * emm_context,
  ebi_t ebi,
  STOLEN_REF bstring *msg)
{
  OAILOG_FUNC_IN (LOG_NAS_ESM);
  emm_sap_t                               emm_sap = {0};
  int                                     rc;
  mme_ue_s1ap_id_t                        ue_id = emm_context->ue_id;
  ue_context_t                           *ue_context  = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, emm_context->ue_id);
  bearer_context_t                       *bearer_context = NULL;

  /*
   * Notify EMM that an activate dedicated EPS bearer context request
   * message has to be sent to the UE
   */
  emm_esm_activate_bearer_req_t          *emm_esm_activate = &emm_sap.u.emm_esm.u.activate_bearer;

  mme_app_get_session_bearer_context_from_all(ue_context, ebi, &bearer_context);
  DevAssert(bearer_context);

  emm_sap.primitive       = EMMESM_ACTIVATE_BEARER_REQ;
  emm_sap.u.emm_esm.ue_id = ue_id;
  emm_sap.u.emm_esm.ctx   = emm_context;
  emm_esm_activate->msg            = *msg;
  emm_esm_activate->ebi            = ebi;

  emm_esm_activate->mbr_dl         = bearer_context->esm_ebr_context.mbr_dl;
  emm_esm_activate->mbr_ul         = bearer_context->esm_ebr_context.mbr_ul;
  emm_esm_activate->gbr_dl         = bearer_context->esm_ebr_context.gbr_dl;
  emm_esm_activate->gbr_ul         = bearer_context->esm_ebr_context.gbr_ul;

  bstring msg_dup = bstrcpy(*msg);
  *msg = NULL;
  MSC_LOG_TX_MESSAGE (MSC_NAS_ESM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMESM_ACTIVATE_BEARER_REQ ue id " MME_UE_S1AP_ID_FMT " ebi %u", ue_id, ebi);
  rc = emm_sap_send (&emm_sap);

  if (rc != RETURNerror) {
    /*
     * Start T3485 retransmission timer
     */
    rc = esm_ebr_start_timer (emm_context, ebi, msg_dup, mme_config.nas_config.t3485_sec, _dedicated_eps_bearer_activate_t3485_handler);
  } else {
    bdestroy_wrapper(&msg_dup);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_ESM, rc);
}
