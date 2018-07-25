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
Source      esm_ebr_context.h

Version     0.1

Date        2013/05/28

Product     NAS stack

Subsystem   EPS Session Management

Author      Frederic Maurel

Description Defines functions used to handle EPS bearer contexts.

*****************************************************************************/
#ifndef ESM_EBR_CONTEXT_SEEN
#define ESM_EBR_CONTEXT_SEEN

/****************************************************************************/
/*********************  G L O B A L    C O N S T A N T S  *******************/
/****************************************************************************/

/****************************************************************************/
/************************  G L O B A L    T Y P E S  ************************/
/****************************************************************************/


/****************************************************************************/
/********************  G L O B A L    V A R I A B L E S  ********************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

#define IS_DEFAULT_BEARER_YES true
#define IS_DEFAULT_BEARER_NO  false
ebi_t esm_ebr_context_create (
  emm_data_context_t * emm_context,
  const proc_tid_t pti,
  void *pdn_context,
  ebi_t ebi,
  struct fteid_set_s * fteid_set,
  bool is_default,
  const bearer_qos_t *bearer_level_qos,
  traffic_flow_template_t * tft,
  protocol_configuration_options_t * pco);

void esm_ebr_context_init (esm_ebr_context_t *esm_ebr_context);

ebi_t esm_ebr_context_release(emm_data_context_t * emm_context, ebi_t ebi, pdn_cid_t *pid,bool ue_requested);

void free_esm_ebr_context(esm_ebr_context_t * ctx);

#endif /* ESM_EBR_CONTEXT_SEEN */
