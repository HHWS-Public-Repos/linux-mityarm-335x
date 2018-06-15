/*
 * Critical Link MityARM-335x SoM Test Fixture Baseboard Initialization File
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/phy.h>
#include <linux/usb/musb.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <video/da8xx-fb.h>
#include <plat/lcdc.h>
#include <plat/mmc.h>
#include <plat/usb.h>
#include <plat/omap_device.h>
#include <plat/mcspi.h>
#include <plat/i2c.h>
#include <plat/nand.h>
#include <plat/board.h>
#include <plat/gpmc.h>

#include <asm/hardware/asp.h>

#include "board-flash.h"
#include "mux.h"
#include "hsmmc.h"
#include "devices.h"

#define BASEBOARD_NAME "MitySOM-335x TestFixture"
/* Vitesse 8601 register defs */
#define VSC8601_PHY_ID   (0x00070420)
#define VSC8601_PHY_MASK (0xFFFFFFFC)
#define MII_EXTPAGE		 (0x1F)
#define RGMII_SKEW		 (0x1C)


#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

struct pinmux_config {
	const char *muxname;
	int val;
};


#define setup_pin_mux(pin_mux) \
{ \
	int i = 0; \
	for (; pin_mux[i].muxname != NULL; i++) \
		omap_mux_init_signal(pin_mux[i].muxname, pin_mux[i].val); \
}

/* Signal set A - loopbacks */
static struct pinmux_config sig_setA_loopback_pin_mux[] = {
	/* Output pins */
	{"lcd_data0.gpio2_6",			AM33XX_PIN_OUTPUT},
	{"lcd_data1.gpio2_7",			AM33XX_PIN_OUTPUT},
	{"lcd_data2.gpio2_8",			AM33XX_PIN_OUTPUT},
	{"lcd_data3.gpio2_9",			AM33XX_PIN_OUTPUT},
	{"lcd_data4.gpio2_10",			AM33XX_PIN_OUTPUT},
	{"lcd_data5.gpio2_11",			AM33XX_PIN_OUTPUT},
	{"lcd_data6.gpio2_12",			AM33XX_PIN_OUTPUT},
	{"lcd_data7.gpio2_13",			AM33XX_PIN_OUTPUT},
	{"lcd_data8.gpio2_14",			AM33XX_PIN_OUTPUT},
	{"lcd_data9.gpio2_15",			AM33XX_PIN_OUTPUT},
	{"lcd_data10.gpio2_16",			AM33XX_PIN_OUTPUT},
	{"lcd_data11.gpio2_17",			AM33XX_PIN_OUTPUT},
	{"lcd_data12.gpio0_8",			AM33XX_PIN_OUTPUT},
	{"lcd_data13.gpio0_9",			AM33XX_PIN_OUTPUT},
	{"lcd_data14.gpio0_10",			AM33XX_PIN_OUTPUT},
	{"lcd_data15.gpio0_11",			AM33XX_PIN_OUTPUT},

	/* Input pins */
	{"lcd_pclk.gpio2_24",			AM33XX_PIN_INPUT},
	{"lcd_vsync.gpio2_22",			AM33XX_PIN_INPUT},
	{"lcd_hsync.gpio2_23",			AM33XX_PIN_INPUT},
	{"lcd_ac_bias_en.gpio2_25",		AM33XX_PIN_INPUT},
	{"gpmc_ad8.gpio0_22",			AM33XX_PIN_INPUT},
	{"gpmc_ad9.gpio0_23",			AM33XX_PIN_INPUT},
	{"gpmc_ad10.gpio0_26",			AM33XX_PIN_INPUT},
	{"gpmc_ad11.gpio0_27",			AM33XX_PIN_INPUT},
	{"gpmc_ad12.gpio1_12",			AM33XX_PIN_INPUT},
	{"gpmc_ad13.gpio1_13",			AM33XX_PIN_INPUT},
	{"gpmc_ad14.gpio1_14",			AM33XX_PIN_INPUT},
	{"gpmc_ad15.gpio1_15",			AM33XX_PIN_INPUT},
	{"gpmc_csn2.gpio1_31",			AM33XX_PIN_INPUT},
	{"gpmc_csn1.gpio1_30",			AM33XX_PIN_INPUT},
	{"uart0_ctsn.gpio1_8",			AM33XX_PIN_INPUT},
	{"uart0_rtsn.gpio1_9",			AM33XX_PIN_INPUT},

	{NULL, 0}
};

/* Signal set B - loopbacks */
static struct pinmux_config sig_setB_loopback_pin_mux[] = {
	/* Outputs */
	{"rmii1_refclk.gpio0_29",		AM33XX_PIN_OUTPUT},
	{"uart1_txd.gpio0_15",			AM33XX_PIN_OUTPUT},
	{"i2c0_scl.gpio3_6",			AM33XX_PIN_OUTPUT},
	{"spi0_d1.gpio0_4",			AM33XX_PIN_OUTPUT},
	{"spi0_cs1.gpio0_6",			AM33XX_PIN_OUTPUT},
	{"xdma_event_intr1.gpio0_20",		AM33XX_PIN_OUTPUT},
	{"gpmc_clk.gpio2_1",			AM33XX_PIN_OUTPUT},
	{"gpmc_a2.gpio1_18",			AM33XX_PIN_OUTPUT},
	{"gpmc_a4.gpio1_20",			AM33XX_PIN_OUTPUT},
	{"gpmc_a6.gpio1_22",			AM33XX_PIN_OUTPUT},
	{"gpmc_a8.gpio1_24",			AM33XX_PIN_OUTPUT},
	{"gpmc_a10.gpio1_26",			AM33XX_PIN_OUTPUT},
	{"mcasp0_fsr.gpio3_19",			AM33XX_PIN_OUTPUT},
	{"mcasp0_aclkx.gpio3_14",		AM33XX_PIN_OUTPUT},

	/* Inputs */
	{"uart1_rxd.gpio0_14",			AM33XX_PIN_INPUT},
	{"i2c0_sda.gpio3_5",			AM33XX_PIN_INPUT},
	{"spi0_d0.gpio0_3",			AM33XX_PIN_INPUT},
	{"spi0_sclk.gpio0_2",			AM33XX_PIN_INPUT},
	{"spi0_cs0.gpio0_5",			AM33XX_PIN_INPUT},
	{"gpmc_ben1.gpio1_28",			AM33XX_PIN_INPUT},
	{"gpmc_a3.gpio1_19",			AM33XX_PIN_INPUT},
	{"gpmc_a5.gpio1_21",			AM33XX_PIN_INPUT},
	{"gpmc_a7.gpio1_23",			AM33XX_PIN_INPUT},
	{"gpmc_a9.gpio1_25",			AM33XX_PIN_INPUT},
	{"gpmc_a11.gpio1_27",			AM33XX_PIN_INPUT},
	{"mcasp0_aclkr.gpio3_18",		AM33XX_PIN_INPUT},
	{"mcasp0_ahclkx.gpio3_21",		AM33XX_PIN_INPUT},
	{"mcasp0_axr1.gpio3_20",		AM33XX_PIN_INPUT},

	/* Others: */
	/* gpmc_a0 is output to control the USB_ID pins */
	{"gpmc_a0.gpio1_16",			AM33XX_PIN_OUTPUT},

	/* MMC pins may not be GPIO after respin... */
	/* TODO: MMC Support? Enable MMC support by removing these: */
	{"mmc0_dat0.gpio2_29",			AM33XX_PIN_OUTPUT},
	{"mmc0_dat2.gpio2_27",			AM33XX_PIN_OUTPUT},
	{"mmc0_cmd.gpio2_31",			AM33XX_PIN_OUTPUT},
	{"mmc0_clk.gpio2_30",			AM33XX_PIN_INPUT},
	{"mmc0_dat1.gpio2_28",			AM33XX_PIN_INPUT},
	{"mmc0_dat3.gpio2_26",			AM33XX_PIN_INPUT},

	{NULL, 0}
};

static struct pinmux_config mmc_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3", AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2", AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1", AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0", AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",   AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",   AM33XX_PIN_INPUT_PULLUP},

	{NULL, 0}
};

/* Signal set C - NAND flash on chip select 3 */
static struct pinmux_config nand_pin_mux[] = {
	/* The majority of these are specified in board-mityarm335x.c */
	{"gpmc_csn3.gpmc_csn3",			AM33XX_PULL_DISA},
	{NULL, 0}
};

/* Ethernet one RGMII 1 */
static struct pinmux_config enet_pin_mux[] = {
	{"mii1_txen.rgmii1_tctl",		AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.rgmii1_rctl",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.rgmii1_td3",		AM33XX_PIN_OUTPUT},
	{"mii1_txd2.rgmii1_td2",		AM33XX_PIN_OUTPUT},
	{"mii1_txd1.rgmii1_td1",		AM33XX_PIN_OUTPUT},
	{"mii1_txd0.rgmii1_td0",		AM33XX_PIN_OUTPUT},
	{"mii1_txclk.rgmii1_tclk",		AM33XX_PIN_OUTPUT},
	{"mii1_rxclk.rgmii1_rclk",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.rgmii1_rd3",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.rgmii1_rd2",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.rgmii1_rd1",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.rgmii1_rd0",		AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data",			AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk",			AM33XX_PIN_OUTPUT_PULLUP},

	/* Nothing for phy_power or mdint */

	{NULL, 0}
};

/* USB Interfaces */
static struct pinmux_config usb_pin_mux[] = {
	/**
	 * No mux33xx.c specification for usb aside from usbdrvvbus
	 * (within section A)
	 */
	{"usb0_drvvbus.usb0_drvvbus",	AM33XX_PIN_OUTPUT},
	{"usb1_drvvbus.usb1_drvvbus",	AM33XX_PIN_OUTPUT},
	{NULL, 0}
};

/* UART Interface */
static struct pinmux_config uart_pin_mux[] = {
	/* UART0 */
	{"uart0_txd.uart0_txd",			AM33XX_PULL_ENBL},
	{"uart0_rxd.uart0_rxd",			AM33XX_PIN_INPUT_PULLUP},

	{NULL, 0}
};

/* SPI Interface */
static struct pinmux_config spi_pin_mux[] = {
	/* SPI1 - This is mostly specified within board-mityarm335x.c */

	{"xdma_event_intr0.spi1_cs1",		AM33XX_PULL_DISA},

	{NULL, 0}
};

static struct omap_musb_board_data board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	.mode           = MUSB_OTG,
	.power			= 500,
	.instances		= 1,
};

/**
 * Fixup for the Vitesse 8601 PHY on the MityARM335x dev kit
 * We need to adjust the recv clock skew to recenter the data eye
 */
static int am335x_vsc8601_phy_fixup(struct phy_device *phydev)
{
	unsigned int val;

	pr_info("am335x_vsc8601_phy_fixup %x here addr = %d\n",
		phydev->phy_id, phydev->addr);

	/* skew control is in extended register set */
	if (phy_write(phydev,  MII_EXTPAGE, 1) < 0) {
		pr_err("Error enabling extended PHY regs\n");
		return 1;
	}
	/* read the skew */
	val = phy_read(phydev, RGMII_SKEW);
	if (val < 0) {
		pr_err("Error reading RGMII skew reg\n");
		return val;
	}
	val &= 0x0FFF; /* clear skew values */
	val |= 0x3000; /* 0 Tx skew, 2.0ns Rx skew */
	if (phy_write(phydev, RGMII_SKEW, val) < 0) {
		pr_err("failed to write RGMII_SKEW\n");
		return 1;
	}
	/* disable the extended page access */
	if (phy_write(phydev, MII_EXTPAGE, 0) < 0) {
		pr_err("Error disabling extended PHY regs\n");
		return 1;
	}
	return 0;
}

/* NAND - initialized below in board init section */
static struct mtd_partition mityarm335x_test_nand_partitions[] = {
	{
		.name           = "MyNAND",
		.offset         = 0,
		.size           = MTDPART_SIZ_FULL,
	},
};

/* NOR - initialized below in board init section */
static struct mtd_partition mityarm335x_test_nor_partitions[] = {
	{
		.name           = "MyNOR",
		.offset         = 0,
		.size           = MTDPART_SIZ_FULL,
	},
};

static const struct flash_platform_data mityarm335x_spi_flash_test = {
	.name      = "spi_flash_test",
	.parts     = mityarm335x_test_nor_partitions,
	.nr_parts  = ARRAY_SIZE(mityarm335x_test_nor_partitions),
	.type      = "m25p64-nonjedec",
};

static const struct omap2_mcspi_device_config spi1_ctlr_data_test = {
	.turbo_mode = 0,
	.d0_is_mosi = 1,
};

static struct spi_board_info mityarm335x_spi1_slave_info[] = {
	{
		.modalias		= "m25p80",
		.platform_data		= &mityarm335x_spi_flash_test,
		.controller_data	= (void *)&spi1_ctlr_data_test,
		.irq			= -1,
		.max_speed_hz		= 30000000,
		.bus_num		= 2,
		.chip_select		= 1,
		.mode			= SPI_MODE_3,
	},
};

/* Second NAND chip... */
static struct resource gpmc_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device gpmc_nand_device = {
	.name		= "omap2-nand",
	.id		= 1,
	.num_resources	= 1,
	.resource	= &gpmc_nand_resource,
};

static struct gpmc_timings am335x_nand_timings = {

	.sync_clk = 0,

	.cs_on = 0,
	.cs_rd_off = 44,
	.cs_wr_off = 44,

	.adv_on = 6,
	.adv_rd_off = 34,
	.adv_wr_off = 44,

	.we_off = 40,
	.oe_off = 54,

	.access = 64,
	.rd_cycle = 82,
	.wr_cycle = 82,

	.wr_access = 40,
	.wr_data_mux_bus = 0,
};

static struct omap_nand_platform_data board_nand_data = {
	.gpmc_t		= &am335x_nand_timings,
};

/* ADC; analog inputs */

/*
 * Pin mux for analog inputs; the driver is a modified version
 * of the touch screen...
 */
static struct pinmux_config adc_pin_mux[] = {
	{"ain0.ain0",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain1.ain1",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain2.ain2",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain3.ain3",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain4.ain4",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain5.ain5",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain6.ain6",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain7.ain7",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefp.vrefp",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefn.vrefn",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{NULL, 0},
};

static struct resource adc_resources[]  = {
	[0] = {
		.start  = AM33XX_TSC_BASE,
		.end    = AM33XX_TSC_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = AM33XX_IRQ_ADC_GEN,
		.end    = AM33XX_IRQ_ADC_GEN,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device adc_device = {
	.name   = "ain",
	.id     = -1,
	.dev    = {},
	.num_resources  = ARRAY_SIZE(adc_resources),
	.resource       = adc_resources,
};

static struct omap2_hsmmc_info mmc_info[] __initdata = {
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_32_33 | MMC_VDD_33_34,
	},
	{}
};

static void mmc_init(void)
{
	setup_pin_mux(mmc_pin_mux);
	omap2_hsmmc_init(mmc_info);
}

static void adc_init(void)
{
	int err;

	setup_pin_mux(adc_pin_mux);
	err = platform_device_register(&adc_device);
	if (err)
		pr_err("failed to register adc device\n");
}

static void mityarm335x_loopback_test_init(void)
{
	/* Establish loopback GPIO test sets */
	setup_pin_mux(sig_setA_loopback_pin_mux);
	setup_pin_mux(sig_setB_loopback_pin_mux);
}

static int omap2_nand_gpmc_retime(
	struct omap_nand_platform_data *gpmc_nand_data)
{
	struct gpmc_timings t;
	int err;

	if (!gpmc_nand_data->gpmc_t)
		return 0;

	memset(&t, 0, sizeof(t));
	t.sync_clk = gpmc_nand_data->gpmc_t->sync_clk;
	t.cs_on = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->cs_on);
	t.adv_on = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->adv_on);

	/* Read */
	t.adv_rd_off = gpmc_round_ns_to_ticks(
				gpmc_nand_data->gpmc_t->adv_rd_off);
	t.oe_on  = t.adv_on;
	t.access = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->access);
	t.oe_off = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->oe_off);
	t.cs_rd_off = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->cs_rd_off);
	t.rd_cycle  = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->rd_cycle);

	/* Write */
	t.adv_wr_off = gpmc_round_ns_to_ticks(
				gpmc_nand_data->gpmc_t->adv_wr_off);
	t.we_on  = t.oe_on;
	if (cpu_is_omap34xx()) {
		t.wr_data_mux_bus =	gpmc_round_ns_to_ticks(
				gpmc_nand_data->gpmc_t->wr_data_mux_bus);
		t.wr_access = gpmc_round_ns_to_ticks(
				gpmc_nand_data->gpmc_t->wr_access);
	}
	t.we_off = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->we_off);
	t.cs_wr_off = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->cs_wr_off);
	t.wr_cycle  = gpmc_round_ns_to_ticks(gpmc_nand_data->gpmc_t->wr_cycle);

	/* Configure GPMC */
	if (gpmc_nand_data->devsize == NAND_BUSWIDTH_16)
		gpmc_cs_configure(gpmc_nand_data->cs, GPMC_CONFIG_DEV_SIZE, 1);
	else
		gpmc_cs_configure(gpmc_nand_data->cs, GPMC_CONFIG_DEV_SIZE, 0);
	gpmc_cs_configure(gpmc_nand_data->cs,
			GPMC_CONFIG_DEV_TYPE, GPMC_DEVICETYPE_NAND);
	err = gpmc_cs_set_timings(gpmc_nand_data->cs, &t);
	if (err)
		return err;

	return 0;
}

static void mityarm335x_test_nand_init(void)
{
	struct omap_nand_platform_data *gpmc_nand_data;
	int cs = 3;
	int err = 0;
	struct device *dev;

	/* Establish NAND flash on chip select 3 */
	setup_pin_mux(nand_pin_mux);

	board_nand_data.cs		= cs;
	board_nand_data.parts		= mityarm335x_test_nand_partitions;
	board_nand_data.nr_parts	=
		ARRAY_SIZE(mityarm335x_test_nand_partitions);
	board_nand_data.devsize		= 0;

	board_nand_data.ecc_opt = OMAP_ECC_HAMMING_CODE_DEFAULT;
	board_nand_data.gpmc_irq = OMAP_GPMC_IRQ_BASE + cs;

	board_nand_data.ecc_opt		= OMAP_ECC_BCH8_CODE_HW;
	board_nand_data.xfer_type	= NAND_OMAP_PREFETCH_POLLED;

	gpmc_nand_data = &board_nand_data;
	err = 0;
	dev = &gpmc_nand_device.dev;

	gpmc_nand_device.dev.platform_data = gpmc_nand_data;

	err = gpmc_cs_request(gpmc_nand_data->cs, NAND_IO_SIZE,
				&gpmc_nand_data->phys_base);
	if (err < 0) {
		dev_err(dev, "Cannot request GPMC CS\n");
		return;
	}

	 /* Set timings in GPMC */
	err = omap2_nand_gpmc_retime(gpmc_nand_data);
	if (err < 0) {
		dev_err(dev, "Unable to set gpmc timings: %d\n", err);
		return;
	}

	/* Enable RD PIN Monitoring Reg */
	if (gpmc_nand_data->dev_ready)
		gpmc_cs_configure(gpmc_nand_data->cs, GPMC_CONFIG_RDY_BSY, 1);

	err = platform_device_register(&gpmc_nand_device);
	if (err < 0) {
		dev_err(dev, "Unable to register NAND device\n");
		goto out_free_cs;
	}

	return;

out_free_cs:
	gpmc_cs_free(gpmc_nand_data->cs);

	return;
}

static void mityarm335x_test_nor_init(void)
{
	/* Set up NOR partitions */
	setup_pin_mux(spi_pin_mux);
	spi_register_board_info(mityarm335x_spi1_slave_info,
				ARRAY_SIZE(mityarm335x_spi1_slave_info));
}

static void mityarm335x_test_communications(void)
{
	/* Establish pin muxes for Ethernet and UART8 */
	setup_pin_mux(enet_pin_mux);
	setup_pin_mux(uart_pin_mux);

	am33xx_cpsw_init(AM33XX_CPSW_MODE_RGMII, "0:01", "0:00");

	/* Register PHY for enet */
	phy_register_fixup_for_uid(VSC8601_PHY_ID,
				VSC8601_PHY_MASK,
				am335x_vsc8601_phy_fixup);
}

static void mityarm335x_test_usb(void)
{
	/* Set up USB pins */
	setup_pin_mux(usb_pin_mux);

	/* Initialize musb using board_data */
	usb_musb_init(&board_data);
}

static void mityarm335x_test_analog(void)
{
	/* Set up analog pins */
	adc_init();
}

static void mityarm335x_test_mmc(void)
{
	/* Set up MMC pins */
	mmc_init();
}

/* Call test setup methods for the entire baseboard */
static __init void baseboard_setup(void)
{
	mityarm335x_loopback_test_init();
	mityarm335x_test_nand_init();
	mityarm335x_test_nor_init();
	mityarm335x_test_communications();
	mityarm335x_test_usb();
	mityarm335x_test_analog();
/* TODO: MMC Support? Enable MMC support here: */
#if 0
	mityarm335x_test_mmc();
#endif
}

static __init int baseboard_init(void)
{
	pr_info("%s [%s]...\n", __func__, BASEBOARD_NAME);

	/* Call major baseboard setup utility function */
	baseboard_setup();

	return 0;
}
arch_initcall_sync(baseboard_init);
