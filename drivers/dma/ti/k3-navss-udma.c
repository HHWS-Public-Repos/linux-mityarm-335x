// SPDX-License-Identifier: GPL-2.0
/*
 * K3 NAVSS DMA glue interface
 *
 * Copyright (C) 2018 Texas Instruments Incorporated - http://www.ti.com
 *
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <dt-bindings/dma/k3-udma.h>
#include <linux/irqchip/irq-ti-sci-inta.h>
#include <linux/soc/ti/k3-ringacc.h>
#include <linux/dma/ti-cppi5.h>
#include <linux/dma/k3-navss-udma.h>

#include "k3-udma.h"

struct k3_nav_udmax_common {
	struct device *dev;
	struct udma_dev *udmax;
	const struct udma_tisci_rm *tisci_rm;
	struct k3_ringacc *ringacc;
	u32 src_thread;
	u32 dst_thread;

	u32  hdesc_size;
	bool epib;
	u32  psdata_size;
	u32  swdata_size;
};

struct k3_nav_udmax_tx_channel {
	struct k3_nav_udmax_common common;

	struct udma_tchan *udma_tchanx;
	int udma_tchan_id;
	bool need_tisci_free;

	struct k3_ring *ringtx;
	struct k3_ring *ringtxcq;

	bool psil_paired;

	unsigned int virq;

	atomic_t free_pkts;
	bool tx_pause_on_err;
	bool tx_filt_einfo;
	bool tx_filt_pswords;
	bool tx_supr_tdpkt;
};

/**
 * k3_nav_udmax_rx_flow - UDMA RX flow context data
 *
 */
struct k3_nav_udmax_rx_flow {
	struct udma_rflow *udma_rflow;
	int udma_rflow_id;
	struct k3_ring *ringrx;
	struct k3_ring *ringrxfdq;

	unsigned int virq;
};

struct k3_nav_udmax_rx_channel {
	struct k3_nav_udmax_common common;

	struct udma_rchan *udma_rchanx;
	int udma_rchan_id;
	bool need_tisci_free;

	bool psil_paired;

	u32  swdata_size;
	int  flow_id_base;

	struct k3_nav_udmax_rx_flow *flows;
	u32 flow_num;
	u32 flows_ready;
};

#define K3_UDMAX_TDOWN_TIMEOUT_US 1000

static int of_k3_nav_udmax_parse(struct device_node *udmax_np,
				 struct k3_nav_udmax_common *common)
{
	common->ringacc = of_k3_ringacc_get_by_phandle(udmax_np, "ti,ringacc");
	if (IS_ERR(common->ringacc))
		return PTR_ERR(common->ringacc);

	common->udmax = of_xudma_dev_get(udmax_np, NULL);
	if (IS_ERR(common->udmax))
		return PTR_ERR(common->udmax);

	common->tisci_rm = xudma_dev_get_tisci_rm(common->udmax);

	return 0;
}

static int of_k3_nav_udmax_parse_chn(struct device_node *chn_np,
				     const char *name,
				     struct k3_nav_udmax_common *common,
				     bool tx_chn)
{
	struct device_node *psil_cfg_node;
	struct device_node *ch_cfg_node;
	struct of_phandle_args dma_spec;
	int index, ret = 0;
	char prop[50];
	u32 val;

	if (unlikely(!name))
		return -EINVAL;

	index = of_property_match_string(chn_np, "dma-names", name);
	if (index < 0)
		return index;

	if (of_parse_phandle_with_args(chn_np, "dmas", "#dma-cells", index,
				       &dma_spec))
		return -ENOENT;

	if (tx_chn && dma_spec.args[2] != UDMA_DIR_TX) {
		ret = -EINVAL;
		goto out_put_spec;
	}

	if (!tx_chn && dma_spec.args[2] != UDMA_DIR_RX) {
		ret = -EINVAL;
		goto out_put_spec;
	}

	/* get psil cfg node */
	psil_cfg_node = of_find_node_by_phandle(dma_spec.args[0]);
	if (!psil_cfg_node) {
		ret = -ENOENT;
		goto out_put_spec;
	}

	snprintf(prop, sizeof(prop), "ti,psil-config%u", dma_spec.args[1]);
	ch_cfg_node = of_find_node_by_name(psil_cfg_node, prop);
	if (!ch_cfg_node) {
		dev_err(common->dev,
			"Channel %u configuration node is missing\n",
			dma_spec.args[1]);
		goto out_put_psil_cfg;
	}

	common->epib = of_property_read_bool(ch_cfg_node, "ti,needs-epib");

	if (!of_property_read_u32(ch_cfg_node, "ti,psd-size", &val))
		common->psdata_size = val;

	ret = of_property_read_u32(psil_cfg_node, "ti,psil-base", &val);
	if (ret) {
		dev_err(common->dev, "ti,psil-base is missing %d\n", ret);
		goto out_ch_cfg;
	}

	if (tx_chn)
		common->dst_thread = val + dma_spec.args[1];
	else
		common->src_thread = val + dma_spec.args[1];
	ret = of_k3_nav_udmax_parse(dma_spec.np, common);

out_ch_cfg:
	of_node_put(ch_cfg_node);
out_put_psil_cfg:
	of_node_put(psil_cfg_node);
out_put_spec:
	of_node_put(dma_spec.np);
	return ret;
};

static void k3_nav_udmax_dump_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn)
{
	struct device *dev = tx_chn->common.dev;

	dev_dbg(dev, "dump_tx_chn:\n"
		"udma_tchan_id: %d\n"
		"src_thread: %08x\n"
		"dst_thread: %08x\n",
		tx_chn->udma_tchan_id,
		tx_chn->common.src_thread,
		tx_chn->common.dst_thread);
}

static void k3_nav_udmax_dump_tx_rt_chn(struct k3_nav_udmax_tx_channel *chn,
					char *mark)
{
	struct device *dev = chn->common.dev;

	dev_dbg(dev, "=== dump ===> %s\n", mark);
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_TCHAN_RT_CTL_REG,
		xudma_tchanrt_read(chn->udma_tchanx, UDMA_TCHAN_RT_CTL_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_TCHAN_RT_PEER_RT_EN_REG,
		xudma_tchanrt_read(chn->udma_tchanx,
				   UDMA_TCHAN_RT_PEER_RT_EN_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_TCHAN_RT_PCNT_REG,
		xudma_tchanrt_read(chn->udma_tchanx, UDMA_TCHAN_RT_PCNT_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_TCHAN_RT_BCNT_REG,
		xudma_tchanrt_read(chn->udma_tchanx, UDMA_TCHAN_RT_BCNT_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_TCHAN_RT_SBCNT_REG,
		xudma_tchanrt_read(chn->udma_tchanx, UDMA_TCHAN_RT_SBCNT_REG));
}

static int k3_nav_udmax_cfg_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn)
{
	const struct udma_tisci_rm *tisci_rm = tx_chn->common.tisci_rm;
	struct ti_sci_msg_rm_udmap_tx_ch_cfg req;

	memset(&req, 0, sizeof(req));

	req.valid_params = TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_FILT_EINFO_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_FILT_PSWORDS_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_SUPR_TDPKT_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID;
	req.nav_id = tisci_rm->tisci_dev_id;
	req.index = tx_chn->udma_tchan_id;
	if (tx_chn->tx_pause_on_err)
		req.tx_pause_on_err = 1;
	if (tx_chn->tx_filt_einfo)
		req.tx_filt_einfo = 1;
	if (tx_chn->tx_filt_pswords)
		req.tx_filt_pswords = 1;
	req.tx_chan_type = TI_SCI_RM_UDMAP_CHAN_TYPE_PKT_PBRR;
	if (tx_chn->tx_supr_tdpkt)
		req.tx_supr_tdpkt = 1;
	req.tx_fetch_size = tx_chn->common.hdesc_size >> 2;
	req.txcq_qnum = k3_ringacc_get_ring_id(tx_chn->ringtxcq);

	return tisci_rm->tisci_udmap_ops->tx_ch_cfg(tisci_rm->tisci, &req);
}

struct k3_nav_udmax_tx_channel *k3_nav_udmax_request_tx_chn(struct device *dev,
		const char *name, struct k3_nav_udmax_tx_channel_cfg *cfg)
{
	struct k3_nav_udmax_tx_channel *tx_chn;
	int ret;

	tx_chn = devm_kzalloc(dev, sizeof(*tx_chn), GFP_KERNEL);
	if (!tx_chn)
		return ERR_PTR(-ENOMEM);

	tx_chn->common.dev = dev;
	tx_chn->common.swdata_size = cfg->swdata_size;
	tx_chn->tx_pause_on_err = cfg->tx_pause_on_err;
	tx_chn->tx_filt_einfo = cfg->tx_filt_einfo;
	tx_chn->tx_filt_pswords = cfg->tx_filt_pswords;
	tx_chn->tx_supr_tdpkt = cfg->tx_supr_tdpkt;

	/* parse of udmap channel */
	ret = of_k3_nav_udmax_parse_chn(dev->of_node, name,
					&tx_chn->common, true);
	if (ret)
		goto err;

	tx_chn->common.hdesc_size = cppi5_hdesc_calc_size(tx_chn->common.epib,
						tx_chn->common.psdata_size,
						tx_chn->common.swdata_size);

	/* request and cfg UDMAP TX channel */
	tx_chn->udma_tchanx = xudma_tchan_get(tx_chn->common.udmax, -1);
	if (IS_ERR(tx_chn->udma_tchanx)) {
		ret = PTR_ERR(tx_chn->udma_tchanx);
		dev_err(dev, "UDMAX tchanx get err %d\n", ret);
		goto err;
	}
	tx_chn->udma_tchan_id = xudma_tchan_get_id(tx_chn->udma_tchanx);

	atomic_set(&tx_chn->free_pkts, cfg->txcq_cfg.size);

	/* request and cfg rings */
	tx_chn->ringtx = k3_ringacc_request_ring(tx_chn->common.ringacc,
						 tx_chn->udma_tchan_id, 0);
	if (!tx_chn->ringtx) {
		ret = -ENODEV;
		dev_err(dev, "Failed to get TX ring %u\n",
			tx_chn->udma_tchan_id);
		goto err;
	}

	tx_chn->ringtxcq = k3_ringacc_request_ring(tx_chn->common.ringacc,
						   -1, 0);
	if (!tx_chn->ringtxcq) {
		ret = -ENODEV;
		dev_err(dev, "Failed to get TXCQ ring\n");
		goto err;
	}

	ret = k3_ringacc_ring_cfg(tx_chn->ringtx, &cfg->tx_cfg);
	if (ret) {
		dev_err(dev, "Failed to cfg ringtx %d\n", ret);
		goto err;
	}

	ret = k3_ringacc_ring_cfg(tx_chn->ringtxcq, &cfg->txcq_cfg);
	if (ret) {
		dev_err(dev, "Failed to cfg ringtx %d\n", ret);
		goto err;
	}

	/* request and cfg psi-l */
	tx_chn->common.src_thread =
			xudma_dev_get_psil_base(tx_chn->common.udmax) +
			tx_chn->udma_tchan_id;

	tx_chn->need_tisci_free = false;
	ret = k3_nav_udmax_cfg_tx_chn(tx_chn);
	if (ret) {
		dev_err(dev, "Failed to cfg tchan %d\n", ret);
		goto err;
	}

	tx_chn->need_tisci_free = true;

	ret = xudma_navss_psil_pair(tx_chn->common.udmax,
				    tx_chn->common.src_thread,
				    tx_chn->common.dst_thread);
	if (ret) {
		dev_err(dev, "PSI-L request err %d\n", ret);
		goto err;
	}

	tx_chn->psil_paired = true;

	k3_nav_udmax_dump_tx_chn(tx_chn);

	return tx_chn;

err:
	k3_nav_udmax_release_tx_chn(tx_chn);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_request_tx_chn);

void k3_nav_udmax_release_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn)
{
	if (tx_chn->psil_paired) {
		xudma_navss_psil_unpair(tx_chn->common.udmax,
					tx_chn->common.src_thread,
					tx_chn->common.dst_thread);
		tx_chn->psil_paired = false;
	}

	if (!IS_ERR_OR_NULL(tx_chn->common.udmax)) {
		if (tx_chn->need_tisci_free)
			tx_chn->need_tisci_free = false;

		if (!IS_ERR_OR_NULL(tx_chn->udma_tchanx))
			xudma_tchan_put(tx_chn->common.udmax,
					tx_chn->udma_tchanx);

		xudma_dev_put(tx_chn->common.udmax);
	}

	if (tx_chn->ringtxcq)
		k3_ringacc_ring_free(tx_chn->ringtxcq);

	if (tx_chn->ringtx)
		k3_ringacc_ring_free(tx_chn->ringtx);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_release_tx_chn);

int k3_nav_udmax_push_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn,
			     struct cppi5_host_desc_t *desc_tx,
			     dma_addr_t desc_dma)
{
	u32 ringtxcq_id;

	if (!atomic_add_unless(&tx_chn->free_pkts, -1, 0))
		return -ENOMEM;

	ringtxcq_id = k3_ringacc_get_ring_id(tx_chn->ringtxcq);
	cppi5_desc_set_retpolicy(&desc_tx->hdr, 0, ringtxcq_id);

	return k3_ringacc_ring_push(tx_chn->ringtx, &desc_dma);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_push_tx_chn);

int k3_nav_udmax_pop_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn,
			    dma_addr_t *desc_dma)
{
	int ret;

	ret = k3_ringacc_ring_pop(tx_chn->ringtxcq, desc_dma);
	if (!ret)
		atomic_inc(&tx_chn->free_pkts);

	return ret;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_pop_tx_chn);

int k3_nav_udmax_enable_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn)
{
	u32 txrt_ctl;

	txrt_ctl = UDMA_PEER_RT_EN_ENABLE;
	xudma_tchanrt_write(tx_chn->udma_tchanx,
			    UDMA_TCHAN_RT_PEER_RT_EN_REG,
			    txrt_ctl);

	txrt_ctl = xudma_tchanrt_read(tx_chn->udma_tchanx,
				      UDMA_TCHAN_RT_CTL_REG);
	txrt_ctl |= UDMA_CHAN_RT_CTL_EN;
	xudma_tchanrt_write(tx_chn->udma_tchanx, UDMA_TCHAN_RT_CTL_REG,
			    txrt_ctl);

	k3_nav_udmax_dump_tx_rt_chn(tx_chn, "txchn en");
	return 0;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_enable_tx_chn);

void k3_nav_udmax_disable_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn)
{
	k3_nav_udmax_dump_tx_rt_chn(tx_chn, "txchn dis1");

	xudma_tchanrt_write(tx_chn->udma_tchanx, UDMA_TCHAN_RT_CTL_REG, 0);

	xudma_tchanrt_write(tx_chn->udma_tchanx,
			    UDMA_TCHAN_RT_PEER_RT_EN_REG, 0);
	k3_nav_udmax_dump_tx_rt_chn(tx_chn, "txchn dis2");
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_disable_tx_chn);

void k3_nav_udmax_tdown_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn,
			       bool sync)
{
	int i = 0;
	u32 val;

	k3_nav_udmax_dump_tx_rt_chn(tx_chn, "txchn tdown1");

	xudma_tchanrt_write(tx_chn->udma_tchanx, UDMA_TCHAN_RT_CTL_REG,
			    UDMA_CHAN_RT_CTL_EN | UDMA_CHAN_RT_CTL_TDOWN);

	val = xudma_tchanrt_read(tx_chn->udma_tchanx, UDMA_TCHAN_RT_CTL_REG);

	while (sync && (val & UDMA_CHAN_RT_CTL_EN)) {
		val = xudma_tchanrt_read(tx_chn->udma_tchanx,
					 UDMA_TCHAN_RT_CTL_REG);
		udelay(1);
		if (i > K3_UDMAX_TDOWN_TIMEOUT_US) {
			dev_err(tx_chn->common.dev, "TX tdown timeout\n");
			break;
		}
		i++;
	}

	val = xudma_tchanrt_read(tx_chn->udma_tchanx,
				 UDMA_TCHAN_RT_PEER_RT_EN_REG);
	if (sync && (val & UDMA_PEER_RT_EN_ENABLE))
		dev_err(tx_chn->common.dev, "TX tdown peer not stopped\n");
	k3_nav_udmax_dump_tx_rt_chn(tx_chn, "txchn tdown2");
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tdown_tx_chn);

void k3_nav_udmax_reset_tx_chn(struct k3_nav_udmax_tx_channel *tx_chn,
			       void *data,
			       void (*cleanup)(void *data, dma_addr_t desc_dma))
{
	dma_addr_t desc_dma;
	int occ_tx, i, ret;

	/* reset TXCQ as it is not input for udma - expected to be empty */
	if (tx_chn->ringtxcq)
		k3_ringacc_ring_reset(tx_chn->ringtxcq);

	/*
	 * TXQ reset need to be special way as it is input for udma and its
	 * state cached by udma, so:
	 * 1) save TXQ occ
	 * 2) clean up TXQ and call callback .cleanup() for each desc
	 * 3) reset TXQ in a special way
	 */
	occ_tx = k3_ringacc_ring_get_occ(tx_chn->ringtx);
	dev_dbg(tx_chn->common.dev, "TX reset occ_tx %u\n", occ_tx);

	for (i = 0; i < occ_tx; i++) {
		ret = k3_ringacc_ring_pop(tx_chn->ringtx, &desc_dma);
		if (ret) {
			dev_err(tx_chn->common.dev, "TX reset pop %d\n", ret);
			break;
		}
		cleanup(data, desc_dma);
	}

	k3_ringacc_ring_reset_dma(tx_chn->ringtx, occ_tx);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_reset_tx_chn);

u32 k3_nav_udmax_tx_get_hdesc_size(struct k3_nav_udmax_tx_channel *tx_chn)
{
	return tx_chn->common.hdesc_size;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tx_get_hdesc_size);

u32 k3_nav_udmax_tx_get_txcq_id(struct k3_nav_udmax_tx_channel *tx_chn)
{
	return k3_ringacc_get_ring_id(tx_chn->ringtxcq);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tx_get_txcq_id);

int k3_nav_udmax_tx_get_irq(struct k3_nav_udmax_tx_channel *tx_chn,
			    unsigned int *irq, u32 flags, bool share,
			    struct k3_nav_udmax_tx_channel *tx_chn_share)
{
	unsigned int virq = 0;

	if (share && tx_chn_share)
		virq = tx_chn_share->virq;

	tx_chn->virq = ti_sci_inta_register_event(tx_chn->common.dev,
				k3_ringacc_get_tisci_dev_id(tx_chn->ringtxcq),
				k3_ringacc_get_ring_id(tx_chn->ringtxcq), virq,
				false, flags);
	if (tx_chn->virq <= 0)
		return -ENODEV;

	*irq = tx_chn->virq;

	return 0;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tx_get_irq);

void k3_nav_udmax_tx_put_irq(struct k3_nav_udmax_tx_channel *tx_chn)
{
	if (tx_chn->virq <= 0)
		return;

	ti_sci_inta_unregister_event(tx_chn->common.dev,
				k3_ringacc_get_tisci_dev_id(tx_chn->ringtxcq),
				k3_ringacc_get_ring_id(tx_chn->ringtxcq),
				tx_chn->virq);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tx_put_irq);

static int k3_nav_udmax_cfg_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn)
{
	const struct udma_tisci_rm *tisci_rm = rx_chn->common.tisci_rm;
	struct ti_sci_msg_rm_udmap_rx_ch_cfg req;
	int ret;

	memset(&req, 0, sizeof(req));

	req.valid_params = TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID;

	req.nav_id = tisci_rm->tisci_dev_id;
	req.index = rx_chn->udma_rchan_id;
	req.rx_fetch_size = rx_chn->common.hdesc_size >> 2;
	/*
	 * TODO: we can't support rxcq_qnum/RCHAN[a]_RCQ cfg with current sysfw
	 * and udmax impl, so just configure it to invalid value.
	 * req.rxcq_qnum = k3_ringacc_get_ring_id(rx_chn->flows[0].ringrx);
	 */
	req.rxcq_qnum = 0xFFFF;
	if (rx_chn->flow_num && rx_chn->flow_id_base != rx_chn->udma_rchan_id) {
		/* Default flow + extra ones */
		req.flowid_start = rx_chn->flow_id_base;
		req.flowid_cnt = rx_chn->flow_num;
		req.valid_params |=
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_START_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_CNT_VALID;
	}
	req.rx_chan_type = TI_SCI_RM_UDMAP_CHAN_TYPE_PKT_PBRR;

	ret = tisci_rm->tisci_udmap_ops->rx_ch_cfg(tisci_rm->tisci, &req);
	if (ret)
		dev_err(rx_chn->common.dev, "rchan%d cfg failed %d\n",
			rx_chn->udma_rchan_id, ret);

	return ret;
}

static void k3_nav_udmax_release_rx_flow(struct k3_nav_udmax_rx_channel *rx_chn,
					 u32 flow_num)
{
	struct k3_nav_udmax_rx_flow *flow = &rx_chn->flows[flow_num];

	if (IS_ERR_OR_NULL(flow->udma_rflow))
		return;

	if (flow->ringrxfdq)
		k3_ringacc_ring_free(flow->ringrxfdq);

	if (flow->ringrx)
		k3_ringacc_ring_free(flow->ringrx);

	xudma_rflow_put(rx_chn->common.udmax, flow->udma_rflow);
	rx_chn->flows_ready--;
}

static int k3_nav_udmax_cfg_rx_flow(struct k3_nav_udmax_rx_channel *rx_chn,
				    u32 flow_idx,
				    struct k3_nav_udmax_rx_flow_cfg *flow_cfg)
{
	struct k3_nav_udmax_rx_flow *flow = &rx_chn->flows[flow_idx];
	const struct udma_tisci_rm *tisci_rm = rx_chn->common.tisci_rm;
	struct device *dev = rx_chn->common.dev;
	struct ti_sci_msg_rm_udmap_flow_cfg req;
	int rx_ring_id;
	int rx_ringfdq_id;
	int ret = 0;

	flow->udma_rflow = xudma_rflow_get(rx_chn->common.udmax,
					   flow->udma_rflow_id);
	if (IS_ERR(flow->udma_rflow)) {
		ret = PTR_ERR(flow->udma_rflow);
		dev_err(dev, "UDMAX rflow get err %d\n", ret);
		goto err;
	}

	if (flow->udma_rflow_id != xudma_rflow_get_id(flow->udma_rflow)) {
		xudma_rflow_put(rx_chn->common.udmax, flow->udma_rflow);
		return -ENODEV;
	}

	/* request and cfg rings */
	flow->ringrx = k3_ringacc_request_ring(rx_chn->common.ringacc,
					       flow_cfg->ring_rxq_id, 0);
	if (!flow->ringrx) {
		ret = -ENODEV;
		dev_err(dev, "Failed to get RX ring\n");
		goto err;
	}

	flow->ringrxfdq = k3_ringacc_request_ring(rx_chn->common.ringacc,
						  flow_cfg->ring_rxfdq0_id, 0);
	if (!flow->ringrxfdq) {
		ret = -ENODEV;
		dev_err(dev, "Failed to get RXFDQ ring\n");
		goto err;
	}

	ret = k3_ringacc_ring_cfg(flow->ringrx, &flow_cfg->rx_cfg);
	if (ret) {
		dev_err(dev, "Failed to cfg ringrx %d\n", ret);
		goto err;
	}

	ret = k3_ringacc_ring_cfg(flow->ringrxfdq, &flow_cfg->rxfdq_cfg);
	if (ret) {
		dev_err(dev, "Failed to cfg ringrxfdq %d\n", ret);
		goto err;
	}

	rx_ring_id = k3_ringacc_get_ring_id(flow->ringrx);
	rx_ringfdq_id = k3_ringacc_get_ring_id(flow->ringrxfdq);

	memset(&req, 0, sizeof(req));

	req.valid_params =
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_EINFO_PRESENT_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_PSINFO_PRESENT_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_ERROR_HANDLING_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DESC_TYPE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_SRC_TAG_HI_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_SRC_TAG_LO_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_TAG_HI_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_TAG_LO_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ0_SZ0_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ1_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ2_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ3_QNUM_VALID;
	req.nav_id = tisci_rm->tisci_dev_id;
	req.flow_index = flow->udma_rflow_id;
	if (rx_chn->common.epib)
		req.rx_einfo_present = 1;
	if (rx_chn->common.psdata_size)
		req.rx_psinfo_present = 1;
	if (flow_cfg->rx_error_handling)
		req.rx_error_handling = 1;
	req.rx_desc_type = 0;
	req.rx_dest_qnum = rx_ring_id;
	req.rx_src_tag_hi_sel = 0;
	req.rx_src_tag_lo_sel = flow_cfg->src_tag_lo_sel;
	req.rx_dest_tag_hi_sel = 0;
	req.rx_dest_tag_lo_sel = 0;
	req.rx_fdq0_sz0_qnum = rx_ringfdq_id;
	req.rx_fdq1_qnum = rx_ringfdq_id;
	req.rx_fdq2_qnum = rx_ringfdq_id;
	req.rx_fdq3_qnum = rx_ringfdq_id;

	ret = tisci_rm->tisci_udmap_ops->rx_flow_cfg(tisci_rm->tisci, &req);
	if (ret) {
		dev_err(dev, "flow%d config failed: %d\n", flow->udma_rflow_id,
			ret);
		goto err;
	}

	rx_chn->flows_ready++;

	return 0;
err:
	k3_nav_udmax_release_rx_flow(rx_chn, flow_idx);
	return ret;
}

static void k3_nav_udmax_dump_rx_chn(struct k3_nav_udmax_rx_channel *chn)
{
	struct device *dev = chn->common.dev;

	dev_dbg(dev, "dump_rx_chn:\n"
		"udma_rchan_id: %d\n"
		"src_thread: %08x\n"
		"dst_thread: %08x\n"
		"epib: %d\n"
		"hdesc_size: %u\n"
		"psdata_size: %u\n"
		"swdata_size: %u\n"
		"flow_id_base: %d\n"
		"flow_num: %d\n",
		chn->udma_rchan_id,
		chn->common.src_thread,
		chn->common.dst_thread,
		chn->common.epib,
		chn->common.hdesc_size,
		chn->common.psdata_size,
		chn->common.swdata_size,
		chn->flow_id_base,
		chn->flow_num);
}

static void k3_nav_udmax_dump_rx_rt_chn(struct k3_nav_udmax_rx_channel *chn,
					char *mark)
{
	struct device *dev = chn->common.dev;

	dev_dbg(dev, "=== dump ===> %s\n", mark);
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_RCHAN_RT_CTL_REG,
		xudma_rchanrt_read(chn->udma_rchanx, UDMA_RCHAN_RT_CTL_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_RCHAN_RT_PEER_RT_EN_REG,
		xudma_rchanrt_read(chn->udma_rchanx,
				   UDMA_RCHAN_RT_PEER_RT_EN_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_RCHAN_RT_PCNT_REG,
		xudma_rchanrt_read(chn->udma_rchanx, UDMA_RCHAN_RT_PCNT_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_RCHAN_RT_BCNT_REG,
		xudma_rchanrt_read(chn->udma_rchanx, UDMA_RCHAN_RT_BCNT_REG));
	dev_dbg(dev, "0x%08X: %08X\n", UDMA_RCHAN_RT_SBCNT_REG,
		xudma_rchanrt_read(chn->udma_rchanx, UDMA_RCHAN_RT_SBCNT_REG));
}

struct k3_nav_udmax_rx_channel *k3_nav_udmax_request_rx_chn(struct device *dev,
		const char *name, struct k3_nav_udmax_rx_channel_cfg *cfg)
{
	struct k3_nav_udmax_rx_channel *rx_chn;
	int ret, i;

	if (cfg->flow_id_num <= 0)
		return ERR_PTR(-EINVAL);

	if (cfg->flow_id_num != 1 &&
	    (cfg->def_flow_cfg || cfg->flow_id_use_rxchan_id))
		return ERR_PTR(-EINVAL);

	rx_chn = devm_kzalloc(dev, sizeof(*rx_chn), GFP_KERNEL);
	if (!rx_chn)
		return ERR_PTR(-ENOMEM);

	rx_chn->common.dev = dev;
	rx_chn->common.swdata_size = cfg->swdata_size;

	/* parse of udmap channel */
	ret = of_k3_nav_udmax_parse_chn(dev->of_node, name,
					&rx_chn->common, false);
	if (ret)
		goto err;

	rx_chn->common.hdesc_size = cppi5_hdesc_calc_size(rx_chn->common.epib,
						rx_chn->common.psdata_size,
						rx_chn->common.swdata_size);

	/* request and cfg UDMAP RX channel */
	rx_chn->udma_rchanx = xudma_rchan_get(rx_chn->common.udmax, -1);
	if (IS_ERR(rx_chn->udma_rchanx)) {
		ret = PTR_ERR(rx_chn->udma_rchanx);
		dev_err(dev, "UDMAX rchanx get err %d\n", ret);
		goto err;
	}
	rx_chn->udma_rchan_id = xudma_rchan_get_id(rx_chn->udma_rchanx);

	rx_chn->flow_num = cfg->flow_id_num;
	rx_chn->flow_id_base = cfg->flow_id_base;

	/* Use RX channel id as flow id: target dev can't generate flow_id */
	if (cfg->flow_id_use_rxchan_id)
		rx_chn->flow_id_base = rx_chn->udma_rchan_id;

	rx_chn->flows = devm_kcalloc(dev, rx_chn->flow_num,
				     sizeof(*rx_chn->flows), GFP_KERNEL);
	if (!rx_chn->flows) {
		ret = -ENOMEM;
		goto err;
	}

	/* Reserve range of RX flows */
	if (!cfg->flow_id_use_rxchan_id) {
		ret = xudma_reserve_rflow_range(rx_chn->common.udmax,
						rx_chn->flow_id_base,
						rx_chn->flow_num);
		if (ret < 0) {
			dev_err(dev, "UDMAX reserve_rflow get err %d\n", ret);
			goto err;
		}
		rx_chn->flow_id_base = ret;
	}

	for (i = 0; i < rx_chn->flow_num; i++)
		rx_chn->flows[i].udma_rflow_id = rx_chn->flow_id_base + i;

	/* request and cfg psi-l */
	rx_chn->common.dst_thread =
			xudma_dev_get_psil_base(rx_chn->common.udmax) +
			rx_chn->udma_rchan_id;

	rx_chn->need_tisci_free = false;
	ret = k3_nav_udmax_cfg_rx_chn(rx_chn);
	if (ret) {
		dev_err(dev, "Failed to cfg rchan %d\n", ret);
		goto err;
	}
	rx_chn->need_tisci_free = true;

	/* init default RX flow only if flow_num = 1 */
	if (cfg->def_flow_cfg) {
		ret = k3_nav_udmax_cfg_rx_flow(rx_chn, 0, cfg->def_flow_cfg);
		if (ret)
			goto err;
	}

	if (!cfg->skip_psil) {
		ret = xudma_navss_psil_pair(rx_chn->common.udmax,
					    rx_chn->common.src_thread,
					    rx_chn->common.dst_thread);
		if (ret) {
			dev_err(dev, "PSI-L request err %d\n", ret);
			goto err;
		}

		rx_chn->psil_paired = true;
	}

	k3_nav_udmax_dump_rx_chn(rx_chn);

	return rx_chn;

err:
	k3_nav_udmax_release_rx_chn(rx_chn);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_request_rx_chn);

void k3_nav_udmax_release_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn)
{
	int i;

	if (rx_chn->psil_paired) {
		xudma_navss_psil_unpair(rx_chn->common.udmax,
					rx_chn->common.src_thread,
					rx_chn->common.dst_thread);
		rx_chn->psil_paired = false;
	}

	if (IS_ERR_OR_NULL(rx_chn->common.udmax))
		return;

	for (i = 0; i < rx_chn->flow_num; i++)
		k3_nav_udmax_release_rx_flow(rx_chn, i);

	if (rx_chn->need_tisci_free)
		rx_chn->need_tisci_free = false;

	xudma_free_rflow_range(rx_chn->common.udmax,
			       rx_chn->flow_id_base, rx_chn->flow_num);

	if (!IS_ERR_OR_NULL(rx_chn->udma_rchanx))
		xudma_rchan_put(rx_chn->common.udmax,
				rx_chn->udma_rchanx);

	xudma_dev_put(rx_chn->common.udmax);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_release_rx_chn);

int k3_nav_udmax_rx_flow_init(struct k3_nav_udmax_rx_channel *rx_chn,
			      u32 flow_idx,
			      struct k3_nav_udmax_rx_flow_cfg *flow_cfg)
{
	if (flow_idx >= rx_chn->flow_num)
		return -EINVAL;

	return k3_nav_udmax_cfg_rx_flow(rx_chn, flow_idx, flow_cfg);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_rx_flow_init);

u32 k3_nav_udmax_rx_flow_get_fdq_id(struct k3_nav_udmax_rx_channel *rx_chn,
				    u32 flow_idx)
{
	struct k3_nav_udmax_rx_flow *flow;

	if (flow_idx >= rx_chn->flow_num)
		return -EINVAL;

	flow = &rx_chn->flows[flow_idx];

	return k3_ringacc_get_ring_id(flow->ringrxfdq);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_rx_flow_get_fdq_id);

u32 k3_nav_udmax_rx_get_flow_id_base(struct k3_nav_udmax_rx_channel *rx_chn)
{
	return rx_chn->flow_id_base;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_rx_get_flow_id_base);

int k3_nav_udmax_enable_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn)
{
	u32 rxrt_ctl;

	if (rx_chn->flows_ready < rx_chn->flow_num)
		return -EINVAL;

	rxrt_ctl = xudma_rchanrt_read(rx_chn->udma_rchanx,
				      UDMA_RCHAN_RT_CTL_REG);
	rxrt_ctl |= UDMA_CHAN_RT_CTL_EN;
	xudma_rchanrt_write(rx_chn->udma_rchanx, UDMA_RCHAN_RT_CTL_REG,
			    rxrt_ctl);

	xudma_rchanrt_write(rx_chn->udma_rchanx,
			    UDMA_RCHAN_RT_PEER_RT_EN_REG,
			    UDMA_PEER_RT_EN_ENABLE);

	k3_nav_udmax_dump_rx_rt_chn(rx_chn, "rxrt en");
	return 0;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_enable_rx_chn);

void k3_nav_udmax_disable_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn)
{
	k3_nav_udmax_dump_rx_rt_chn(rx_chn, "rxrt dis1");

	xudma_rchanrt_write(rx_chn->udma_rchanx,
			    UDMA_RCHAN_RT_PEER_RT_EN_REG,
			    0);
	xudma_rchanrt_write(rx_chn->udma_rchanx, UDMA_RCHAN_RT_CTL_REG, 0);

	k3_nav_udmax_dump_rx_rt_chn(rx_chn, "rxrt dis2");
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_disable_rx_chn);

void k3_nav_udmax_tdown_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn,
			       bool sync)
{
	int i = 0;
	u32 val;

	k3_nav_udmax_dump_rx_rt_chn(rx_chn, "rxrt tdown1");

	xudma_rchanrt_write(rx_chn->udma_rchanx, UDMA_RCHAN_RT_PEER_RT_EN_REG,
			    UDMA_PEER_RT_EN_ENABLE | UDMA_PEER_RT_EN_TEARDOWN);

	val = xudma_rchanrt_read(rx_chn->udma_rchanx, UDMA_RCHAN_RT_CTL_REG);

	while (sync && (val & UDMA_CHAN_RT_CTL_EN)) {
		val = xudma_rchanrt_read(rx_chn->udma_rchanx,
					 UDMA_RCHAN_RT_CTL_REG);
		udelay(1);
		if (i > K3_UDMAX_TDOWN_TIMEOUT_US) {
			dev_err(rx_chn->common.dev, "RX tdown timeout\n");
			break;
		}
		i++;
	}

	val = xudma_rchanrt_read(rx_chn->udma_rchanx,
				 UDMA_RCHAN_RT_PEER_RT_EN_REG);
	if (sync && (val & UDMA_PEER_RT_EN_ENABLE))
		dev_err(rx_chn->common.dev, "TX tdown peer not stopped\n");
	k3_nav_udmax_dump_rx_rt_chn(rx_chn, "rxrt tdown2");
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_tdown_rx_chn);

void k3_nav_udmax_reset_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn,
			       u32 flow_num, void *data,
			       void (*cleanup)(void *data, dma_addr_t desc_dma),
			       bool skip_fdq)
{
	struct k3_nav_udmax_rx_flow *flow = &rx_chn->flows[flow_num];
	struct device *dev = rx_chn->common.dev;
	dma_addr_t desc_dma;
	int occ_rx, i, ret;

	/* reset RXCQ as it is not input for udma - expected to be empty */
	if (flow->ringrx)
		k3_ringacc_ring_reset(flow->ringrx);

	/* Skip RX FDQ in case one FDQ is used for the set of flows */
	if (skip_fdq)
		return;

	/*
	 * RX FDQ reset need to be special way as it is input for udma and its
	 * state cached by udma, so:
	 * 1) save RX FDQ occ
	 * 2) clean up RX FDQ and call callback .cleanup() for each desc
	 * 3) reset RX FDQ in a special way
	 */
	occ_rx = k3_ringacc_ring_get_occ(flow->ringrxfdq);
	dev_dbg(dev, "RX reset flow %u occ_tx %u\n", flow_num, occ_rx);

	for (i = 0; i < occ_rx; i++) {
		ret = k3_ringacc_ring_pop(flow->ringrxfdq, &desc_dma);
		if (ret) {
			dev_err(dev, "RX reset pop %d\n", ret);
			break;
		}
		cleanup(data, desc_dma);
	}

	k3_ringacc_ring_reset_dma(flow->ringrxfdq, occ_rx);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_reset_rx_chn);

int k3_nav_udmax_push_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn,
			     u32 flow_num, struct cppi5_host_desc_t *desc_rx,
			     dma_addr_t desc_dma)
{
	struct k3_nav_udmax_rx_flow *flow = &rx_chn->flows[flow_num];

	return k3_ringacc_ring_push(flow->ringrxfdq, &desc_dma);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_push_rx_chn);

int k3_nav_udmax_pop_rx_chn(struct k3_nav_udmax_rx_channel *rx_chn,
			    u32 flow_num, dma_addr_t *desc_dma)
{
	struct k3_nav_udmax_rx_flow *flow = &rx_chn->flows[flow_num];

	return k3_ringacc_ring_pop(flow->ringrx, desc_dma);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_pop_rx_chn);

int k3_nav_udmax_rx_get_irq(struct k3_nav_udmax_rx_channel *rx_chn,
			    u32 flow_num,
			    unsigned int *irq, u32 flags, bool share,
			    u32 flow_num_share)
{
	struct k3_nav_udmax_rx_flow *flow, *flow_share;
	unsigned int virq = 0;

	if (flow_num >= rx_chn->flow_num ||
	    (flow_num_share != -1 && flow_num_share >= rx_chn->flow_num))
		return -EINVAL;

	flow = &rx_chn->flows[flow_num];

	if (share && flow_num_share != -1) {
		flow_share = &rx_chn->flows[flow_num_share];
		virq = flow_share->virq;
	}

	flow->virq = ti_sci_inta_register_event(rx_chn->common.dev,
				k3_ringacc_get_tisci_dev_id(flow->ringrx),
				k3_ringacc_get_ring_id(flow->ringrx), virq,
				false, flags);
	if (flow->virq <= 0)
		return -ENODEV;

	*irq = flow->virq;

	return 0;
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_rx_get_irq);

void k3_nav_udmax_rx_put_irq(struct k3_nav_udmax_rx_channel *rx_chn,
			     u32 flow_num)
{
	struct k3_nav_udmax_rx_flow *flow;

	if (flow_num >= rx_chn->flow_num)
		return;

	flow = &rx_chn->flows[flow_num];

	if (flow->virq <= 0)
		return;

	ti_sci_inta_unregister_event(rx_chn->common.dev,
				     k3_ringacc_get_tisci_dev_id(flow->ringrx),
				     k3_ringacc_get_ring_id(flow->ringrx),
				     flow->virq);
}
EXPORT_SYMBOL_GPL(k3_nav_udmax_rx_put_irq);

MODULE_DESCRIPTION("TI K3 UDMA glue layer for non DMAengine clients");
MODULE_AUTHOR("Grygorii Strashko <grygorii.strashko@ti.com>");
MODULE_LICENSE("GPL v2");
