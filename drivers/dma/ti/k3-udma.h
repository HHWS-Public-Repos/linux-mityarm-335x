/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (C) 2018 Texas Instruments Incorporated - http://www.ti.com
 */

#ifndef K3_UDMA_H_
#define K3_UDMA_H_

#define UDMA_PSIL_DST_THREAD_ID_OFFSET 0x8000

/* Global registers */
#define UDMA_REV_REG			0x0
#define UDMA_PERF_CTL_REG		0x4
#define UDMA_EMU_CTL_REG		0x8
#define UDMA_PSIL_TO_REG		0x10
#define UDMA_UTC_CTL_REG		0x1c
#define UDMA_CAP_REG(i)			(0x20 + ((i) * 4))
#define UDMA_RX_FLOW_ID_FW_OES_REG	0x80
#define UDMA_RX_FLOW_ID_FW_STATUS_REG	0x88

/* TX chan RT regs */
#define UDMA_TCHAN_RT_CTL_REG		0x0
#define UDMA_TCHAN_RT_SWTRIG_REG	0x8
#define UDMA_TCHAN_RT_STDATA_REG	0x80

#define UDMA_TCHAN_RT_PEERn_REG(i)	(0x200 + ((i) * 0x4))
#define UDMA_TCHAN_RT_PEER_STATIC_TR_XY_REG	\
	UDMA_TCHAN_RT_PEERn_REG(0)	/* PSI-L: 0x400 */
#define UDMA_TCHAN_RT_PEER_STATIC_TR_Z_REG	\
	UDMA_TCHAN_RT_PEERn_REG(1)	/* PSI-L: 0x401 */
#define UDMA_TCHAN_RT_PEER_BCNT_REG		\
	UDMA_TCHAN_RT_PEERn_REG(4)	/* PSI-L: 0x404 */
#define UDMA_TCHAN_RT_PEER_RT_EN_REG		\
	UDMA_TCHAN_RT_PEERn_REG(8)	/* PSI-L: 0x408 */

#define UDMA_TCHAN_RT_PCNT_REG		0x400
#define UDMA_TCHAN_RT_BCNT_REG		0x408
#define UDMA_TCHAN_RT_SBCNT_REG		0x410

/* RX chan RT regs */
#define UDMA_RCHAN_RT_CTL_REG		0x0
#define UDMA_RCHAN_RT_SWTRIG_REG	0x8
#define UDMA_RCHAN_RT_STDATA_REG	0x80

#define UDMA_RCHAN_RT_PEERn_REG(i)	(0x200 + ((i) * 0x4))
#define UDMA_RCHAN_RT_PEER_STATIC_TR_XY_REG	\
	UDMA_RCHAN_RT_PEERn_REG(0)	/* PSI-L: 0x400 */
#define UDMA_RCHAN_RT_PEER_STATIC_TR_Z_REG	\
	UDMA_RCHAN_RT_PEERn_REG(1)	/* PSI-L: 0x401 */
#define UDMA_RCHAN_RT_PEER_BCNT_REG		\
	UDMA_RCHAN_RT_PEERn_REG(4)	/* PSI-L: 0x404 */
#define UDMA_RCHAN_RT_PEER_RT_EN_REG		\
	UDMA_RCHAN_RT_PEERn_REG(8)	/* PSI-L: 0x408 */

#define UDMA_RCHAN_RT_PCNT_REG		0x400
#define UDMA_RCHAN_RT_BCNT_REG		0x408
#define UDMA_RCHAN_RT_SBCNT_REG		0x410

/* UDMA_TCHAN_RT_CTL_REG/UDMA_RCHAN_RT_CTL_REG */
#define UDMA_CHAN_RT_CTL_EN	BIT(31)
#define UDMA_CHAN_RT_CTL_TDOWN	BIT(30)
#define UDMA_CHAN_RT_CTL_PAUSE	BIT(29)
#define UDMA_CHAN_RT_CTL_FTDOWN	BIT(28)
#define UDMA_CHAN_RT_CTL_ERROR	BIT(0)

/* UDMA_TCHAN_RT_PEER_RT_EN_REG/UDMA_RCHAN_RT_PEER_RT_EN_REG (PSI-L: 0x408) */
#define UDMA_PEER_RT_EN_ENABLE		BIT(31)
#define UDMA_PEER_RT_EN_TEARDOWN	BIT(30)
#define UDMA_PEER_RT_EN_PAUSE		BIT(29)
#define UDMA_PEER_RT_EN_FLUSH		BIT(28)
#define UDMA_PEER_RT_EN_IDLE		BIT(1)

/*
 * UDMA_TCHAN_RT_PEER_STATIC_TR_XY_REG /
 * UDMA_RCHAN_RT_PEER_STATIC_TR_XY_REG
 */
#define PDMA_STATIC_TR_X_MASK		GENMASK(26, 24)
#define PDMA_STATIC_TR_X_SHIFT		(24)
#define PDMA_STATIC_TR_Y_MASK		GENMASK(11, 0)
#define PDMA_STATIC_TR_Y_SHIFT		(0)

#define PDMA_STATIC_TR_Y(x)	\
	(((x) << PDMA_STATIC_TR_Y_SHIFT) & PDMA_STATIC_TR_Y_MASK)
#define PDMA_STATIC_TR_X(x)	\
	(((x) << PDMA_STATIC_TR_X_SHIFT) & PDMA_STATIC_TR_X_MASK)

/*
 * UDMA_TCHAN_RT_PEER_STATIC_TR_Z_REG /
 * UDMA_RCHAN_RT_PEER_STATIC_TR_Z_REG
 */
#define PDMA_STATIC_TR_Z_MASK		GENMASK(11, 0)
#define PDMA_STATIC_TR_Z_SHIFT		(0)
#define PDMA_STATIC_TR_Z(x)	\
	(((x) << PDMA_STATIC_TR_Z_SHIFT) & PDMA_STATIC_TR_Z_MASK)

/* Private API to UDMA via k3-udma-private */
#include <linux/of.h>
#include <linux/dmaengine.h>
#include <linux/soc/ti/ti_sci_protocol.h>

struct udma_dev;
struct udma_tchan;
struct udma_rchan;
struct udma_rflow;

enum udma_rm_range {
	RM_RANGE_TCHAN = 0,
	RM_RANGE_RCHAN,
	RM_RANGE_RFLOW,
	RM_RANGE_LAST,
};

/* Channel Throughput Levels */
enum udma_tp_level {
	UDMA_TP_NORMAL = 0,
	UDMA_TP_HIGH = 1,
	UDMA_TP_LAST,
};

struct udma_tisci_rm {
	const struct ti_sci_handle *tisci;
	const struct ti_sci_rm_udmap_ops *tisci_udmap_ops;
	u32  tisci_dev_id;

	/* tisci information for PSI-L thread pairing/unpairing */
	const struct ti_sci_rm_psil_ops *tisci_psil_ops;
	u32  tisci_navss_dev_id;

	struct ti_sci_resource *rm_ranges[RM_RANGE_LAST];
};

/* HACK: Direct access to UDMA resources */
int xudma_navss_psil_pair(struct udma_dev *ud, u32 src_thread, u32 dst_thread);
int xudma_navss_psil_unpair(struct udma_dev *ud, u32 src_thread,
			    u32 dst_thread);

struct udma_dev *of_xudma_dev_get(struct device_node *np, const char *property);
void xudma_dev_put(struct udma_dev *ud);
u32 xudma_dev_get_psil_base(struct udma_dev *ud);
struct udma_tisci_rm *xudma_dev_get_tisci_rm(struct udma_dev *ud);

int xudma_reserve_rflow_range(struct udma_dev *ud, int from, int cnt);
int xudma_free_rflow_range(struct udma_dev *ud, int from, int cnt);

struct udma_tchan *xudma_tchan_get(struct udma_dev *ud, int id);
struct udma_rchan *xudma_rchan_get(struct udma_dev *ud, int id);
struct udma_rflow *xudma_rflow_get(struct udma_dev *ud, int id);

void xudma_tchan_put(struct udma_dev *ud, struct udma_tchan *p);
void xudma_rchan_put(struct udma_dev *ud, struct udma_rchan *p);
void xudma_rflow_put(struct udma_dev *ud, struct udma_rflow *p);

int xudma_tchan_get_id(struct udma_tchan *p);
int xudma_rchan_get_id(struct udma_rchan *p);
int xudma_rflow_get_id(struct udma_rflow *p);

u32 xudma_tchanrt_read(struct udma_tchan *tchan, int reg);
void xudma_tchanrt_write(struct udma_tchan *tchan, int reg, u32 val);
u32 xudma_rchanrt_read(struct udma_rchan *rchan, int reg);
void xudma_rchanrt_write(struct udma_rchan *rchan, int reg, u32 val);

#endif /* K3_UDMA_H_ */
