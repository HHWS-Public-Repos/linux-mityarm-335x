# SPDX-License-Identifier: GPL-2.0
#
# Generic framework for controlling remote processors
#

obj-$(CONFIG_REMOTEPROC)		+= remoteproc.o
remoteproc-y				:= remoteproc_core.o
remoteproc-y				+= remoteproc_debugfs.o
remoteproc-y				+= remoteproc_sysfs.o
remoteproc-y				+= remoteproc_virtio.o
remoteproc-y				+= remoteproc_elf_loader.o
obj-$(CONFIG_IMX_REMOTEPROC)		+= imx_rproc.o
obj-$(CONFIG_OMAP_REMOTEPROC)		+= omap_remoteproc.o
obj-$(CONFIG_WKUP_M3_RPROC)		+= wkup_m3_rproc.o
obj-$(CONFIG_DA8XX_REMOTEPROC)		+= da8xx_remoteproc.o
obj-$(CONFIG_KEYSTONE_REMOTEPROC)	+= keystone_remoteproc.o
obj-$(CONFIG_PRU_REMOTEPROC)		+= pru_rproc.o
obj-$(CONFIG_QCOM_ADSP_PIL)		+= qcom_adsp_pil.o
obj-$(CONFIG_QCOM_RPROC_COMMON)		+= qcom_common.o
obj-$(CONFIG_QCOM_Q6V5_COMMON)		+= qcom_q6v5.o
obj-$(CONFIG_QCOM_Q6V5_PIL)		+= qcom_q6v5_pil.o
obj-$(CONFIG_QCOM_Q6V5_WCSS)		+= qcom_q6v5_wcss.o
obj-$(CONFIG_QCOM_SYSMON)		+= qcom_sysmon.o
obj-$(CONFIG_QCOM_WCNSS_PIL)		+= qcom_wcnss_pil.o
qcom_wcnss_pil-y			+= qcom_wcnss.o
qcom_wcnss_pil-y			+= qcom_wcnss_iris.o
obj-$(CONFIG_ST_REMOTEPROC)		+= st_remoteproc.o
obj-$(CONFIG_ST_SLIM_REMOTEPROC)	+= st_slim_rproc.o
obj-$(CONFIG_TI_K3_R5_REMOTEPROC)	+= ti_k3_r5_remoteproc.o
