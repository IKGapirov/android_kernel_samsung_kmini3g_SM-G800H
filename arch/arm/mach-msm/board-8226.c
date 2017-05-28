/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/export.h>
#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <linux/persistent_ram.h>
#include <linux/memblock.h>
#endif
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "pm.h"
#include "modem_notifier.h"

#ifdef CONFIG_PROC_AVC
#include <linux/proc_avc.h>
#endif

#if defined(CONFIG_SEC_MILLET_PROJECT) || defined(CONFIG_SEC_MATISSE_PROJECT) || defined(CONFIG_MACH_S3VE3G_EUR) || defined (CONFIG_MACH_VICTOR3GDSDTV_LTN) || \
    defined(CONFIG_SEC_AFYON_PROJECT) || defined(CONFIG_SEC_VICTOR_PROJECT) || defined(CONFIG_SEC_BERLUTI_PROJECT) || \
    defined(CONFIG_SEC_GNOTE_PROJECT) || defined(CONFIG_SEC_ATLANTIC_PROJECT) || defined(CONFIG_SEC_DEGAS_PROJECT) || \
	defined(CONFIG_SEC_T10_PROJECT) || defined(CONFIG_SEC_T8_PROJECT) || defined(CONFIG_SEC_MEGA2_PROJECT) || defined(CONFIG_SEC_MS01_PROJECT)
#include <mach/msm8x26-thermistor.h>
#endif

#ifdef CONFIG_SENSORS_SSP
extern int poweroff_charging;
static struct regulator *vsensor_2p85, *vsensor_1p8;
static int __init sensor_hub_init(void)
{
	int ret;

	if(poweroff_charging)
		return 0;

	vsensor_2p85 = regulator_get(NULL, "8226_l19");
	if (IS_ERR(vsensor_2p85))
		pr_err("[SSP] could not get 8226_l19, %ld\n",
			PTR_ERR(vsensor_2p85));

	vsensor_1p8 = regulator_get(NULL, "8226_lvs1");
	if (IS_ERR(vsensor_1p8))
		pr_err("[SSP] could not get 8226_lvs1, %ld\n",
			PTR_ERR(vsensor_1p8));

	ret = regulator_enable(vsensor_2p85);
	if (ret)
		pr_err("[SSP] %s: error enabling regulator 2p85\n", __func__);

	ret = regulator_enable(vsensor_1p8);
	if (ret)
		pr_err("[SSP] %s: error enabling regulator 1p8\n", __func__);

	pr_info("[SSP] %s: power on\n", __func__);
	return 0;
}
#endif /* CONFIG_SENSORS_SSP */

static struct memtype_reserve msm8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct persistent_ram_descriptor desc = {
        .name = "ram_console",
};

static struct persistent_ram ram = {
        .descs = &desc,
        .num_descs = 1,
};

void __init ram_console_debug_reserve(unsigned long ram_console_size)
{
        int ret;

        ram.start = memblock_end_of_DRAM() - ram_console_size;
        ram.size = ram_console_size;
        ram.descs->size = ram_console_size;
        INIT_LIST_HEAD(&ram.node);

        ret = persistent_ram_early_init(&ram);
        if (ret) {
                pr_err("%s:ram console persistent_ram_early_init failed\n",__func__);
                goto fail;
        }

        return;

fail:
        pr_err("Failed to reserve memory block for ram console\n");
}

static struct resource ram_console_resources[] = {
        {
                .flags = IORESOURCE_MEM,
        },
};

static struct platform_device ram_console_device = {
        .name           = "ram_console",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(ram_console_resources),
        .resource       = ram_console_resources,
};

void __init ram_console_debug_init(void)
{
        int err;
        err = platform_device_register(&ram_console_device);
        if (err)
                pr_err("%s: ram console registration failed (%d)!\n",
                        __func__, err);
}
#endif

static int msm8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),

	{}
};

static struct reserve_info msm8226_reserve_info __initdata = {
	.memtype_reserve_table = msm8226_reserve_table,
	.paddr_to_memtype = msm8226_paddr_to_memtype,
};

static void __init msm8226_early_memory(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8226_reserve_table);
}

static void __init msm8226_reserve(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8226_reserve_table);
	msm_reserve();
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	ram_console_debug_reserve(SZ_1M *2);
#endif
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8226_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8226_rumi_clock_init_data);
	else
		msm_clock_init(&msm8226_clock_init_data);
	tsens_tm_init_driver();
#if defined(CONFIG_SEC_MILLET_PROJECT) || defined(CONFIG_SEC_MATISSE_PROJECT) || defined(CONFIG_MACH_S3VE3G_EUR)  || defined (CONFIG_MACH_VICTOR3GDSDTV_LTN)  || \
    defined(CONFIG_SEC_AFYON_PROJECT) || defined(CONFIG_SEC_VICTOR_PROJECT) || defined(CONFIG_SEC_BERLUTI_PROJECT) || \
    defined(CONFIG_SEC_HESTIA_PROJECT) || defined(CONFIG_SEC_GNOTE_PROJECT) || defined(CONFIG_SEC_ATLANTIC_PROJECT) || \
	defined(CONFIG_SEC_DEGAS_PROJECT) || defined(CONFIG_SEC_T10_PROJECT) || defined(CONFIG_SEC_T8_PROJECT) || defined(CONFIG_SEC_MEGA2_PROJECT) || \
	defined(CONFIG_SEC_MS01_PROJECT)
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
#endif
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	ram_console_debug_init();
#endif
	msm_thermal_device_init();
}
struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static void samsung_sys_class_init(void)
{
	pr_info("samsung sys class init.\n");

	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return;
	}

	pr_info("samsung sys class end.\n");
};

#if defined(CONFIG_BATTERY_SAMSUNG)
#if defined(CONFIG_SEC_MILLET_PROJECT) || defined(CONFIG_SEC_MATISSE_PROJECT) ||defined(CONFIG_SEC_BERLUTI_PROJECT) || \
	defined(CONFIG_SEC_VICTOR_PROJECT) || defined(CONFIG_SEC_FRESCONEO_PROJECT) || defined(CONFIG_SEC_AFYON_PROJECT) || \
	defined(CONFIG_SEC_S3VE_PROJECT) || defined(CONFIG_SEC_ATLANTIC_PROJECT) || defined(CONFIG_SEC_VICTOR_PROJECT) || \
	defined(CONFIG_SEC_DEGAS_PROJECT) || defined(CONFIG_SEC_HESTIA_PROJECT) || defined(CONFIG_SEC_MEGA2_PROJECT) || \
	defined(CONFIG_SEC_GNOTE_PROJECT) || defined(CONFIG_SEC_T10_PROJECT) || defined(CONFIG_SEC_T8_PROJECT) || \
	defined(CONFIG_SEC_VASTA_PROJECT) || defined(CONFIG_SEC_VICTOR3GDSDTV_PROJECT)
/* Dummy init function for models that use QUALCOMM PMIC PM8226 charger*/
void __init samsung_init_battery(void)
{
	pr_err("%s: Battery init dummy, using QUALCOMM PM8226 internal BMS \n", __func__);
};
#else
extern void __init samsung_init_battery(void);
#endif
#endif
#if defined(CONFIG_MACH_AFYONLTE_TMO) || defined(CONFIG_MACH_AFYONLTE_CAN)
extern void __init board_tsp_init(void);
#endif
void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif

#ifdef CONFIG_PROC_AVC
	sec_avc_log_init();
#endif

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	board_dt_populate(adata);
	samsung_sys_class_init();
	msm8226_add_drivers();
#if defined(CONFIG_BATTERY_SAMSUNG)
	samsung_init_battery();
#endif
#ifdef CONFIG_SENSORS_SSP
	sensor_hub_init();
#endif
}

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.init_very_early = msm8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END