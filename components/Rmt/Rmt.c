#include <stdio.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/list.h"

#include "esp_log.h"

#include "Rmt.h"
#include "du.h"
#include "IPCP.h"
#include "pci.h"
#include "normalIPCP.h"
#include "configRINA.h"
//#include "EFCP.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

#define MAX_PDUS_SENT_PER_CYCLE 10

/** @brief RMT Array PortId Created.
 *  */
static portTableEntry_t xPortIdTable[2];

pci_t *vCastPointerTo_pci_t(void *pvArgument);

/* @brief Called when a SDU arrived into the RMT from the Shim DIF */
// BaseType_t xRmtReceive(struct ipcpNormalData_t *pxData, struct du_t *pxDu, portId_t xFrom);

/* @brief Called when a SDU arrived into the RMT from the EFCP Container*/
static int xRmtN1PortWriteDu(rmt_t *pxRmt, struct rmtN1Port_t *pxN1Port, struct du_t *pxDu);

/* @brief Create an N-1 Port in the RMT Component*/
static struct rmtN1Port_t *pxRmtN1PortCreate(portId_t xId, ipcpInstance_t *pxN1Ipcp);

static struct rmtN1Port_t *pxRmtN1PortCreate(portId_t xId, ipcpInstance_t *pxN1Ipcp)
{

	ESP_LOGD(TAG_RMT, "Creating a N-1 Port in the RMT");
	struct rmtN1Port_t *pxTmp;

	// configASSERT(IS_PORT_ID_OK(xId));

	pxTmp = pvPortMalloc(sizeof(*pxTmp));
	if (!pxTmp)
		return NULL;

	pxTmp->xPortId = xId;
	pxTmp->pxN1Ipcp = pxN1Ipcp;
	pxTmp->eState = eN1_PORT_STATE_ENABLED;

	pxTmp->uxBusy = pdFALSE;
	pxTmp->xStats.plen = 0;
	pxTmp->xStats.dropPdus = 0;
	pxTmp->xStats.errPdus = 0;
	pxTmp->xStats.txPdus = 0;
	pxTmp->xStats.txBytes = 0;
	pxTmp->xStats.rxPdus = 0;
	pxTmp->xStats.rxBytes = 0;
	pxTmp->pxRmtPsQueues = pxRmtQueueCreate(xId);

	ESP_LOGD(TAG_RMT, "N-1 port %pK created successfully (port-id = %d)", pxTmp, xId);

	return pxTmp;
}

/* @brief Bind the N-1 Port with the RMT. SDUP and RMT Policies are not considered.�
 * It is called from the IPCP normal when a Flow is required to be bounded. From the
 * IPCP normal is send the RMT instance, the portId from the Shim, and Shim Instance */
BaseType_t xRmtN1PortBind(rmt_t *pxRmtInstance, portId_t xId, ipcpInstance_t *pxN1Ipcp);

BaseType_t xRmtN1PortBind(rmt_t *pxRmtInstance, portId_t xId, ipcpInstance_t *pxN1Ipcp)
{
	ESP_LOGD(TAG_RMT, "Binding the RMT with the port id:%d", xId);

	struct rmtN1Port_t *pxTmp;
	// struct rmt_ps *ps;

	if (!pxRmtInstance)
	{
		ESP_LOGE(TAG_RMT, "Bogus instance passed");
		return pdFALSE;
	}

	if (!IS_PORT_ID_OK(xId))
	{
		ESP_LOGE(TAG_RMT, "Wrong port-id %d", xId);
		return pdFALSE;
	}

	if (!pxN1Ipcp)
	{
		ESP_LOGE(TAG_RMT, "Invalid N-1 IPCP passed");
		return pdFALSE;
	}

	if (!pxRmtInstance->pxN1Port)
	{
		ESP_LOGE(TAG_RMT, "This RMT has already an N-1 Port binded ");
		return pdFALSE;
	}

	pxTmp = pxRmtN1PortCreate(xId, pxN1Ipcp);
	if (!pxTmp)
		return pdFALSE;

	/*
	if (!ps || !ps->rmt_q_create_policy) {
		rcu_read_unlock();
		LOG_ERR("No PS in the RMT, can't bind");
		n1_port_destroy(tmp);
		return -1;
	}

	tmp->rmt_ps_queues = ps->rmt_q_create_policy(ps, tmp);
	rcu_read_unlock();

	if (!tmp->rmt_ps_queues) {
		LOG_ERR("Cannot create structs for scheduling policy");
		n1_port_destroy(tmp);
		return -1;
	}*/

	/*No added to the Hash Table because we've assumed there is only a flow in the shim DIF
	 * Instead, it is aggregate to the PortIdArray with an unique member.*/

	xPortIdTable[0].pxPortN1 = pxTmp;

	ESP_LOGD(TAG_RMT, "Added send queue to rmt instance %pK for port-id %d", pxRmtInstance, xId);

	/*Associate the DIF name with the SDUP configuration, SDUP will not be used for the moment*/

	return pdTRUE;
}

/* @brief Add an Address into the RMT list. This list is useful when the
 * packet arrived and need to know whether it is for us or not. */

BaseType_t xRmtAddressAdd(rmt_t *pxInstance, address_t xAddress)
{
	rmtAddress_t *pxRmtAddr;

	if (!pxInstance)
	{
		ESP_LOGE(TAG_RMT, "Bogus instance passed");
		return pdFALSE;
	}

	pxRmtAddr = pvPortMalloc(sizeof(*pxRmtAddr));
	if (!pxRmtAddr)
		return pdFALSE;

	pxRmtAddr->xAddress = xAddress;

	ESP_LOGE(TAG_RMT, "Adding and Address into the RMT list:%d", pxRmtAddr->xAddress);

	vListInitialiseItem(&(pxRmtAddr->xAddressListItem));
	listSET_LIST_ITEM_OWNER(&(pxRmtAddr->xAddressListItem), (void *)pxRmtAddr);
	vListInsert(&pxInstance->xAddresses, &(pxRmtAddr->xAddressListItem));

	return pdTRUE;
}

/* @brief Check if the Address defined in the PDU is stored in
 * the address list in the RMT.*/
BaseType_t xRmtPduIsAddressedToMe(rmt_t *pxRmt, address_t xAddress);

BaseType_t xRmtPduIsAddressedToMe(rmt_t *pxRmt, address_t xAddress)
{
	ESP_LOGE(TAG_RMT, "looking for address:%d", xAddress);
	rmtAddress_t *pxAddr;

	ListItem_t *pxListItem, *pxNext;
	ListItem_t const *pxListEnd;

	pxAddr = pvPortMalloc(sizeof(*pxAddr));

	/* Find a way to iterate in the list and compare the addesss*/
	pxListEnd = listGET_END_MARKER(&pxRmt->xAddresses);
	pxListItem = listGET_HEAD_ENTRY(&pxRmt->xAddresses);
	ESP_LOGE(TAG_RMT, "FLAG");
	while (pxListItem != pxListEnd)
	{

		pxAddr = (rmtAddress_t *)listGET_LIST_ITEM_OWNER(pxListItem);
		ESP_LOGE(TAG_RMT, "Address to evaluate: %d, Address founded:%d", xAddress, pxAddr->xAddress);
		if (pxAddr->xAddress == xAddress)
		{
			ESP_LOGD(TAG_RMT, "Address to me founded!");
			return pdTRUE;
		}
		pxListItem = listGET_NEXT(pxListItem);
		;
	}

	return pdFALSE;
}

static BaseType_t xRmtProcessMgmtPdu(struct ipcpNormalData_t *pxData, portId_t xPortId, struct du_t *pxDu);
static BaseType_t xRmtProcessMgmtPdu(struct ipcpNormalData_t *pxData, portId_t xPortId, struct du_t *pxDu)
{

	if (!xNormalMgmtDuPost(pxData, xPortId, pxDu))
	{
		ESP_LOGE(TAG_RMT, "Failed");
		return pdFALSE;
	}
	return pdTRUE;
}

static BaseType_t xRmtProcessDtPdu(rmt_t *pxRmt, portId_t xPortId, struct du_t *pxDu);

static BaseType_t xRmtProcessDtPdu(rmt_t *pxRmt, portId_t xPortId, struct du_t *pxDu)
{
	address_t xDstAddrTmp;
	cepId_t xCepTmp;
	pduType_t xPduTypeTmp;

	xDstAddrTmp = pxDu->pxPci->xDestination;

	if (!is_address_ok(xDstAddrTmp))
	{
		ESP_LOGE(TAG_RMT, "PDU has wrong destination address");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	xPduTypeTmp = pxDu->pxPci->xType;

	if (xPduTypeTmp == PDU_TYPE_MGMT)
	{
		ESP_LOGE(TAG_RMT, "MGMT should not be here");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	xCepTmp = pxDu->pxPci->connectionId_t.xDestination;

	if (!is_cep_id_ok(xCepTmp))
	{
		ESP_LOGE(TAG_RMT, "Wrong CEP-id in PDU");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	if (!xEfcpContainerReceive(pxRmt->pxEfcpc, xCepTmp, pxDu))
	{
		ESP_LOGE(TAG_RMT, "EFCP container problems");
		return pdFALSE;
	}

	ESP_LOGD(TAG_RMT, "process_dt_pdu internally finished");

	return pdTRUE;
}

BaseType_t xRmtReceive(struct ipcpNormalData_t *pxData, struct du_t *pxDu, portId_t xFrom);

BaseType_t xRmtReceive(struct ipcpNormalData_t *pxData, struct du_t *pxDu, portId_t xFrom)
{
	ESP_LOGD(TAG_RMT, "RMT has received a RINA packet from the port %d", xFrom);

	pduType_t xPduType;
	address_t xDstAddr;
	qosId_t xQosId;
	struct rmtN1Port_t *pxN1Port;
	size_t uxBytes;

	if (!pxData->pxRmt)
	{
		ESP_LOGE(TAG_RMT, "No RMT passed");
		xDuDestroy(pxDu);
		return pdFALSE;
	}
	if (!IS_PORT_ID_OK(xFrom))
	{
		ESP_LOGE(TAG_RMT, "Wrong port-id %d", xFrom);
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	uxBytes = pxDu->pxNetworkBuffer->xRinaDataLength;
	pxDu->pxCfg = pxData->pxEfcpc->pxConfig;

	pxN1Port = xPortIdTable[0].pxPortN1;
	if (!pxN1Port)
	{
		ESP_LOGE(TAG_RMT, "Could not retrieve N-1 port for the received PDU...");
		xDuDestroy(pxDu);
		return pdFALSE;
	}
	stats_inc(rx, pxN1Port, uxBytes);

	/* SDU Protection to be implemented after testing if it is required.
	if (sdup_unprotect_pdu(n1_port->sdup_port, pxDu)) {
		ESP_LOGE(TAG_RMT,"Failed to unprotect PDU");
		xDuDestroy(pxDu);
		return pdFALSE;
	}*/

	/* This one updates the pci->sdup_header and pdu->skb->data pointers
	if (sdup_get_lifetime_limit(n1_port->sdup_port, pxDu)) {
		ESP_LOGE(TAG_RMT,"Failed to get PDU's TTL");
		xDuDestroy(pxDu);
		return pdFALSE;
	}*/
	/* end SDU Protection */

	if (unlikely(xDuDecap(pxDu)))
	{
		/*Decap PDU */
		ESP_LOGE(TAG_RMT, "Could not decap PDU");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	ESP_LOGD(TAG_RMT, "DU Decap sucessfuly");

	xPduType = pxDu->pxPci->xType;
	xDstAddr = pxDu->pxPci->xDestination;
	xQosId = pxDu->pxPci->connectionId_t.xQosId;

	if (!pdu_type_is_ok(xPduType) ||
		!is_address_ok(xDstAddr) ||
		!is_qos_id_ok(xQosId))
	{
		ESP_LOGE(TAG_RMT, "Wrong PDU type (%u), dst address (%u) or qos_id (%u)",
				 xPduType, xDstAddr, xQosId);
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	/* pdu is for me */
	// if (xRmtPduIsAddressedToMe(pxData->pxRmt, xDstAddr))
	if (xDstAddr == LOCAL_ADDRESS)
	{
		/* pdu is for me */
		switch (xPduType)
		{
		case PDU_TYPE_MGMT:
			ESP_LOGD(TAG_RMT, "Mgmt PDU!!!");
			return xRmtProcessMgmtPdu(pxData, xFrom, pxDu);

		case PDU_TYPE_CACK:
		case PDU_TYPE_SACK:
		case PDU_TYPE_NACK:
		case PDU_TYPE_FC:
		case PDU_TYPE_ACK:
		case PDU_TYPE_ACK_AND_FC:
		case PDU_TYPE_RENDEZVOUS:
		case PDU_TYPE_DT:
			ESP_LOGD(TAG_RMT, "DT PDU!!!");
			/*
			 * (FUTURE)
			 *
			 * enqueue PDU in pdus_dt[dest-addr, qos-id]
			 * don't process it now ...
			 */
			return xRmtProcessDtPdu(pxData->pxRmt, xFrom, pxDu);

		default:
			ESP_LOGE(TAG_RMT, "Unknown PDU type %d", xPduType);
			xDuDestroy(pxDu);
			return pdFALSE;
		}
	}
	/* pdu is not for me. Then release buffer and destroy anything.
	 * A forwarding to next hop will be consider in other version. */
	else
	{
		if (!xDstAddr)
			return xRmtProcessMgmtPdu(pxData, xFrom, pxDu);
		else
		{
			ESP_LOGD(TAG_RMT, "PDU is not for me");
			return pdFALSE;
		}
	}
}

void vRmtN1PortBusy(rmt_t *pxRmt, NetworkBufferDescriptor_t *pxNetworkBuffer)
{
	struct du_t *pxDu;

	pxDu = pvPortMalloc(sizeof(*pxDu));
	pxDu->pxNetworkBuffer = pxNetworkBuffer;
	vTaskSuspendAll();
	{
		taskENTER_CRITICAL(&mux);
		{

			pxRmt->pxN1Port->uxBusy = pdTRUE;
			(void)xRfifoPushHead(pxRmt->pxN1Port->pxRmtPsQueues->pxDtQueue, (void *)&pxDu);
			pxRmt->pxN1Port->pxPendingDu = NULL;
		}
		taskEXIT_CRITICAL(&mux);
	}
	(void)xTaskResumeAll();
}

static BaseType_t xRmtN1PortWriteDu(rmt_t *pxRmt,
									struct rmtN1Port_t *pxN1Port,
									struct du_t *pxDu)
{

	BaseType_t ret;
	ssize_t bytes = pxDu->pxNetworkBuffer->xDataLength;

	ESP_LOGD(TAG_RMT, "Gonna send SDU to port-id %d", pxN1Port->xPortId);
	pxRmt->pxN1Port = pxN1Port;
	ret = pxN1Port->pxN1Ipcp->pxOps->duWrite(pxN1Port->pxN1Ipcp->pxData, pxN1Port->xPortId, pxDu, false);

	/*TODO: Change this to support the RMT queue*/
	if (!ret)
		return pdFALSE;

	if (ret == pdFALSE)
	{

		if (pxN1Port->pxPendingDu)
		{
			ESP_LOGE(TAG_RMT, "Already a pending SDU present for port %d",
					 pxN1Port->xPortId);
			xDuDestroy(pxN1Port->pxPendingDu);
			pxN1Port->xStats.plen--;
		}

		pxN1Port->pxPendingDu = pxDu;
		pxN1Port->xStats.plen++;
		ESP_LOGD(TAG_RMT, "xRmtN1PortWriteDu:Pending");

		if (pxN1Port->eState == eN1_PORT_STATE_DO_NOT_DISABLE)
		{
			pxN1Port->eState = eN1_PORT_STATE_ENABLED;
			// tasklet_hi_schedule(&rmt->egress_tasklet);
		}
		else
			pxN1Port->eState = eN1_PORT_STATE_DISABLED;
	}

	return pdTRUE;
}

BaseType_t xRmtSendPortId(rmt_t *pxRmtInstance,
						  portId_t xPortId,
						  struct du_t *pxDu)
{

	ESP_LOGD(TAG_RMT, "Processing DU to be send");

	struct rmtN1Port_t *pxN1Port;
	// rmtPs_t *ps;//???
	int cases;
	BaseType_t ret;
	BaseType_t xMustEnqueue;

	/*ps = container_of(rcu_dereference(instance->base.ps),
			  struct rmt_ps, base);

	if (!ps || !ps->rmt_enqueue_policy) {
		rcu_read_unlock();
		LOG_ERR("PS or enqueue policy null, dropping pdu");
		du_destroy(du);
		return -1;
	}*/
	pxN1Port = xPortIdTable[0].pxPortN1;
	// pxN1Port = n1pmap_find(instance, id);
	if (!pxN1Port)
	{

		ESP_LOGE(TAG_RMT, "Could not find the N-1 port");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	xMustEnqueue = pdFALSE;
	if (pxN1Port->uxBusy)
	{

		xMustEnqueue = pdTRUE;
	}

	ret = uxRmtEnqueueDfltPolicy(pxN1Port, pxDu, xMustEnqueue);
	// cases = 0; // Send Default policy, change later.

	switch (ret)
	{

	case RMT_PS_ENQ_SCHED:
		pxN1Port->xStats.plen++;
		/* send an event to the RINA task to activate the Queue Timer to schedule and send data.*/
		// ESP_LOGE(TAG_RMT, "SCHEDULE");

		ret = pdTRUE;
		break;
	case RMT_PS_ENQ_DROP:
		pxN1Port->xStats.dropPdus++;
		// ESP_LOGE(TAG_RMT, "PDU dropped while enqueing");
		ret = pdTRUE;
		break;
	case RMT_PS_ENQ_ERR:
		pxN1Port->xStats.errPdus++;
		ESP_LOGE(TAG_RMT, "Some error occurred while enqueuing PDU");
		ret = pdTRUE;
		break;

	case RMT_PS_ENQ_SEND:
		if (xMustEnqueue)
		{
			ESP_LOGE(TAG_RMT, "Wrong behaviour of the policy");
			xDuDestroy(pxDu);
			pxN1Port->xStats.errPdus++;
			ESP_LOGD(TAG_RMT, "Policy should have enqueue, returned SEND");
			ret = pdFALSE;
			break;
		}

		ESP_LOGD(TAG_RMT, "PDU ready to be sent, no need to enqueue");
		ret = xRmtN1PortWriteDu(pxRmtInstance, pxN1Port, pxDu);
		// ESP_LOGE(TAG_RMT, "Sended");

		if (ret >= 0)
		{
			stats_inc(tx, pxN1Port, ret);
			ret = pdTRUE;
		}
		break;
	default:
		ESP_LOGE(TAG_RMT, "rmt_enqueu_policy returned wrong value");
		break;
		ret = pdTRUE;
	}

	// n1_port_unlock(n1_port);
	// n1pmap_release(instance, n1_port);
	return ret;
}

BaseType_t xRmtSend(rmt_t *pxRmtInstance,
					struct du_t *pxDu)
{
	int i;

	if (!pxRmtInstance || !pxDu || !xPciIsOk(pxDu->pxPci))
	{
		ESP_LOGE(TAG_RMT, "Bogus input parameters passed");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

#if 0
	if (pff_nhop(instance->pff, &du->pci,
		     &(instance->cache.pids),
		     &(instance->cache.count))) {
		LOG_ERR("Cannot get the NHOP for this PDU (saddr: %u daddr: %u type: %u)",
				pci_source(&du->pci), pci_destination(&du->pci),
				pci_type(&du->pci));

		du_destroy(du); 
		return -1;
	}

	if (pxRmtInstance. instance->cache.count == 0) {
		ESP_LOGD(TAG_RMT, "No NHOP for this PDU ...");
		xDuDestroy(pxDu);
		return pdFALSE;
	}

	for (i = 0; i < instance->cache.count; i++) {
		portId_t   pid;
		struct du_t * pxDuTmp;

		pid = instance->cache.pids[i];

		if (i == instance->cache.count-1)
			pxDuTmp = pxDu;
		else
			pxDuTmp = du_dup(pxDu);

		if (rmt_send_port_id(instance, pid, pxDuTmp))
			ESP_LOGE("Failed to send a PDU to port-id %d", pid);
	}
#endif

	if (!xRmtSendPortId(pxRmtInstance, pxRmtInstance->pxN1Port->xPortId, pxDu))
	{
		ESP_LOGE(TAG_RMT, "Failed to send a PDU to port-id %d", pxRmtInstance->pxN1Port->xPortId);
		return pdFALSE;
	}
	return pdTRUE;
}

pci_t *vCastPointerTo_pci_t(void *pvArgument)
{
	return (void *)(pvArgument);
}

rmt_t *pxRmtCreate(struct efcpContainer_t *pxEfcpc)
{
	rmt_t *pxRmtTmp;
	struct rmtN1Port_t *pxPortN1;

	pxPortN1 = pvPortMalloc(sizeof(*pxPortN1));

	if (!pxEfcpc)
	{
		ESP_LOGE(TAG_RMT, "Bogus input parameters");
		return NULL;
	}

	pxRmtTmp = pvPortMalloc(sizeof(*pxRmtTmp));
	if (!pxRmtTmp)
		return NULL;

	vListInitialise(&pxRmtTmp->xAddresses);

	// pxRmtTmp->pxParent = pxInstance;
	pxRmtTmp->pxEfcpc = pxEfcpc;

	/*tmp->pff = pff_create(&tmp->robj, tmp->parent);
	if (!tmp->pff) {
		rmt_destroy(tmp);
		return NULL;
	}*/
	pxRmtTmp->pxN1Port = pxPortN1;
	// tmp->n1_ports = n1pmap_create(&tmp->robj);
	if (!pxRmtTmp->pxN1Port)
	{
		ESP_LOGD(TAG_RMT, "Failed to create N-1 ports map");
		// rmt_destroy(tmp);
		return NULL;
	}

	/*if (pff_cache_init(&tmp->cache)) {
		LOG_ERR("Failed to init pff cache");
		rmt_destroy(tmp);
		return NULL;
	}

	tasklet_init(&tmp->egress_tasklet,
			 send_worker,
			 (unsigned long) tmp);*/

	ESP_LOGD(TAG_RMT, "Instance %pK initialized successfully", pxRmtTmp);
	return pxRmtTmp;
}

BaseType_t xRmtDequeu(rmt_t *pxRmt)
{

	struct du_t *pxDu = NULL;
	int ret;
	int pdus_sent = 0;

	if (!pxRmt)
	{
		ESP_LOGE(TAG_RMT, "No RMT instance passed");
	}
	pdus_sent = 0;
	ret = 0;

	while (pdus_sent < MAX_PDUS_SENT_PER_CYCLE)
	{

		pxDu = pxRmtDequeueDfltPolicy(pxRmt->pxN1Port);
		if (!pxDu)
		{
			ESP_LOGD(TAG_RMT, "No SDU to be sended");
			break;
		}

		ret = xRmtN1PortWriteDu(pxRmt, pxRmt->pxN1Port, pxDu);

		if (ret < 0)
			break;
		pdus_sent++;
	}

	if (!xRfifoIsEmpty(pxRmt->pxN1Port->pxRmtPsQueues->pxDtQueue))
	{
		return pdFALSE;
	}
	else
	{
		ESP_LOGD(TAG_RMT, "Port is not Busy anymore");
		vTaskSuspendAll();
		{
			taskENTER_CRITICAL(&mux);
			{
				pxRmt->pxN1Port->uxBusy = pdFALSE;
			}
			taskEXIT_CRITICAL(&mux);
		}
		(void)xTaskResumeAll();
		return pdTRUE;
	}
}