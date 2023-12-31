/*
 * TI Common Platform Time Sync
 *
 * Copyright (C) 2012 Richard Cochran <richardcochran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _TI_CPTS_H_
#define _TI_CPTS_H_

#if IS_ENABLED(CONFIG_TI_CPTS)

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clocksource.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_TI_1PPS_DM_TIMER
#include <linux/gpio.h>
#endif
#include <linux/ptp_clock_kernel.h>
#include <linux/skbuff.h>
#include <linux/ptp_classify.h>
#include <linux/timecounter.h>
#ifdef CONFIG_TI_1PPS_DM_TIMER
#include <linux/kthread.h>
#include <clocksource/timer-ti-dm.h>
#endif

struct cpsw_cpts {
	u32 idver;                /* Identification and version */
	u32 control;              /* Time sync control */
	u32 rftclk_sel;		  /* Reference Clock Select Register */
	u32 ts_push;              /* Time stamp event push */
	u32 ts_load_val;          /* Time stamp load value */
	u32 ts_load_en;           /* Time stamp load enable */
	u32 res2[2];
	u32 intstat_raw;          /* Time sync interrupt status raw */
	u32 intstat_masked;       /* Time sync interrupt status masked */
	u32 int_enable;           /* Time sync interrupt enable */
	u32 res3;
	u32 event_pop;            /* Event interrupt pop */
	u32 event_low;            /* 32 Bit Event Time Stamp */
	u32 event_high;           /* Event Type Fields */
};

/* Bit definitions for the IDVER register */
#define TX_IDENT_SHIFT       (16)    /* TX Identification Value */
#define TX_IDENT_MASK        (0xffff)
#define RTL_VER_SHIFT        (11)    /* RTL Version Value */
#define RTL_VER_MASK         (0x1f)
#define MAJOR_VER_SHIFT      (8)     /* Major Version Value */
#define MAJOR_VER_MASK       (0x7)
#define MINOR_VER_SHIFT      (0)     /* Minor Version Value */
#define MINOR_VER_MASK       (0xff)

/* Bit definitions for the CONTROL register */
#define HW4_TS_PUSH_EN       (1<<11) /* Hardware push 4 enable */
#define HW3_TS_PUSH_EN       (1<<10) /* Hardware push 3 enable */
#define HW2_TS_PUSH_EN       (1<<9)  /* Hardware push 2 enable */
#define HW1_TS_PUSH_EN       (1<<8)  /* Hardware push 1 enable */
#define INT_TEST             (1<<1)  /* Interrupt Test */
#define CPTS_EN              (1<<0)  /* Time Sync Enable */

#define CPTS_RFTCLK_SEL_MASK 0x1f

/*
 * Definitions for the single bit resisters:
 * TS_PUSH TS_LOAD_EN  INTSTAT_RAW INTSTAT_MASKED INT_ENABLE EVENT_POP
 */
#define TS_PUSH             (1<<0)  /* Time stamp event push */
#define TS_LOAD_EN          (1<<0)  /* Time Stamp Load */
#define TS_PEND_RAW         (1<<0)  /* int read (before enable) */
#define TS_PEND             (1<<0)  /* masked interrupt read (after enable) */
#define TS_PEND_EN          (1<<0)  /* masked interrupt enable */
#define EVENT_POP           (1<<0)  /* writing discards one event */

/* Bit definitions for the EVENT_HIGH register */
#define PORT_NUMBER_SHIFT    (24)    /* Indicates Ethernet port or HW pin */
#define PORT_NUMBER_MASK     (0x1f)
#define EVENT_TYPE_SHIFT     (20)    /* Time sync event type */
#define EVENT_TYPE_MASK      (0xf)
#define MESSAGE_TYPE_SHIFT   (16)    /* PTP message type */
#define MESSAGE_TYPE_MASK    (0xf)
#define SEQUENCE_ID_SHIFT    (0)     /* PTP message sequence ID */
#define SEQUENCE_ID_MASK     (0xffff)

enum {
	CPTS_EV_PUSH, /* Time Stamp Push Event */
	CPTS_EV_ROLL, /* Time Stamp Rollover Event */
	CPTS_EV_HALF, /* Time Stamp Half Rollover Event */
	CPTS_EV_HW,   /* Hardware Time Stamp Push Event */
	CPTS_EV_RX,   /* Ethernet Receive Event */
	CPTS_EV_TX,   /* Ethernet Transmit Event */
};

#define CPTS_FIFO_DEPTH 16
#define CPTS_MAX_EVENTS 32

#define CPTS_EVENT_RX_TX_TIMEOUT 20 /* ms */
#define CPTS_EVENT_HWSTAMP_TIMEOUT 200 /* ms */

#define CPTS_MAX_EXT_TS 4
#define CPTS_PPS_HW_INDEX 3

struct cpts_event {
	struct list_head list;
	unsigned long tmo;
	u32 high;
	u32 low;
};

#define CPTS_CAP_RFTCLK_SEL BIT(0)

struct cpts {
	struct device *dev;
	struct cpsw_cpts __iomem *reg;
	int tx_enable;
	int rx_enable;
	struct ptp_clock_info info;
	struct ptp_clock *clock;
	spinlock_t lock; /* protects time registers */
	u32 cc_mult; /* for the nominal frequency */
	struct cyclecounter cc;
	struct timecounter tc;
	int phc_index;
	struct clk *refclk;
	struct list_head events;
	struct list_head pool;
	struct cpts_event pool_data[CPTS_MAX_EVENTS];
	unsigned long ov_check_period;
	struct sk_buff_head txq;
	unsigned long ov_check_period_slow;
	u32 rftclk_sel;
	u32 ext_ts_inputs;
	u32 hw_ts_enable;
	u32 caps;

#ifdef CONFIG_TI_1PPS_DM_TIMER
	u8 use_1pps_gen;
	u8 use_1pps_latch;
	u8 use_1pps_ref;
	u8 pps_latch_receive;
	int pps_hw_index;
	int pps_enable;
	int pps_state;
	int pps_latch_state;
	int ref_enable;
	struct omap_dm_timer *odt;/* timer for 1PPS generator */
	struct omap_dm_timer *odt2;/* timer for 1PPS latch */
	u32 count_prev;
	u64 hw_timestamp;
	u32 pps_latch_offset;
	int pps_offset;
	spinlock_t bc_mux_lock; /* protect mux control gpio (pps_enable_gpio) */

	struct pinctrl *pins;
	struct pinctrl_state *pin_state_pwm_off;
	struct pinctrl_state *pin_state_pwm_on;
	struct pinctrl_state *pin_state_ref_off;
	struct pinctrl_state *pin_state_ref_on;
	struct pinctrl_state *pin_state_latch_off;
	struct pinctrl_state *pin_state_latch_on;

	int pps_enable_gpio;
	int ref_enable_gpio;

	int pps_tmr_irqn;
	int pps_latch_irqn;
	int bc_clkid;

	struct kthread_worker *pps_kworker;
	struct kthread_delayed_work pps_work;
#endif
};

int cpts_rx_timestamp(struct cpts *cpts, struct sk_buff *skb);
int cpts_tx_timestamp(struct cpts *cpts, struct sk_buff *skb);
int cpts_register(struct cpts *cpts);
void cpts_unregister(struct cpts *cpts);
struct cpts *cpts_create(struct device *dev, void __iomem *regs,
			 struct device_node *node);
void cpts_release(struct cpts *cpts);

static inline bool cpts_can_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	unsigned int class = ptp_classify_raw(skb);

	if (class == PTP_CLASS_NONE)
		return false;

	return true;
}

#else
struct cpts;

static inline int cpts_rx_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	return -EOPNOTSUPP;
}

static inline int cpts_tx_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	return -EOPNOTSUPP;
}

static inline
struct cpts *cpts_create(struct device *dev, void __iomem *regs,
			 struct device_node *node)
{
	return NULL;
}

static inline void cpts_release(struct cpts *cpts)
{
}

static inline int
cpts_register(struct cpts *cpts)
{
	return 0;
}

static inline void cpts_unregister(struct cpts *cpts)
{
}

static inline bool cpts_can_timestamp(struct cpts *cpts, struct sk_buff *skb)
{
	return false;
}
#endif


#endif
