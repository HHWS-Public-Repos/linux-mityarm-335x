# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the TI network device drivers.
#

obj-$(CONFIG_TI_CPSW) += cpsw-common.o
obj-$(CONFIG_TI_DAVINCI_EMAC) += cpsw-common.o

obj-$(CONFIG_TLAN) += tlan.o
obj-$(CONFIG_CPMAC) += cpmac.o
obj-$(CONFIG_TI_DAVINCI_EMAC) += davinci_emac.o
obj-$(CONFIG_TI_DAVINCI_MDIO) += davinci_mdio.o
obj-$(CONFIG_TI_DAVINCI_CPDMA) += davinci_cpdma.o
obj-$(CONFIG_TI_CPSW_PHY_SEL) += cpsw-phy-sel.o
obj-$(CONFIG_TI_CPSW_ALE) += cpsw_ale.o
obj-$(CONFIG_TI_CPTS_MOD) += cpts.o
obj-$(CONFIG_TI_PTP_BC) += ptp_bc.o
obj-$(CONFIG_TI_CPSW) += ti_cpsw.o
ti_cpsw-y := cpsw.o

obj-$(CONFIG_TI_KEYSTONE_NETCP) += keystone_netcp.o
keystone_netcp-y := netcp_core.o
obj-$(CONFIG_TI_KEYSTONE_NETCP_ETHSS) += keystone_netcp_ethss.o
keystone_netcp_ethss-y := netcp_ethss.o netcp_ethss_sysfs.o netcp_sgmii.o

obj-$(CONFIG_TI_AM65_CPSW_NUSS) += ti-am65-cpsw-nuss.o
ti-am65-cpsw-nuss-y := am65-cpsw-nuss.o cpsw_sl.o am65-cpsw-ethtool.o
obj-$(CONFIG_TI_AM65_CPTS) += am65-cpts.o

obj-$(CONFIG_TI_PRUETH) += ti_prueth.o
ti_prueth-y := prueth.o iep.o prueth_node_tbl.o
ti_prueth-$(CONFIG_SYSFS) += prueth_sysfs.o
ti_prueth-$(CONFIG_DEBUG_FS) += prueth_dbgfs.o
obj-$(CONFIG_TI_ICSSG_PRUETH) += icssg-prueth.o
icssg-prueth-y := icssg_prueth.o icssg_classifier.o icssg_ethtool.o icssg_iep.o
